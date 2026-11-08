/* SPDX-License-Identifier: GPL-3.0-only */
/* SARDINE on the FM-1: the glue between Sardine (the layer engine and its screen) and Felucca's firmware (the engines,
 * voices and effects, the panel, the screen driver, USB MIDI, the flash). Included by felucca.c after Felucca's own
 * sources when FELUCCA_SARDINE is 1; the layer engine (sa_layer.c) and the maths (tuna_libm.c, from Tuna) are their
 * own translation units, linked beside felucca.o (tools/build.py). Shaped as Sill's glue.
 *
 * Who runs what:
 *   the audio ISR (audio.c audio_block -> sa_audio_pre): the layers in time, their notes straight into Felucca's
 *     tracks (trk_note_on / off) and out as USB MIDI;
 *   the main loop (main.c -> sa_frame, sa_input): the panel into the controls (the audio held off while one acts), a
 *     synth part's sound loaded when its layer's changed, the cards into the tracks, the LEDs, the sets read and
 *     written (written while stopped), the screen.
 * Tracks: 1..3 the synth layers' parts, 4 the kit (DRUM engine). */
#include "sa_layer.h"
#include "sa_sounds.h"
#define SV_LOCK() fm1_irq_off()                    /* (the screen's previews share the engine's buffers) */
#define SV_UNLOCK() fm1_irq_on()
#include "sa_ui.h"
#include "sa_ui.c"
#include "sa_version.h"

static sa_groove sa_g;
static sa_ui sa_u;

/* what the parts were last set to */
static int16_t sa_loaded[SA_SYNTHS] = {-1, -1, -1};    /* the sound loaded on each synth part */
static sas_base sa_base[SA_SYNTHS];
static uint32_t sa_applied_sig;
static uint8_t sa_drums_ready;

/* ---- the audio ISR --------------------------------------------------------------------------------------------- */
/* each synth part's sound as it renders (fx.c mix_part): every 4th sample, the last 256 (23 ms); and when it last
 * sounded */
#define SA_SCOPE 256u
static int16_t sa_scope[SA_SYNTHS + 1][SA_SCOPE];            /* (+1: the whole mix, SA_SCOPE_MIX, or (0.8.47) on HOME's
                                                              * VIEW the drums' part: never both on one screen) */
static uint16_t sa_scope_w[SA_SYNTHS + 1];
static uint32_t sa_scope_ms[SA_SYNTHS + 1];
static volatile uint8_t sa_scope_drums;                      /* (VIEW open: the last ring the drums') */
static void sa_scope_take(uint32_t part, const int32_t *b, uint32_t n)
{
    if (part == SA_SYNTHS && !sa_scope_drums) return;        /* (the drums' part: only for VIEW) */
    if (part == SA_SYNTHS + 1u) {                            /* (the mix: only when not) */
        if (sa_scope_drums) return;
        part = SA_SYNTHS;
    }
    if (part > SA_SYNTHS) return;
    uint32_t w = sa_scope_w[part];
    for (uint32_t i = 0; i < n; i += 4u)
        sa_scope[part][w++ & (SA_SCOPE - 1u)] = (int16_t)clamp(b[i] >> 2, -32767, 32767);
    sa_scope_w[part] = (uint16_t)w;
    sa_scope_ms[part] = fm1_ms;
}
/* the open synth layer's wave for its screen: from a rising zero crossing, scaled to its own peak */
/* (0.8.47) HOME's VIEW: each part's scope (LEAD CHORD BASS, the drums together), from a rising zero crossing, the
 * longer the lower (BASS, DRUMS); quiet small (scaled against a floor, not its own peak) */
static void sa_scopes(void)
{
    static const uint8_t ST[4] = {2, 2, 3, 4};
    for (uint32_t p = 0; p < 4u; ++p) {
        sa_u.scope_on[p] = 0;
        if (fm1_ms - sa_scope_ms[p] > 120u) continue;
        int16_t s[SA_SCOPE];
        uint32_t w = sa_scope_w[p], st = ST[p], at = 0;
        for (uint32_t i = 0; i < SA_SCOPE; ++i) s[i] = sa_scope[p][(w + i) & (SA_SCOPE - 1u)];
        for (uint32_t i = 1; i + 54u * st < SA_SCOPE; ++i)
            if (s[i - 1] < 0 && s[i] >= 0) { at = i; break; }
        int pk = 3000;
        for (uint32_t i = 0; i < 54u; ++i) { int a = s[at + i * st] < 0 ? -s[at + i * st] : s[at + i * st]; if (a > pk) pk = a; }
        for (uint32_t i = 0; i < 54u; ++i) sa_u.scopes[p][i] = (int8_t)(s[at + i * st] * 18 / pk);
        sa_u.scope_on[p] = 1;
    }
}
static void sa_wave(void)
{
    sa_u.wave_n = 0;
    sa_scope_drums = sa_u.page == SV_PG_MAIN && sa_u.home2 == 3;
    if (sa_scope_drums) { sa_scopes(); return; }
    bool mix = sa_u.page == SV_PG_MIXER && sa_u.mixmute < 3;    /* (the MIXER: the whole mix, longer) */
    if (!mix && ((sa_u.page != SV_PG_LAYER && sa_u.page != SV_PG_EDIT) || sa_u.view < SV_V_LEAD ||
                 sa_u.view > SV_V_BASS))
        return;
    uint32_t p = mix ? SA_SYNTHS : sa_g.s.code[sa_u.layer].part % SA_SYNTHS;
    uint32_t st = mix ? 2u : sa_u.view == SV_V_BASS ? 2u : 1u;   /* (the bass: twice as long) */
    if (fm1_ms - sa_scope_ms[p] > 120u) return;                 /* (silent) */
    int16_t s[SA_SCOPE];
    uint32_t w = sa_scope_w[p];
    for (uint32_t i = 0; i < SA_SCOPE; ++i) s[i] = sa_scope[p][(w + i) & (SA_SCOPE - 1u)];
    uint32_t at = 0;
    for (uint32_t i = 1; i + 96u * st < SA_SCOPE; ++i)
        if (s[i - 1] < 0 && s[i] >= 0) { at = i; break; }
    int pk = 1;
    for (uint32_t i = 0; i < 96u; ++i) { int a = s[at + i * st] < 0 ? -s[at + i * st] : s[at + i * st]; if (a > pk) pk = a; }
    if (pk < 8) return;
    if (mix && pk < 2048) pk = 2048;                          /* (the mix: quiet stays small) */
    for (uint32_t i = 0; i < 96u; ++i) sa_u.wave[i] = (int8_t)(s[at + i * st] * 120 / pk);
    sa_u.wave_n = 96;
}
/* (0.8.35) an in-key kit's kick on the chord's root, each kick (about E1 .. D#2): it follows the progression. The
 * row's variation's own tuning under it (sa_apply keeps it) */
static int8_t sa_kick_semi0;
static int sa_kick_off(int pc)
{
    int k = ((pc - 4) % 12 + 12) % 12;
    return k > 6 ? k - 12 : k;
}
static void sa_sound(void *ctx, const sa_event *e)
{
    (void)ctx;
    uint32_t p = e->part % SA_PARTS;
    if (e->on && p == SA_PART_DRUMS && e->note == 36 && SA_KIT_INKEY(sa_g.s.kit))
        drum_lane_ov[SA_PART_DRUMS][0].semi = (int8_t)sv_clampi(sa_kick_semi0 + sa_kick_off(sa_g.chord_pc[0]), -12, 12);
    if (e->on)
        trk_note_on(&trk[p], e->note, e->vel);
    else
        trk_note_off(&trk[p], e->note);
    /* MIDI out: each layer on its channel (MENU, MIDI: LEAD 1, CHORD 2, BASS 3, DRUMS 10 out of the box; OFF: nothing),
     * the kit's notes General MIDI's (PERC's own 82..87 as their GM ones) */
    uint32_t l = p == SA_PART_DRUMS ? 3u : p % 3u, ch = sa_u.midi_ch[l], note = e->note;
    if (!ch) return;
    if (l == 3u && note >= 82u && note <= 87u) {
        static const uint8_t GM[6] = {54, 60, 76, 56, 69, 80};   /* TAMB BONGO WOOD COWBELL CABASA ZAP */
        note = GM[note - 82u];
    }
    uint32_t st = (e->on ? 0x90u : 0x80u) | (ch - 1u), cin = st >> 4;
    midi_out_event(cin | st << 8 | note << 16 | (uint32_t)(e->on ? e->vel : 0u) << 24);
    if (e->on && e->vel) {                                   /* the MIDI OUT page's log */
        uint32_t i = sa_u.ml_n % SV_MLOG;
        sa_u.ml_layer[i] = (uint8_t)l;
        sa_u.ml_ch[i] = (uint8_t)ch;
        sa_u.ml_note[i] = (uint8_t)note;
        sa_u.ml_vel[i] = e->vel;
        sa_u.ml_n++;
    }
}

