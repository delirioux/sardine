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
 *     synth part's sound loaded when its layer's changed, the cards into the tracks, the LEDs, the settings saved
 *     while stopped, the screen.
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
static void sa_sound(void *ctx, const sa_event *e)
{
    (void)ctx;
    uint32_t p = e->part % SA_PARTS;
    if (e->on)
        trk_note_on(&trk[p], e->note, e->vel);
    else
        trk_note_off(&trk[p], e->note);
    /* MIDI out: the synth parts on channels 1..3, the kit on 10 (its notes are General MIDI's) */
    uint32_t ch = p == SA_PART_DRUMS ? 9u : p, st = (e->on ? 0x90u : 0x80u) | ch, cin = st >> 4;
    midi_out_event(cin | st << 8 | (uint32_t)e->note << 16 | (uint32_t)(e->on ? e->vel : 0u) << 24);
}

static void sa_audio_pre(uint32_t n) { sa_process(&sa_g, (int)n); }

static void sa_audio_post(int32_t *out, uint32_t n)
{
    (void)out;
    (void)n;
}

/* ---- the settings: one record in Felucca's first project object; saved only while stopped (the flash is the code's:
 * XIP, writing it while playing would stall the audio) ---- */
#define SA_REC_MAGIC 0x31524153u                             /* "SAR1" */
typedef struct { uint32_t magic; uint16_t size, ver; sa_settings s; } sa_record;
static uint32_t sa_save_ms;
static bool sa_dirty;
static void sa_settings_load(void)
{
#if FELUCCA_FLASH
    static sa_record r;                                      /* (static: off the stack) */
    memset(&r, 0, sizeof r);
    int n = st_load(OBJ_PROJECT0, &r, sizeof r);
    if (n >= (int)(sizeof r - sizeof r.s) && r.magic == SA_REC_MAGIC) {
        sa_settings s;
        sa_defaults(&s);                                     /* (a shorter, older record: the new fields their defaults) */
        uint32_t k = r.size < sizeof s ? r.size : sizeof s;
        memcpy(&s, &r.s, k);
        sa_settings_fix(&s);
        sa_g.s = s;
    }
#endif
}
static void sa_settings_save(void)
{
#if FELUCCA_FLASH
    static sa_record r;
    memset(&r, 0, sizeof r);
    r.magic = SA_REC_MAGIC;
    r.size = (uint16_t)sizeof r.s;
    r.ver = 1;
    r.s = sa_g.s;
    st_save(OBJ_PROJECT0, &r, sizeof r);
#endif
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
    if (snd->fm6)
        sa_fm6_load(&trk[part], snd->fm6);
    sa_base[part].bright = snd->bright >= 0 ? trk[part].p[P_E0 + snd->bright] : 0;
    sa_base[part].fenv = trk[part].p[P_ED_FLT];
    sa_base[part].dist = snd->dist >= 0 ? snd->dist : trk[part].p[P_DIST];
    fm1_irq_on();
}

static void sa_apply(void)
{
    const sa_settings *s = &sa_g.s;
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
    for (int p = 0; p < SA_SYNTHS; ++p) sig = sv_hashb(sig, &sa_loaded[p], sizeof sa_loaded[p]);
    if (sig != sa_applied_sig || !sig) {
        fm1_irq_off();
        for (int l = 0; l < SA_LAYERS; ++l) {
            const sa_code *c = &s->code[l];
            if (c->type == SA_T_SYNTH && c->part < SA_SYNTHS && sa_loaded[c->part] == c->sound)
                sas_synth(&trk[c->part], sas_of(c->sound), &sa_base[c->part], &s->live[l], s);
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
    sa_settings_load();
    sa_g.s.palette = (uint8_t)(settings.palette % NPALETTES);
    sa_edit_reset(&sa_g);
    sa_set_sound(&sa_g, sa_sound, 0);
    sv_init(&sa_u, &sa_g);
    sa_apply();
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

/* the LEDs: the page's button; REC while something waits for it (blinking once armed); FX while the layer open is
 * muted; PLAY green while playing; the keys held, else the chord's tones dim */
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
    led_put(nl, panel.btn[B_REC], pend && (!sa_g.drop_armed || (fm1_ms / 125u) % 2u));
    if ((sa_u.page == SV_PG_LAYER || sa_u.page == SV_PG_ARP) && sa_u.layer >= 0 && sa_g.s.live[sa_u.layer].mute)
        led_put(nl, panel.btn[B_FX], 1);
    for (k = 0; k < NB; k++)
        led_put(nd, panel.btn[k], 1);
    uint32_t lit = 0;
    if (sa_g.playing)
        for (k = 0; k < 27u; k++)
            for (int t = 0; t < 3; ++t)
                if ((SV_KEY_LO + k) % 12u == sa_g.chord_pc[t]) lit |= 1u << k;
    for (k = 0; k < 27u; k++) {
        led_put(nl, 14u + k, (int)((fm1_in.notes >> k) & 1u));
        led_put(nd, 14u + k, (int)((lit >> k) & 1u) || !((fm1_in.notes >> k) & 1u));
    }
    if (sa_g.playing)
        led_clear(nd, panel.btn[B_PLAY]);
    for (c = 0; c < FM1_NCOL; c++)
        nd[c] = settings_leds == LEDS_DIM || settings_leds == LEDS_DIM_LO ? (uint8_t)(nd[c] & ~nl[c]) : 0u;
    for (k = 0; k < 27u; k++)
        if ((lit >> k) & 1u && !((fm1_in.notes >> k) & 1u))
            led_put(nd, 14u + k, 1);
    if (sa_g.playing)
        nl[LED_PLAY_GREEN >> 3] |= (uint8_t)(1u << (LED_PLAY_GREEN & 7u));
    fm1_led_dim_level(settings_leds == LEDS_DIM_LO);
    for (c = 0; c < FM1_NCOL; c++) {
        fm1_led_dim[c] = nd[c];
        fm1_led_breath[c] = 0;
        fm1_led[c] = nl[c];
    }
}

/* a frame: the settings into the tracks, the LEDs, the screen; the settings saved a while after the last change,
 * stopped */
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
    sa_apply();
    if (sa_dirty && !sa_g.playing && now - sa_save_ms > 2000u) {
        sa_settings_save();
        sa_dirty = false;
    }
    song.playing = sa_g.playing;
    sa_leds();
    sa_u.batt = (int)batt_shown();
    sv_tick(&sa_u, dt > 100u ? 100 : (int)dt);
    if (first) {
        sv_draw_all(&sa_u);
        first = 0;
    } else {
        sv_draw(&sa_u);
    }
}