/* (0.8.29) MIDI OUT's controllers, on each layer's channel, as the layer sounds: CC7 its volume (LEVEL, mute, SOLO,
 * a STYLE MIX's fade; DRUMS the MIXER's DRUMS), CC74 a synth layer's cutoff (CUTOFF, FILTER, SWEEP, WOBBLE, the fills'
 * dip, 3D), CC10 its pan (PAN, 3D). Sent when one changes, at most every 10 ms, from the main loop; all again when a
 * host connects or a channel changes. The pump is not sent (a DAW's sidechain does that) */
static uint8_t sa_cc_last[4][3], sa_cc_ch[4];
static uint32_t sa_cc_ms, sa_cc_link;
static void sa_midi_cc(void)
{
    static const uint8_t CC[3] = {7, 74, 10};
    if (fm1_ms - sa_cc_ms < 10u)
        return;
    sa_cc_ms = fm1_ms;
    if (midi_out_link() != sa_cc_link) {                     /* (a host anew: everything again) */
        sa_cc_link = midi_out_link();
        memset(sa_cc_last, 0xFF, sizeof sa_cc_last);
    }
    const sa_settings *s = &sa_g.s;
    for (int l = 0; l < 4; ++l) {
        uint8_t ch = sa_u.midi_ch[l];
        if (ch != sa_cc_ch[l]) {
            sa_cc_ch[l] = ch;
            memset(sa_cc_last[l], 0xFF, sizeof sa_cc_last[l]);
        }
        if (!ch || ch > 16)
            continue;
        int v[3] = {-1, -1, -1};
        bool solo_out = sa_g.solo && !((sa_g.solo >> l) & 1u);
        if (l < 3) {
            const sa_code *c = &s->code[l];
            if (c->type != SA_T_SYNTH || c->part >= SA_SYNTHS)
                continue;
            int p = c->part;
            float gn = sa_g.mx_state ? sa_g.mx_gain[l] : 1.0f;
            v[0] = s->live[l].mute || solo_out ? 0 : (int)((float)(s->live[l].level * 127 / 100) * gn + 0.5f);
            const sas_sound *snd = sas_of(c->sound);
            if (snd->bright >= 0 && sa_loaded[p] == c->sound) {
                int base = sa_base[p].bright < 24 ? 24 : sa_base[p].bright;
                v[1] = trk[p].p[P_E0 + snd->bright] * 127 / base;
            }
            v[2] = trk[p].p[P_PAN] + 64;
        } else {
            v[0] = s->dmute || solo_out ? 0 : s->dvol * 127 / 100;
        }
        for (int k = 0; k < 3; ++k) {
            if (v[k] < 0)
                continue;
            uint8_t x = (uint8_t)(v[k] > 127 ? 127 : v[k]);
            if (x == sa_cc_last[l][k])
                continue;
            if (midi_out_room() < 16u)                       /* (the notes first: tried again next time) */
                return;
            fm1_irq_off();
            int ok = midi_out_try(0x0Bu | (0xB0u | (ch - 1u)) << 8 | (uint32_t)CC[k] << 16 | (uint32_t)x << 24);
            fm1_irq_on();
            if (ok)
                sa_cc_last[l][k] = x;
        }
    }
}

/* (1.0.2) MIDI clock. CLOCK INT: Sardine's own, sent out over USB (START as it starts, a clock each tick, 24 to a
 * beat, STOP as it stops). CLOCK EXT: it follows the clock coming in (USB or TRS; seq.c events_block hands over the
 * realtime bytes): START plays from the 1 on the next clock, CONTINUE as START (Sardine keeps no song position),
 * STOP stops; each clock a tick; the tempo from the last 24 (a beat), on the screen too; a second without a clock
 * while playing stops it (a cable pulled). Nothing goes out on EXT: a DAW's thru would come back as its own clock */
static uint32_t sa_clk_ms[25], sa_clk_n, sa_clk_last;   /* (25: the last 24 pulses' spans, a beat) */
static void sa_midi_rt(uint32_t status, uint32_t ms)
{
    if (!sa_u.clock_ext) return;
    if (status == 0xF8u) {
        sa_clk_ms[sa_clk_n % 25u] = ms;
        if (++sa_clk_n > 25u) {
            uint32_t dt = ms - sa_clk_ms[sa_clk_n % 25u];       /* (24 pulses ago: a beat) */
            if (dt >= 240u && dt <= 1200u) {                    /* (250 .. 50 BPM; a gap or a burst left out) */
                sa_g.ext_tf = sa_g.sr * (float)dt / 24000.0f;
                sa_g.s.bpm = (uint8_t)sv_clampi((int)((60000u + dt / 2u) / dt), 60, 200);
            }
        }
        sa_clk_last = ms ? ms : 1u;
        if (sa_g.playing && sa_g.ext_credit < 1000) sa_g.ext_credit++;
    } else if (status == 0xFAu || status == 0xFBu) {           /* START, CONTINUE: from the 1 */
        if (sa_g.playing) sa_play(&sa_g, false);
        sa_play(&sa_g, true);
    } else if (status == 0xFCu) {
        sa_play(&sa_g, false);
    }
}
static void sa_clock_out(uint32_t st) { midi_out_event(0x0Fu | st << 8); }
static void sa_audio_pre(uint32_t n)
{
    static bool was;
    static uint32_t sent;
    bool ext = sa_u.clock_ext != 0;
    if (sa_g.ext != ext) {                                   /* (CLOCK changed: from the next pulse, or its own) */
        sa_g.ext = ext;
        sa_g.ext_credit = 0;
        sa_clk_n = 0;
        sa_clk_last = 0;
    }
    if (ext && sa_g.playing && sa_clk_last && fm1_ms - sa_clk_last > 1000u) {
        sa_play(&sa_g, false);                               /* (the clock gone: stopped) */
        sa_clk_n = 0;
        sa_clk_last = 0;
    }
    sa_u.clock_in = ext && sa_clk_last && fm1_ms - sa_clk_last < 1000u;
    if (!ext && sa_g.playing && !was) {                      /* INT: START, then the clocks as it ticks */
        sa_clock_out(0xFAu);
        sent = sa_g.tick_n;
    }
    sa_process(&sa_g, (int)n);
    sas_block(&sa_g);                                        /* the pump */
    if (!ext && sa_g.playing)
        for (; sent != sa_g.tick_n; ++sent) sa_clock_out(0xF8u);
    if (!ext && !sa_g.playing && was) sa_clock_out(0xFCu);
    was = sa_g.playing;
}

/* (0.8.38) FADE: the whole mix by the fade's gain, eased across the block */
static void sa_audio_post(int32_t *out, uint32_t n)
{
    static float gain = 1.0f;
    float to = sa_g.fade_gain;
    if (gain >= 0.9999f && to >= 0.9999f) {
        gain = 1.0f;
        return;
    }
    float step = n ? (to - gain) / (float)n : 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        gain += step;
        int32_t q = (int32_t)(gain * 32768.0f);
        out[2u * i] = (int32_t)(((int64_t)out[2u * i] * q) >> 15);
        out[2u * i + 1u] = (int32_t)(((int64_t)out[2u * i + 1u] * q) >> 15);
    }
    gain = to;
}

/* ---- the flash: the preferences (the look, a count of the starts: each start's fresh groove) in Felucca's first
 * project object; the eight sets and LAST (the groove as it was last left, stopped), three to an object, in the other
 * three. Written only while stopped (the flash is
 * the code's: XIP, writing it while playing would stall the audio); read at any time (storage_hw.c reads in short
 * windows, the audio keeps up) ---- */
#define SA_PREF_MAGIC 0x31504153u                            /* "SAP1" */
#define SA_SET_MAGIC 0x31534153u                             /* "SAS1" */
#define SA_REC_MAGIC 0x32524153u                             /* "SAR2": 0.2..0.5's groove, moved into set 1 once */
typedef struct { uint32_t magic, boots; uint8_t look, pad[3]; sv_song song; uint8_t midi_ch[4], midi_ok, snap, clock, play; } sa_prefs;   /* (song, then MIDI: appended) */
#define SA_PREFS_OLD 12u                                     /* (0.7's: no song) */
typedef struct { uint32_t magic; uint16_t size, ver; sa_settings s; } sa_record;
#define SA_SET_PER 3u                                        /* sets to an object */
#define SA_SET_STRIDE 1280u                                  /* the room a set has (sa_settings may grow into it) */
#define SA_SET_LEN (SA_SET_PER * SA_SET_STRIDE)
#define SA_SET_OBJ(k) (OBJ_PROJECT0 + 1u + (uint32_t)(k) / SA_SET_PER)
#define SA_SET_OFF(k) ((uint32_t)(k) % SA_SET_PER * SA_SET_STRIDE)
_Static_assert(sizeof(sa_record) <= SA_SET_STRIDE && SA_SET_LEN <= ST_PAYLOAD_MAX, "the sets' room in the flash");
_Static_assert(OBJ_PROJECT0 + 1u + (SV_SLOTS - 1u) / SA_SET_PER < OBJ_PROJECT0 + 4u, "the sets in the project objects");
#define SA_LAST_MS 3000u                                     /* LAST: kept this long after the last change, stopped */
static uint32_t sa_last_sig;                                 /* the groove LAST holds (0: not written this start) */
static sa_prefs sa_pref;
static uint8_t sa_pref_look;                                 /* the look as last written */
static sa_settings sa_next;                                  /* a groove armed for the bar line (a set, a fresh one) */
static sa_record sa_wait;                                    /* a set stored while playing, written once stopped */
static int sa_wait_slot = -1;
static bool sa_wait_clear;
static sa_pool sa_pl;
static uint32_t sa_save_ms;
static bool sa_dirty;

#if FELUCCA_FLASH
/* st_buf as it is into copy `to` of obj, sequence seq (st_save_to's steps, without its reload of the current copy into
 * st_buf: the caller has read it and changed one set in it) */
static int sa_st_commit(uint32_t obj, uint32_t len, uint32_t to, uint32_t seq)
{
    uint32_t base = st_sector(obj, to), off;
    st_hdr_t h, chk;
    int rc;
    if ((rc = st_erase(base)) != 0)
        return rc;
    for (off = 0; off < len; off += 256u) {
        uint32_t n = len - off > 256u ? 256u : len - off;
        if ((rc = st_prog(base + ST_PAYLOAD_OFF + off, st_buf + off, n)) != 0)
            return rc;
    }
    h.magic = ST_MAGIC;
    h.type = (uint16_t)obj;
    h.slot = (uint16_t)to;
    h.seq = seq;
    h.len = len;
    h.crc = st_crc32(st_buf, len);
    h.rsv[0] = h.rsv[1] = 0xFFFFFFFFu;
    h.hcrc = st_crc32(&h, sizeof h - 4u);
    if ((rc = st_prog(base, &h, sizeof h)) != 0)
        return rc;
    if (st_head(obj, to, &chk) || memcmp(&chk, &h, sizeof h) || st_body(obj, to, &chk))
        return -7;
    return 0;
}
#endif

/* set k from the flash into *s (an older, shorter record: the new fields their defaults); false: empty */
static bool sa_set_read(int k, sa_settings *s)
{
#if FELUCCA_FLASH
    st_hdr_t h;
    uint32_t off = SA_SET_OFF(k);
    sa_record r;
    if (!flash_ok || st_current(SA_SET_OBJ(k), &h) < 0 || h.len < off + 8u)
        return false;
    memcpy(&r, st_buf + off, 8u);
    if (r.magic != SA_SET_MAGIC || r.size < 16u || r.size > SA_SET_STRIDE - 8u || h.len < off + 8u + r.size)
        return false;
    sa_defaults(s);
    memcpy(s, st_buf + off + 8u, r.size < sizeof *s ? r.size : sizeof *s);
    sas_fix(s);
    return true;
#else
    (void)k;
    (void)s;
    return false;
#endif
}

/* set k into the flash (s 0: cleared); stopped only */
static bool sa_set_write(int k, const sa_settings *s)
{
#if FELUCCA_FLASH
    st_hdr_t h;
    uint32_t obj = SA_SET_OBJ(k), off = SA_SET_OFF(k);
    int cur = flash_ok ? st_current(obj, &h) : -1;           /* (its payload, the other sets, now in st_buf) */
    if (!flash_ok)
        return false;
    if (cur < 0) {
        memset(st_buf, 0, SA_SET_LEN);
        h.seq = 0;
        cur = 1;                                             /* (the first write: copy A) */
    } else if (h.len != SA_SET_LEN) {                        /* another firmware's project there (Felucca's, Sill's
                                                              * slot): its sequence kept, so the sets win over it */
        memset(st_buf, 0, SA_SET_LEN);
    }
    memset(st_buf + off, 0, SA_SET_STRIDE);
    if (s) {
        sa_record r;
        r.magic = SA_SET_MAGIC;
        r.size = (uint16_t)sizeof r.s;
        r.ver = 1;
        r.s = *s;
        memcpy(st_buf + off, &r, sizeof r);
    }
    return sa_st_commit(obj, SA_SET_LEN, cur == 0 ? 1u : 0u, h.seq + 1u) == 0;
#else
    (void)k;
    (void)s;
    return false;
#endif
}

/* what set k holds, for SETS */
static void sa_set_info_of(int k, const sa_settings *s)
{
    sv_set_info *si = &sa_u.sets[k];
    memset(si, 0, sizeof *si);
    if (!s)
        return;
    si->used = 1;
    si->bpm = s->bpm;
    si->key = s->harm.key;
    si->scale = s->harm.scale;
    si->kit = s->kit;
    for (int l = 0; l < SA_LAYERS && si->nsyn < SA_SYNTHS; ++l)
        if (s->code[l].type == SA_T_SYNTH)
            si->snd[si->nsyn++] = s->code[l].sound;
}
static void sa_sets_scan(void)
{
    for (int k = 0; k < SV_SLOTS; ++k)
        sa_set_info_of(k, sa_set_read(k, &sa_next) ? &sa_next : 0);
}

/* (1.0.4) A/B and EVOLVE, kept over power-off: bit 7 kept at all (0: a pad byte before), A/B in 0..1, EVOLVE 2..3 */
static uint8_t sa_play_bits(void)
{
    return (uint8_t)(0x80u | (sa_g.s.abauto & 3u) | (sa_g.s.evolve & 3u) << 2);
}
static void sa_prefs_save(void)
{
#if FELUCCA_FLASH
    sa_pref.magic = SA_PREF_MAGIC;
    sa_pref.look = sa_g.s.look;
    if (sa_u.g) {                                            /* (the screen up: its SONG and MIDI channels; at boot,
                                                              * before it, the ones kept stay) */
        sa_pref.song = sa_u.song;
        memcpy(sa_pref.midi_ch, sa_u.midi_ch, sizeof sa_pref.midi_ch);
        sa_pref.snap = sa_g.snap;                            /* (0.8.37: was a pad byte, 0: OFF) */
        sa_pref.clock = sa_u.clock_ext;                      /* (1.0.2: was a pad byte, 0: INT) */
        sa_pref.play = sa_play_bits();                       /* (1.0.4: A/B and EVOLVE over power-off; was a pad) */
        sa_pref.midi_ok = 2;                                 /* (2: 0.8.6 .. 0.8.20 kept 1 with every channel OFF,
                                                              * saved at boot before the screen was up: not read) */
    }
    if (flash_ok && st_save(OBJ_PROJECT0, &sa_pref, sizeof sa_pref) == 0)
        sa_pref_look = sa_pref.look;
#endif
}
/* the preferences; once, 0.5's groove (the one record in the first project object) moved into set 1 */
static void sa_prefs_load(void)
{
#if FELUCCA_FLASH
    memset(&sa_pref, 0, sizeof sa_pref);
    if (!flash_ok)
        return;
    int n = st_load(OBJ_PROJECT0, &sa_wait, sizeof sa_wait);
    if (n >= (int)SA_PREFS_OLD && sa_wait.magic == SA_PREF_MAGIC) {
        memcpy(&sa_pref, &sa_wait, (uint32_t)n < sizeof sa_pref ? (uint32_t)n : sizeof sa_pref);
    } else if (n >= (int)(sizeof sa_wait - sizeof sa_wait.s) && sa_wait.magic == SA_REC_MAGIC &&
               !sa_set_read(0, &sa_next)) {
        uint32_t k = sa_wait.size < sizeof sa_next ? sa_wait.size : sizeof sa_next;
        sa_defaults(&sa_next);
        memcpy(&sa_next, &sa_wait.s, k);
        sas_fix(&sa_next);
        sa_pref.look = sa_next.look;
        sa_set_write(0, &sa_next);
    }
    sa_pref_look = sa_pref.look;
#endif
}

/* SETS' action (REC there), in the main loop */
/* (0.8.42) a fresh groove keeps which layers are MANUAL (their lines are the new groove's, but nothing automatic on
 * them); (1.0.2) and how it plays by itself: A/B, EVOLVE, PHRASE, INTRO and ARC, set once, kept till turned off. A
 * loaded set has its own */
static void sa_keep_hand(sa_settings *n)
{
    for (int l = 0; l < SA_LAYERS; ++l) n->live[l].hand = sa_g.s.live[l].hand;
    n->abauto = sa_g.s.abauto;
    n->evolve = sa_g.s.evolve;
    n->phrase = sa_g.s.phrase;
    n->intro = sa_g.s.intro;
    n->arc = sa_g.s.arc;
}
static void sa_set_do(int act, int k)
{
    char m[32];
    if (act == SV_SA_PART) {                                 /* (0.8.32) SEQ held on HOME: the part resting, new */
        fm1_irq_off();
        int n = sa_part_new(&sa_g);
        bool b = !sa_g.part_b, coming = sa_part_next(&sa_g, 0) > 0;
        fm1_irq_on();
        sv_cpy(m, n ? (b ? "NEW B" : "NEW A") : "NOTHING TO RENEW", sizeof m);
        if (n && !coming) sv_cat(m, ": CUE IT", sizeof m);
        sv_msg(&sa_u, m);
        sa_u.changed = true;
        return;
    }
    if (act != SV_SA_STORE && act != SV_SA_CLEAR) {          /* (a STYLE MIX playing: over before sa_next is written) */
        fm1_irq_off();
        sa_mix_end(&sa_g);
        fm1_irq_on();
    }
    if (act != SV_SA_STYLE && act != SV_SA_LIVE && act != SV_SA_MIX) k = k < 0 ? 0 : k >= SV_SLOTS ? SV_SLOTS - 1 : k;
    else k = 0;
    sv_cpy(m, "SET ", sizeof m);
    m[4] = (char)('1' + k);
    m[5] = 0;
    if (k == SV_LAST) {
        if (act == SV_SA_STORE || act == SV_SA_CLEAR) { sv_msg(&sa_u, "LAST KEEPS ITSELF"); return; }
        sv_cpy(m, "LAST", sizeof m);
    }
    switch (act) {
    case SV_SA_STYLE: {                                      /* a fresh groove in a STYLE, INTRO and ARC kept */
        int k = sa_u.req_slot;
        sas_style_apply(&sa_next, k, sa_pref.boots * 7919u + fm1_ms * 31u + sa_u.frame, &sa_pl);
        sa_keep_hand(&sa_next);
        sa_next.intro = sa_g.s.intro;
        sa_next.arc = sa_g.s.arc;
        fm1_irq_off();
        sa_g.set_keep = true;                                /* (1.0.3: kept as it comes in, not as asked) */
        sa_set_arm(&sa_g, &sa_next);
        fm1_irq_on();
        sv_cpy(m, SAS_STYLE[k % SAS_NSTYLES].name, sizeof m);
        sv_cat(m, sa_g.playing ? ": ON THE 1" : " IN", sizeof m);
        break;
    }
    case SV_SA_MIX: {                                        /* STYLE MIX: a fresh groove in a STYLE, mixed into */
        int k = sa_u.req_slot;
        sas_style_apply(&sa_next, k, sa_pref.boots * 7919u + fm1_ms * 31u + sa_u.frame, &sa_pl);
        sa_keep_hand(&sa_next);
        sa_next.intro = 0;
        sa_next.arc = sa_g.s.arc;
        fm1_irq_off();
        sa_mix(&sa_g, &sa_next, sv_mix_keep_mask(sa_u.mix_keep), sv_mix_out_bars(sa_u.mix_out),
               sv_mix_in_bars(sa_u.mix_in));
        fm1_irq_on();
        sv_cpy(m, "MIX TO ", sizeof m);
        sv_cat(m, SAS_STYLE[k % SAS_NSTYLES].name, sizeof m);
        break;
    }
    case SV_SA_LIVE: {                                       /* LIVE: a fresh groove in a STYLE, only its START in */
        int k = sa_u.req_slot, st = sa_u.live_start;
        sas_style_apply(&sa_next, k, sa_pref.boots * 7919u + fm1_ms * 31u + sa_u.frame, &sa_pl);
        sa_next.intro = 0;                                   /* (built by hand, or by BUILD) */
        sa_next.arc = sa_g.s.arc;
        if (st == SA_START_PAD) {                            /* PAD: CHORD a soft pad, its filter low */
            sa_code *c = &sa_next.code[SA_L_CHORD];
            int pad = sas_find("SOFT PAD"), wash = sas_pat(SA_T_SYNTH, "WASH");
            if (sas_of(c->sound)->role != SAS_R_PAD && pad >= 0) c->sound = (uint8_t)pad;
            if (wash >= 0) c->pat = (uint8_t)wash, c->own = 0;
            sa_next.live[SA_L_CHORD].cut = 45;
            sas_fix(&sa_next);
        }
        sa_keep_hand(&sa_next);
        sa_next.intro = 0;                                   /* (LIVE: no INTRO, whatever was kept) */
        fm1_irq_off();
        sa_g.build_cue = (uint8_t)st;
        sa_g.set_keep = true;
        sa_set_arm(&sa_g, &sa_next);
        fm1_irq_on();
        sv_cpy(m, "LIVE: ", sizeof m);
        sv_cat(m, SAS_STYLE[k % SAS_NSTYLES].name, sizeof m);
        sv_cat(m, sa_g.playing ? ", ON THE 1" : ", PLAY", sizeof m);
        break;
    }
    case SV_SA_LOAD:
    case SV_SA_NEW:
        if (act == SV_SA_NEW) {
            sa_random(&sa_next, sa_pref.boots * 7919u + fm1_ms * 31u + sa_u.frame, &sa_pl);
            sas_fix(&sa_next);
            sv_cpy(m, "A NEW GROOVE", sizeof m);
            sa_keep_hand(&sa_next);
        } else if (k == sa_wait_slot) {
            if (sa_wait_clear) { sv_msg(&sa_u, "THAT SET IS EMPTY"); return; }
            sa_next = sa_wait.s;
        } else if (!sa_set_read(k, &sa_next)) {
            sv_msg(&sa_u, "THAT SET IS EMPTY");
            return;
        }
        fm1_irq_off();
        sa_g.set_keep = act == SV_SA_NEW;                    /* (a new groove: kept as it comes in; a set: its own) */
        sa_set_arm(&sa_g, &sa_next);
        fm1_irq_on();
        sv_cat(m, sa_g.playing ? ": ON THE 1" : " IN", sizeof m);
        break;
    case SV_SA_STORE:
    case SV_SA_CLEAR:
        if (sa_g.playing) {                                  /* kept now, written once stopped */
            if (sa_wait_slot >= 0 && sa_wait_slot != k) {
                sv_msg(&sa_u, "STOP FIRST: A SET WAITS");
                return;
            }
            sa_wait_clear = act == SV_SA_CLEAR;
            fm1_irq_off();
            sa_wait.s = sa_g.s;
            fm1_irq_on();
            sa_wait_slot = k;
            sa_set_info_of(k, sa_wait_clear ? 0 : &sa_wait.s);
            sa_u.sets[k].wait = (uint8_t)(sa_wait_clear ? 2 : 1);
            sv_cat(m, ": WHEN STOPPED", sizeof m);
            break;
        }
        fm1_irq_off();
        sa_next = sa_g.s;
        fm1_irq_on();
        if (k == sa_wait_slot) sa_wait_slot = -1;
        if (!sa_set_write(k, act == SV_SA_CLEAR ? 0 : &sa_next)) {
            sv_msg(&sa_u, "THE FLASH FAILED");
            return;
        }
        sa_set_info_of(k, act == SV_SA_CLEAR ? 0 : &sa_next);
        sv_cat(m, act == SV_SA_CLEAR ? " CLEARED" : " STORED", sizeof m);
        break;
    default:
        return;
    }
    sv_msg(&sa_u, m);
}

/* ---- the sounds into the tracks (main loop) ---------------------------------------------------------------------- */
static void sa_apply_own(int16_t *p, const preset_t *pr)
{
    static const uint8_t FX_DEF[4] = {0, 24, 28, 36};
    for (int i = 0; i < 8; i++)
        p[P_E0 + i] = pr->e[i];
    p[P_ATK] = pr->env[0];
    p[P_DEC] = pr->env[1];
    p[P_SUS] = pr->env[2];
    p[P_REL] = pr->env[3];
    p[P_ED_FLT] = pr->fenv;
    p[P_VOICE] = pr->mono ? V_LEGATO : V_POLY;
    for (int i = 0; i < 4; i++)
        p[P_DIST + i] = (int16_t)(pr->fx[i] ? pr->fx[i] - 1 : FX_DEF[i]);
}
static void sa_fm6_load(track_t *t, const uint8_t *packed)
{
    uint32_t tr = (uint32_t)(t - trk) % NTRK;
    uint8_t v[FP_SIZE + 1u];
    fm6_unpack(packed, v);
    fm6_set_patch(tr, v);
    t->p[P_E7] = FM6_OWN;
    fm6_slot[tr] = FM6_OWN;
    fm6_own_ok &= (uint8_t)~(1u << tr);
}
/* a synth part's sound: the engine, its preset, Sardine's own over it; what CUTOFF, ENV and DRIVE move from */
static void sa_load_synth(uint32_t part, int sound)
{
    const sas_sound *snd = sas_of(sound);
    undo_depth++;                                            /* (no undo copy of Sardine's loads) */
    set_engine_of(&trk[part], snd->engine);
    apply_preset_to(&trk[part], snd->preset);
    undo_depth--;
    fm1_irq_off();
    if (snd->own)
        sa_apply_own(trk[part].p, snd->own);
    if (snd->engine == SAS_ANALOG) {                         /* (0.7.1) the engine presets detuned 20..30 cents beat
                                                              * sourly in a chord: down to 12 (DTN, E1) */
        static const char *const TAME[] = {"RAVE", "STRINGS", "SOFT PAD"};
        for (unsigned k = 0; k < sizeof TAME / sizeof TAME[0]; ++k)
            if (sound == sas_find(TAME[k])) trk[part].p[P_E0 + 1] = 12;
    }
    if (snd->fm6)
        sa_fm6_load(&trk[part], snd->fm6);
    int vib = snd->vib;                                      /* (0.8.23) a lead's vibrato: in after a third of a second */
    static const char *const VIB1[] = {"FLUTE", "WHISTLE", "AIR LEAD", "VOX LEAD"};
    for (unsigned k = 0; !vib && k < sizeof VIB1 / sizeof VIB1[0]; ++k)
        if (sound == sas_find(VIB1[k])) vib = 1;
    if (vib) {
        trk[part].p[P_LRATE] = 89;                           /* 5.4 Hz */
        trk[part].p[P_LWAVE] = 0;                            /* SIN */
        trk[part].p[P_LFADE] = 80;
        trk[part].p[P_LD_PIT] = (int16_t)vib;
    }
    if (snd->mo) {                                           /* (0.8.24) a RAVE sound's movement */
        const sas_motion *m = snd->mo;
        trk[part].p[P_ED_PIT] = m->epit;
        trk[part].p[P_ED_SHP] = m->eshp;
        trk[part].p[P_LSYNC] = m->lsync;
        trk[part].p[P_LWAVE] = m->lwave;
        trk[part].p[P_LD_FLT] = m->lflt;
        trk[part].p[P_LD_SHP] = m->lshp;
        trk[part].p[P_LD_AMP] = m->lamp;
        trk[part].p[P_LPHASE] = m->lamp ? 64 : 0;            /* (a gate starts open: the note's start heard) */
    }
    sa_base[part].bright = snd->bright >= 0 ? trk[part].p[P_E0 + snd->bright] : 0;
    sa_base[part].fenv = trk[part].p[P_ED_FLT];
    sa_base[part].dist = snd->dist >= 0 ? snd->dist : trk[part].p[P_DIST];
    fm1_irq_on();
}

static void sa_apply(void)
{
    static sa_settings swept;                                /* (SWEEP held: the settings with the filter closing) */
    const sa_settings *s = &sa_g.s;
    int sw = (int)(sa_g.sweep * 50.0f);                      /* (in 2 % steps: the parts set again as it moves) */
    if (sw > 0) {
        swept = sa_g.s;
        swept.filter = (uint8_t)(swept.filter * (100 - sw * 2 * 85 / 100) / 100);
        if (swept.mreso < 20 + sw * 2 * 50 / 100)                    /* (and resonant as it closes: up to 70) */
            swept.mreso = (uint8_t)(20 + sw * 2 * 50 / 100);
        s = &swept;
    }
    if (!sa_drums_ready) {
        undo_depth++;
        set_engine_of(&trk[SA_PART_DRUMS], SAS_DRUM);
        apply_preset_to(&trk[SA_PART_DRUMS], 0u);
        undo_depth--;
        sa_drums_ready = 1;
        sa_applied_sig = 0;
    }
    for (int l = 0; l < SA_LAYERS; ++l) {                    /* a synth layer's sound changed (a drop): load it */
        const sa_code *c = &s->code[l];
        if (c->type == SA_T_SYNTH && c->part < SA_SYNTHS && sa_loaded[c->part] != c->sound) {
            sa_load_synth(c->part, c->sound);
            sa_loaded[c->part] = c->sound;
            sa_applied_sig = 0;
        }
    }
    uint32_t sig = sv_hashb(2166136261u, s, sizeof *s);
    sig = sv_hashb(sig, sa_g.az_now, sizeof sa_g.az_now);
    sig = sv_hashb(sig, &sw, sizeof sw);
    sig = sv_hashb(sig, sa_g.el_now, sizeof sa_g.el_now);
    for (int p = 0; p < SA_SYNTHS; ++p) sig = sv_hashb(sig, &sa_loaded[p], sizeof sa_loaded[p]);
    if (sig != sa_applied_sig || !sig) {
        fm1_irq_off();
        for (int l = 0; l < SA_LAYERS; ++l) {
            const sa_code *c = &s->code[l];
            if (c->type == SA_T_SYNTH && c->part < SA_SYNTHS && sa_loaded[c->part] == c->sound)
            {
                sa_3d now = s->d3[c->part];                  /* (3D where it is now: moving with ORBIT RANDOM FLY) */
                now.az = sa_g.az_now[c->part];
                now.el = sa_g.el_now[c->part];
                sas_synth(&trk[c->part], sas_of(c->sound), &sa_base[c->part], &s->live[l], s, &now);
            }
        }
        {   /* each drum row's variation onto its channel(s) of the drum engine (eng_drum.c drum_lane_ov) */
            static const drum_lane_ov_t VAR[SA_DVARS] = {{0, 0, 0, 0}, {0, 0, 10, -28}, {0, 0, -8, 30}, {0, 3, 6, -6},
                                                          {0, -3, -6, 6}, {2, 0, 0, 0}, {3, 0, 0, 0}, {7, 0, 0, 0}};
            static const uint8_t LANE[SA_DROWS] = {0, 1, 2, 3, 5, 6, 7};    /* (HATS: the open hat's channel 4 too) */
            /* CLAP: hands, short and dry with a little room, never the 10's long noise tail: each one from the short
             * claps (80 55 77 66), the hiss down (kit: 2 the 80, 4 the 66, 5 the 55, 6 the 77, 7 DEEP) */
            static const drum_lane_ov_t CLAP[SA_DVARS] = {{2, 0, -6, -10}, {5, 0, -8, -30}, {6, 0, -10, 0}, {4, 3, 0, -12},
                                                           {2, -3, -10, -6}, {2, 0, 0, 0}, {6, 0, 0, 0}, {7, 0, 0, 0}};
            /* PERC: the engine's own percussion on its channel (Sardine's notes 82..87: eng_drum.c SA_PERC), never a
             * model kit's; a variation tunes, colours and lengthens it */
            static const drum_lane_ov_t PERC[SA_DVARS] = {{0, 0, 0, 0}, {0, 0, 4, -30}, {0, 0, -6, 30}, {0, 4, 8, -6},
                                                           {0, -4, -6, 6}, {0, 0, -24, 0}, {0, 2, 22, -8}, {0, -7, -12, 12}};
            for (int r = 0; r < SA_DROWS; ++r) {
                const sa_code *c = &s->code[SA_L_DRUM0 + r];
                int vv = c->type == SA_T_DRUM ? c->amode % SA_DVARS : 0;
                drum_lane_ov_t ov = r == SA_DR_CLAP ? CLAP[vv] : r == SA_DR_PERC ? PERC[vv] : VAR[vv];
                if (r == SA_DR_KICK && SA_KIT_INKEY(s->kit)) {   /* the kick on the chord's root (sa_sound, each
                                                                  * kick); till the first, the key's */
                    sa_kick_semi0 = ov.semi;
                    ov.semi = (int8_t)sv_clampi(ov.semi + sa_kick_off(sa_g.playing ? sa_g.chord_pc[0] : s->harm.key),
                                                -12, 12);
                }
                if (r == SA_DR_KICK && s->kit == SA_KIT_RUMBLE) {   /* RUMBLE: the tail long and dark */
                    ov.decay = (int8_t)sv_clampi(ov.decay + 40, -64, 64);
                    ov.tone = (int8_t)sv_clampi(ov.tone - 16, -64, 64);
                }
                drum_lane_ov[SA_PART_DRUMS][LANE[r]] = ov;
                if (r == SA_DR_HATS) drum_lane_ov[SA_PART_DRUMS][4] = ov;
            }
        }
        sas_drums(&trk[SA_PART_DRUMS], s);
        sas_song(s);
        fm1_irq_on();
        sa_applied_sig = sig ? sig : 1u;
    }
    if (s->palette != settings.palette % NPALETTES) {        /* (the menu's colour, kept as Felucca keeps its own) */
        settings.palette = s->palette;
        settings_save();
    }
}

/* ---- the power-on screen: Sardine's logo (tools/sa_logo.py) and the Felucca it runs on; main.c holds it 0.75 s ---- */
#include "sa_logo.inc"
#define SA_SPLASH_MS 750u
static void sa_splash(void)
{
    uint16_t row[2][SA_LOGO_SIZE];
    uint32_t y, x0 = (240u - SA_LOGO_SIZE) / 2u, y0 = 16u;
    char line[40] = "Sardine " SA_VERSION " on Felucca ";
    uint32_t k = sizeof "Sardine " SA_VERSION " on Felucca " - 1u;
    lcd_fill(0, 0, 240, 240, SA_LOGO_BG);
    for (y = 0; y < SA_LOGO_SIZE; y++) {
        const uint8_t *p = SA_LOGO_RLE + SA_LOGO_ROW[y];
        uint16_t *r = row[y & 1u];
        uint32_t x = 0;
        while (x < SA_LOGO_SIZE) {
            uint32_t n = (*p >> 3) + 1u, c = SA_LOGO_PAL[*p & 7u];
            p++;
            while (n-- && x < SA_LOGO_SIZE)
                r[x++] = (uint16_t)c;
        }
        lcd_blit(x0, y0 + y, SA_LOGO_SIZE, 1, r);
    }
    for (const char *v = FELUCCA_VERSION; *v && k < sizeof line - 1u; v++)
        line[k++] = *v;
    line[k] = 0;
    draw_text_line(0, y0 + SA_LOGO_SIZE + 18u, 240, &AF_S, line, 0xA535u, SA_LOGO_BG, 1);
    lcd_sync();
}

/* (0.8.46) the LED test round under the logo: the buttons in turn, then (1.0.5) the keys from both ends to the middle
 * (a key from the left and its mirror from the right together, meeting on the middle one), PLAY's green last; a short
 * tail behind; then all of them at once to the end. A dead one shows as a gap. ms: since the logo */
static void sa_led_test(uint32_t ms)
{
    static uint8_t ready;
    uint8_t nl[FM1_NCOL] = {0};
    uint32_t c, steps = 14u + 14u + 1u, step = (SA_SPLASH_MS - 150u) / steps, at = ms / step;
    if (!ready) {
        led_pos_init();
        ready = 1;
    }
    for (uint32_t id = 0; id < 42u; id++) {
        uint32_t k = id - 14u, when = id < 14u ? id : id < 41u ? 14u + (k < 26u - k ? k : 26u - k) : 28u;
        bool on = at >= steps || (when <= at && at - when < 3u);   /* (its step: this one and two behind; then all) */
        if (!on) continue;
        if (id < 41u) led_put(nl, id, 1);
        else nl[LED_PLAY_GREEN >> 3] |= (uint8_t)(1u << (LED_PLAY_GREEN & 7u));
    }
    for (c = 0; c < FM1_NCOL; c++) {
        fm1_led_dim[c] = 0;
        fm1_led_breath[c] = 0;
        fm1_led[c] = nl[c];
    }
}

/* ---- start ------------------------------------------------------------------------------------------------------ */
static void sa_fw_init(void)
{
    uint32_t i;
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = GP[i].def;
    undo_depth++;
    fm6_init();
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        track_defaults(t);
        set_engine_of(t, i == SA_PART_DRUMS ? SAS_DRUM : SAS_ANALOG);
        apply_preset_to(t, 0u);
        t->engine = t->eng_req;
        track_defaults_steps(t);                             /* (Felucca's sequencer stays empty) */
    }
    undo_depth--;
    song.sel = 0;
    song.master_q12 = 2048;
    sa_init(&sa_g, (float)FS);
    sas_pool_make(&sa_pl);
    sa_prefs_load();
    sa_pref.boots++;                                         /* each start its own groove (the count kept) */
    sa_prefs_save();
    sa_random(&sa_g.s, sa_pref.boots, &sa_pl);
    sas_fix(&sa_g.s);                                        /* (the pump by the sounds' types) */
    sa_g.s.look = sa_pref.look;
    sa_g.s.palette = (uint8_t)(settings.palette % NPALETTES);
    sa_edit_reset(&sa_g);
    sa_set_sound(&sa_g, sa_sound, 0);
    sv_init(&sa_u, &sa_g);
    sa_g.snap = sa_pref.snap == 1;                           /* SNAP as kept, OFF by default */
    sa_u.clock_ext = sa_pref.clock == 1;                     /* CLOCK as kept, INT by default */
    if (sa_pref.play & 0x80u) {                              /* (1.0.4) A/B and EVOLVE as they were at power-off */
        sa_g.s.abauto = (uint8_t)((sa_pref.play & 3u) % SA_ABAUTOS);
        sa_g.s.evolve = (uint8_t)(((sa_pref.play >> 2) & 3u) % SA_EVOLVES);
    }
    {   /* SONG as kept (none kept, or all END: sets 1..4, sv_init's) */
        bool any = false;
        for (int k = 0; k < SV_STEPS; ++k) any |= sa_pref.song.set[k] != 0;
        if (any) {
            sa_u.song = sa_pref.song;
            for (int k = 0; k < SV_STEPS; ++k) {
                if (sa_u.song.set[k] > SV_SETS) sa_u.song.set[k] = 0;
                sa_u.song.bars[k] %= SV_SONG_BARS;
            }
            sa_u.song.on = 0;                                /* (SONG starts OFF: a start plays its own groove) */
        }
    }
    /* (MIDI OUT: every start on 1 2 3 10, sv_init's, whatever was set before: a channel turned OFF comes back) */
    sa_sets_scan();
    sa_apply();
}

/* ---- SONG: the steps' sets in turn, each armed a bar before its bar line ---- */
static sa_settings sa_song_set;                              /* the step armed (kept until its bar line) */
static int sa_song_end, sa_song_next = -1;
static bool sa_song_primed;
static bool sa_song_ok(int k)
{
    int st = sa_u.song.set[k];
    return st >= 1 && st <= SV_SETS && (sa_u.sets[st - 1].used || sa_u.sets[st - 1].wait);
}
static int sa_song_after(int k)                              /* the next step with a set (round); -1: none */
{
    for (int i = 1; i <= SV_STEPS; ++i)
        if (sa_song_ok((k + i) % SV_STEPS)) return (k + i) % SV_STEPS;
    return -1;
}
static bool sa_song_arm(int k)
{
    int st = sa_u.song.set[k] - 1;
    if (st == sa_wait_slot) {
        if (sa_wait_clear) return false;
        sa_song_set = sa_wait.s;
    } else if (!sa_set_read(st, &sa_song_set)) {
        return false;
    }
    fm1_irq_off();
    sa_g.set_keep = false;                                   /* (SONG: each set its own) */
    sa_set_arm(&sa_g, &sa_song_set);
    fm1_irq_on();
    return true;
}
static void sa_song(void)
{
    if (!sa_u.song.on) {
        sa_u.song_step = -1;
        sa_song_primed = false;
        sa_song_next = -1;
        return;
    }
    if (!sa_g.playing) {                                     /* stopped: the first step's set in, ready for PLAY */
        sa_u.song_step = -1;
        sa_song_next = -1;
        if (!sa_song_primed) {
            int f = sa_song_after(SV_STEPS - 1);
            if (f >= 0 && sa_song_arm(f)) sv_msg(&sa_u, "SONG: PLAY");
            sa_song_primed = true;
        }
        return;
    }
    int bar = sa_g.bar;
    if (sa_u.song_step < 0) {                                /* SONG on while playing, or PLAY after the first */
        int f = sa_song_after(SV_STEPS - 1);
        if (f < 0) return;
        sa_u.song_step = f;
        if (sa_song_primed && bar <= 0) {
            sa_song_end = sv_song_bars(sa_u.song.bars[f]);
        } else {
            sa_song_arm(f);
            sa_song_end = bar + 1 + sv_song_bars(sa_u.song.bars[f]);
        }
        sa_song_primed = false;
        sa_song_next = -1;
        return;
    }
    if (sa_song_next < 0 && bar >= sa_song_end - 1) {       /* the next one, for the bar line */
        int n = sa_song_after(sa_u.song_step);
        if (n < 0) return;
        if (sa_u.song.set[n] != sa_u.song.set[sa_u.song_step] || n != sa_u.song_step) sa_song_arm(n);
        sa_song_next = n;
    }
    if (sa_song_next >= 0 && bar >= sa_song_end) {
        sa_u.song_step = sa_song_next;
        sa_song_end += sv_song_bars(sa_u.song.bars[sa_song_next]);
        sa_song_next = -1;
    }
}

/* MIDI in: a note on a channel Sardine sends a layer on is its own coming back (a DAW echoing what it gets): dropped */
static int sa_midi_ours(uint32_t ch)
{
    for (int k = 0; k < 4; ++k)
        if (sa_u.midi_ch[k] && sa_u.midi_ch[k] == ch + 1u) return 1;
    return 0;
}

/* ---- MOVES: the UI's held moves onto Felucca's PERFORM effects (perform.c) and the groove ---- */
static uint8_t sa_moves_on;                                  /* the moves playing */
static int sa_wash_bar = -1;                                 /* WASH let go: off on the next 1 (this bar) */
static void sa_perf(uint32_t e, int down) { fm1_irq_off(); perf_press(e, down); fm1_irq_on(); }
static void sa_moves(void)
{
    static const uint8_t PF[SV_MOVES] = {PF_RTHR, PF_ETHR, PF_TAPE, PF_N};
    uint8_t want = sa_u.moves;
    if (sa_wash_bar >= 0 && (!sa_g.playing || sa_g.bar != sa_wash_bar)) {   /* WASH: dry again on the 1 */
        sa_perf(PF_RTHR, 0);
        sa_wash_bar = -1;
        sa_moves_on &= (uint8_t)~(1u << SV_MV_WASH);
    }
    for (int mv = 0; mv < SV_MOVES; ++mv) {
        uint8_t bit = (uint8_t)(1u << mv);
        bool w = want & bit, on = sa_moves_on & bit;
        if (mv == SV_MV_WASH && w && sa_wash_bar >= 0) sa_wash_bar = -1;   /* (held again before the 1: on) */
        if (w == on || (mv == SV_MV_WASH && !w && sa_wash_bar >= 0)) continue;
        if (mv == SV_MV_WOBBLE) {
            sa_g.wobble = w;
        } else if (mv == SV_MV_WASH && !w && sa_g.playing) {
            sa_wash_bar = sa_g.bar;                         /* (held to the next 1) */
            continue;
        } else {
            sa_perf(PF[mv], w);
        }
        sa_moves_on = (uint8_t)(w ? sa_moves_on | bit : sa_moves_on & ~bit);
    }
    {   /* (0.8.49) ARC's WASH on its last bar, dry on the drop; under a WASH held by hand it leaves it be */
        static uint8_t arc_wash;
        uint8_t aw = sa_g.arr_wash && sa_g.playing;
        if (aw != arc_wash) {
            if (!(sa_moves_on & (1u << SV_MV_WASH))) sa_perf(PF_RTHR, aw);
            arc_wash = aw;
        }
    }
}

/* ---- the main loop ----------------------------------------------------------------------------------------------- */
static uint32_t sa_notes_prev;
static void sa_input(void)
{
    uint32_t released = 0, pressed = fm1_input_edges(&released), b, k;
    static const uint8_t KNOB[NE] = {SV_K_SELECT, SV_K_ALGO, SV_K_PRESETS, SV_K1, SV_K2, SV_K3, SV_K4};
    fm1_input_note_edges();
    for (b = 0; b < NB; b++) {                               /* (Felucca's B_* are Sardine's SV_B_*, in the same order) */
        uint32_t bit = 1u << panel.btn[b];
        if (pressed & bit) {
            fm1_irq_off();
            sv_button(&sa_u, (int)b, true);
            fm1_irq_on();
        }
        if (released & bit) {
            fm1_irq_off();
            sv_button(&sa_u, (int)b, false);
            fm1_irq_on();
        }
    }
    for (k = 0; k < NE; k++) {
        int32_t s = panel_enc(k);
        if (s) {
            fm1_irq_off();
            sv_knob(&sa_u, KNOB[k], s);
            fm1_irq_on();
        }
    }
    uint32_t notes = fm1_in.notes, ch = notes ^ sa_notes_prev;
    for (k = 0; ch && k < 27u; k++)
        if ((ch >> k) & 1u) {
            fm1_irq_off();
            sv_key(&sa_u, SV_KEY_LO + (int)k, (notes >> k) & 1u);
            fm1_irq_on();
        }
    sa_notes_prev = notes;
}

/* the LEDs: the page's button (FX past a layer's home page); SEQ while something waits for it (blinking once armed);
 * REC while step recording (on SETS: blinking, it does the action); on a synth layer the notes it sounds; PLAY green while playing; the keys held, else the chord's tones dim */
static void sa_leds(void)
{
    static uint8_t ready;
    uint8_t nl[FM1_NCOL] = {0}, nd[FM1_NCOL] = {0};
    uint32_t k, c;
    if (!ready) {
        led_pos_init();
        ready = 1;
    }
    int pb = sv_page_button(&sa_u);
    if (pb >= 0)
        led_put(nl, panel.btn[pb], 1);
    bool pend = sa_pending(&sa_g) > 0;
    led_put(nl, panel.btn[B_SEQ], pend && (!sa_g.drop_armed || (fm1_ms / 125u) % 2u));   /* SEQ: what waits */
    led_put(nl, panel.btn[B_REC], sa_u.rec || (sa_u.page == SV_PG_SETS && (fm1_ms / 500u) % 2u));
    led_put(nl, panel.btn[B_ENV], sa_g.fill_held && (sa_g.fill_on || (fm1_ms / 125u) % 2u));   /* (the fill: lit; waiting
                                                                                                 * for the beat: blinking) */
    led_put(nl, panel.btn[B_ARP], sa_g.brk_on && (sa_g.brk_held || (fm1_ms / 125u) % 2u));   /* BREAK (let go: blinking
                                                                                               * until the 1) */
    led_put(nl, panel.btn[B_LFO], sa_g.sweep_held);                                       /* SWEEP */
    led_put(nl, panel.btn[B_OCTDN], sa_u.oct < 0);                  /* the keys' octave moved down, or up */
    led_put(nl, panel.btn[B_OCTUP], sa_u.oct > 0);
    uint32_t efull = 0, edim = 0;
    bool editing = sv_key_leds(&sa_u, &efull, &edim);   /* DRUMS, STEPS: the keys show what they edit */
    for (k = 0; k < NB; k++)
        led_put(nd, panel.btn[k], 1);
    uint32_t lit = editing ? 0u : sv_play_dim(&sa_u);   /* DRUMS: the drum's steps dim; else the chord's tones */
    if (sa_g.playing && !editing && !lit)
        for (k = 0; k < 27u; k++)
            for (int t = 0; t < 3; ++t)
                if ((SV_KEY_LO + k) % 12u == sa_g.chord_pc[t]) lit |= 1u << k;
    uint32_t plays = editing ? 0u : sv_play_keys(&sa_u);   /* the layer open: the notes it sounds, lit */
    for (k = 0; k < 27u; k++) {
        led_put(nl, 14u + k, (int)(((fm1_in.notes | plays) >> k) & 1u));
        led_put(nd, 14u + k, (int)((lit >> k) & 1u) || !((fm1_in.notes >> k) & 1u));
    }
    if (sa_g.playing)
        led_clear(nd, panel.btn[B_PLAY]);
    for (c = 0; c < FM1_NCOL; c++)
        nd[c] = settings_leds == LEDS_DIM || settings_leds == LEDS_DIM_LO ? (uint8_t)(nd[c] & ~nl[c]) : 0u;
    for (k = 0; k < 27u; k++)
        if ((lit >> k) & 1u && !((fm1_in.notes >> k) & 1u))
            led_put(nd, 14u + k, 1);
    if (editing)                                             /* the step keys dim, the steps set full (always: they
                                                              * are the editor, not decoration) */
        for (k = 0; k < 27u; k++) {
            bool f = ((efull >> k) & 1u) || ((fm1_in.notes >> k) & 1u);
            led_clear(nd, 14u + k);                          /* (only the editing keys glow: the rest dark) */
            led_put(nl, 14u + k, f);
            if (((edim >> k) & 1u) && !f) led_put(nd, 14u + k, 1);
        }
    if (sa_g.playing && (!sa_g.fade_on || (fm1_ms / 100u) % 2u))   /* (FADE: blinking) */
        nl[LED_PLAY_GREEN >> 3] |= (uint8_t)(1u << (LED_PLAY_GREEN & 7u));
    fm1_led_dim_level(settings_leds == LEDS_DIM_LO);
    for (c = 0; c < FM1_NCOL; c++) {
        fm1_led_dim[c] = nd[c];
        fm1_led_breath[c] = 0;
        fm1_led[c] = nl[c];
    }
}

/* a frame: SETS' action, the settings into the tracks, a set stored while playing written once stopped, the look
 * kept a while after the last change (stopped), the LEDs, the screen */
static uint32_t sa_last_ms;
static void sa_frame(void)
{
    uint32_t now = fm1_ms, dt = now - sa_last_ms;
    static uint8_t first = 1;
    static uint8_t sig_seen;
    sa_last_ms = now;
    if (sa_u.changed || sig_seen != sa_g.sound_sig) {
        sa_u.changed = false;
        sig_seen = sa_g.sound_sig;
        sa_dirty = true;
        sa_save_ms = now;
    }
    if (sa_u.req_act >= 0) {
        sa_set_do(sa_u.req_act, sa_u.req_slot);
        sa_u.req_act = -1;
    }
    sa_song();
    sa_moves();
    sa_apply();
    sa_midi_cc();
    if (sa_wait_slot >= 0 && !sa_g.playing) {                /* a set stored while playing: now */
        int k = sa_wait_slot;
        sa_wait_slot = -1;
        sa_u.sets[k].wait = 0;
        if (sa_set_write(k, sa_wait_clear ? 0 : &sa_wait.s)) {
            sv_msg(&sa_u, sa_wait_clear ? "SET CLEARED" : "SET STORED");
        } else {
            sa_set_info_of(k, sa_set_read(k, &sa_next) ? &sa_next : 0);
            sv_msg(&sa_u, "THE FLASH FAILED");
        }
    }
    static bool was_playing;
    if (was_playing && !sa_g.playing) sa_save_ms = now;     /* (stopped: the tails ring out before the flash) */
    was_playing = sa_g.playing;
    if (sa_dirty && !sa_g.playing && now - sa_save_ms > SA_LAST_MS) {   /* the look; LAST, when the groove changed */
        if (sa_g.s.look != sa_pref_look || memcmp(&sa_u.song, &sa_pref.song, sizeof sa_u.song) ||
            memcmp(sa_u.midi_ch, sa_pref.midi_ch, sizeof sa_u.midi_ch) || sa_g.snap != (sa_pref.snap == 1) ||
            sa_u.clock_ext != (sa_pref.clock == 1) || sa_play_bits() != sa_pref.play)
            sa_prefs_save();
        uint32_t sig = sv_hashb(2166136261u, &sa_g.s, sizeof sa_g.s) | 1u;
        if (sig != sa_last_sig && sa_set_write(SV_LAST, &sa_g.s)) {
            sa_last_sig = sig;
            sa_set_info_of(SV_LAST, &sa_g.s);
        }
        sa_dirty = false;
    }
    song.playing = sa_g.playing;
    sa_leds();
    sa_u.batt = (int)batt_shown();
    {   /* MENU's CPU: the audio's load (audio.c), read twice a second */
        static uint32_t cpu_at;
        if (now - cpu_at >= 500u) {
            cpu_at = now;
            sa_u.cpu = (int)(song.cpu_q8 * 100u / 256u);
            sa_u.cpu_peak = (int)(audio_max_us * 100u / (HALF_FRAMES * 1000000u / FS));
            sa_u.shed = (int)shed_count;
        }
    }
    sa_wave();
    sv_tick(&sa_u, dt > 100u ? 100 : (int)dt);
    if (first) {
        sv_draw_all(&sa_u);
        first = 0;
    } else {
        sv_draw(&sa_u);
    }
}
