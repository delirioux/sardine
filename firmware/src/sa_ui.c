/* SPDX-License-Identifier: GPL-3.0-only */
/* SARDINE screen and controls (sa_ui.h; 0.7: the fixed layers, FX's pages), drawn with Felucca's gfx.c in the family's frame. Included after gfx.c (the
 * firmware's felucca.c unit through sa_fw.c, or tests/sa_ui_test.c). Needs sa_sounds.h before it.
 *
 * The previews run the layer engine's query, which shares its buffers with the audio's bar line: SV_LOCK() and
 * SV_UNLOCK() hold the audio off around them (the glue defines them; the host leaves them empty). */
#include "sa_ui.h"
#include "sa_version.h"
#include "ui_icons.h"

#ifndef SV_LOCK
#define SV_LOCK() ((void)0)
#define SV_UNLOCK() ((void)0)
#endif

#define SV_HEAD 24
#define SV_CARD_Y 28
#define SV_CARD_W 57
#define SV_CARD_H 44
#define SV_CARD_X(c) (3 + 59 * (int32_t)(c))
#define SV_PANEL_Y 76
#define SV_PANEL_H 122
#define SV_FOOT_Y 202
#define SV_FOOT_H 38
#define SV_MENU_HOLD_MS 600
static void sv_bars_txt(char *t, int n);

static int sv_clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static int sv_wrap(int v, int n) { return n > 0 ? ((v % n) + n) % n : 0; }
static void sv_cpy(char *d, const char *s, unsigned n)
{
    unsigned i = 0;
    for (; s[i] && i + 1 < n; ++i) d[i] = s[i];
    d[i] = 0;
}
static void sv_cat(char *d, const char *s, unsigned n)
{
    unsigned k = 0;
    while (d[k]) ++k;
    sv_cpy(d + k, s, n > k ? n - k : 0);
}
static void sv_fmt_int(char *d, int v)
{
    char b[12];
    int k = 0, neg = v < 0;
    unsigned uv = neg ? (unsigned)-v : (unsigned)v;
    do { b[k++] = (char)('0' + uv % 10); uv /= 10; } while (uv && k < 11);
    int j = 0;
    if (neg) d[j++] = '-';
    while (k) d[j++] = b[--k];
    d[j] = 0;
}
static uint32_t sv_hash(uint32_t h, const char *s)
{
    while (*s) h = (h ^ (uint8_t)*s++) * 16777619u;
    return h;
}
static uint32_t sv_hashb(uint32_t h, const void *p, unsigned n)
{
    const uint8_t *b = (const uint8_t *)p;
    while (n--) h = (h ^ *b++) * 16777619u;
    return h;
}
static int32_t sv_icon_cell(uint32_t *size, uint32_t id)
{
    const uint8_t *idx = *size == 24u ? AI24_IDX : *size == 16u ? AI16_IDX : AI12_IDX;
    if (id >= ICON_COUNT)
        return -1;
    if (idx[id] == 0xFFu) {
        if (*size == 12u || AI12_IDX[id] == 0xFFu)
            return -1;
        *size = 12u;
        idx = AI12_IDX;
    }
    return idx[id];
}
static int32_t sv_icon_mid(int32_t x, int32_t cy, uint32_t size, uint32_t id, uint16_t fg, uint16_t bg)
{
    uint32_t sz = size;
    int32_t k = sv_icon_cell(&sz, id), y0, y1;
    if (k < 0)
        return 0;
    if (sz == 24u) {
        uint32_t v = AI24_INK[k];
        y0 = (int32_t)(v >> 8 & 255u);
        y1 = (int32_t)(v >> 24 & 255u);
    } else {
        uint32_t v = sz == 16u ? AI16_INK[k] : AI12_INK[k];
        y0 = (int32_t)(v >> 4 & 15u);
        y1 = (int32_t)(v >> 12 & 15u) + 1;
    }
    int32_t y = cy - HALF_UP(y1 - y0) - y0;
    cv_alpha_hc(x, y, sz, sz, sz == 24u ? AI24_DATA + AI24_OFF[k] : sz == 16u ? AI16_DATA + AI16_OFF[k] :
                AI12_DATA + AI12_OFF[k], AI_HC, ramp(fg, bg));
    GFX_HOOK_TEXT(x, y + cv_oy, x + (int32_t)sz, y + cv_oy + (int32_t)sz, "icon", 4u);
    return (int32_t)sz;
}

/* ---- the cards ---------------------------------------------------------------------------------------------- */
typedef struct { char label[12], val[20]; int ratio; bool pend, off, name, dim; } sv_card;   /* name: a sound's name (cut if
                                                                                  * long); dim: greyed, not to be turned */
static void sv_card_num(sv_card *c, const char *label, int v, int lo, int hi)
{
    sv_cpy(c->label, label, sizeof c->label);
    sv_fmt_int(c->val, v);
    c->ratio = hi > lo ? (v - lo) * 1000 / (hi - lo) : -1;
    c->pend = c->off = c->name = c->dim = false;
}
static void sv_card_txt(sv_card *c, const char *label, const char *val, int ratio, bool pend)
{
    sv_cpy(c->label, label, sizeof c->label);
    sv_cpy(c->val, val, sizeof c->val);
    c->ratio = ratio;
    c->pend = pend;
    c->off = c->name = c->dim = false;
}
static void sv_card_off(sv_card *c)
{
    memset(c, 0, sizeof *c);
    c->off = true;
    c->ratio = -1;
}

static void sv_draw_card(sa_ui *u, int i, const sv_card *c)
{
    bool hot = u->hot == i && u->hot_ms > 0;
    uint16_t lc = c->dim ? T_DIM : hot || c->pend ? T_ACCENT : T_MID, vc = c->dim ? T_DIM : hot || c->pend ? T_ACCENT : T_THEME;
    uint32_t sig = sv_hash(sv_hash(2166136261u + (uint32_t)i * 7u + ux.gen * 131u + (uint32_t)hot * 3u +
                                   (uint32_t)c->pend * 5u + (uint32_t)c->off * 11u + (uint32_t)c->dim * 13u + (uint32_t)u->page * 977u,
                                   c->label),
                           c->val) + (uint32_t)(c->ratio + 1) * 31u;
    if (!u->force && sig == u->sig_card[i])
        return;
    u->sig_card[i] = sig;
    cv_begin(SV_CARD_W, SV_CARD_H, T_BG);
    cv_rrect(0, 0, SV_CARD_W, SV_CARD_H, 4, c->pend ? T_ACCENT : T_SURF, T_BG);
    if (c->pend)                                   /* waiting for REC: an outline in the second colour */
        cv_rrect(2, 2, SV_CARD_W - 4, SV_CARD_H - 4, 3, T_SURF, T_ACCENT);
    if (c->off) {
        cv_rect(SV_CARD_W / 2 - 6, SV_CARD_H / 2, 12, 1, T_DIM);
    } else {
        cv_text_fit(5, 3, &AF_S, c->label, lc, T_SURF, SV_CARD_W - 8);
        const aafont_t *vf = text_w(&AF_M, c->val) <= SV_CARD_W - 8 ? &AF_M : &AF_S;
        if (c->name) cv_free_text(5, vf == &AF_M ? 17 : 19, vf, c->val, vc, T_SURF, SV_CARD_W - 8);
        else cv_text_fit(5, vf == &AF_M ? 17 : 19, vf, c->val, vc, T_SURF, SV_CARD_W - 8);
        if (c->ratio >= 0) {
            int32_t w = SV_CARD_W - 10, r = sv_clampi(c->ratio, 0, 1000) * w / 1000;
            cv_rect(5, 37, w, 3, T_LINE);
            cv_rect(5, 37, r > 0 ? r : 1, 3, vc);
        }
    }
    cv_blit((uint32_t)SV_CARD_X(i), SV_CARD_Y);
}

/* ---- the layers ------------------------------------------------------------------------------------------------ */
static const char *const SV_VIEW_NAME[SV_VIEWS] = {"HOME", "LEAD", "CHORD", "BASS", "DRUMS"};
static int sv_slot_of_view(int v) { return v == SV_V_LEAD ? SA_L_LEAD : v == SV_V_CHORD ? SA_L_CHORD : SA_L_BASS; }
static bool sv_on_layer(const sa_ui *u) { return u->page == SV_PG_LAYER || u->page == SV_PG_EDIT; }
static bool sv_synth_view(const sa_ui *u) { return sv_on_layer(u) && u->view >= SV_V_LEAD && u->view <= SV_V_BASS; }
static bool sv_drum_view(const sa_ui *u) { return sv_on_layer(u) && u->view == SV_V_DRUMS; }
/* the code the screen edits (the copy) and what plays */
static sa_code *sv_ed(sa_ui *u) { return &u->g->ed[sv_clampi(u->layer, 0, SA_LAYERS - 1)]; }
/* (0.8.30) PAN and PAN 3D: one or the other on a layer. PAN is on when it moves or sits off the centre; PAN 3D wins
 * (a groove of before with both: its PAN greyed, PAN 3D as it was) */
static bool sv_pan3d_on(const sa_ui *u)
{
    int l = sv_clampi(u->layer, 0, SA_LAYERS - 1);
    return u->g->s.d3[sv_clampi(u->g->s.code[l].part, 0, SA_SYNTHS - 1)].on != SA_3D_OFF;
}
static bool sv_pan_on(const sa_ui *u)
{
    const sa_pan *p = &u->g->s.pan[sv_clampi(u->layer, 0, SA_LAYERS - 1)];
    return !sv_pan3d_on(u) && (p->mode != SA_PAN_FIXED || p->pos != 0);
}
static const sa_code *sv_live(const sa_ui *u) { return &u->g->s.code[sv_clampi(u->layer, 0, SA_LAYERS - 1)]; }
static const char *sv_layer_name(const sa_ui *u, int l)
{
    const sa_groove *g = u->g;
    const sa_code *c = g->ed[l].type != SA_T_NONE ? &g->ed[l] : &g->s.code[l];
    if (c->type == SA_T_NONE) return "-";
    if (c->type == SA_T_DRUM && c->amode % SA_DVARS) {   /* a drum's variation: CLAP TIGHT */
        static char nb[4][24];
        static int at;
        char *b = nb[at++ & 3];
        sv_cpy(b, sa_dv_name(c->sound), 24);
        sv_cat(b, " ", 24);
        sv_cat(b, sa_dvar_name(sa_dv_row(c->sound), c->amode % SA_DVARS), 24);
        return b;
    }
    return sas_name(c->type, c->sound);
}

/* DRUMS' rows: seven, always there (KICK SNARE CLAP HATS TOMS RIM METAL), each on a drum channel of its own */
static int sv_drum_rows(const sa_ui *u, int *rows)
{
    (void)u;
    for (int r = 0; r < SA_DROWS; ++r) rows[r] = SA_L_DRUM0 + r;
    return SA_DROWS;
}
static int sv_drow_count(const sa_ui *u) { (void)u; return SA_DROWS; }
static void sv_msg(sa_ui *u, const char *m);
/* a row's INSTR, turned: another drum of its channel, at once */
static void sv_instr_turn(sa_ui *u, int d)
{
    sa_groove *g = u->g;
    int l = u->layer;
    uint8_t list[SA_DVS];                          /* (each drum of the row, in its eight variations) */
    int n = sa_row_voices(l - SA_L_DRUM0, list), at = 0;
    for (int i = 0; i < n; ++i) if (list[i] == g->ed[l].sound) at = i * SA_DVARS + g->ed[l].amode % SA_DVARS;
    at = sv_clampi(at + d, 0, n * SA_DVARS - 1);
    g->ed[l].sound = g->s.code[l].sound = list[at / SA_DVARS];
    g->ed[l].amode = g->s.code[l].amode = (uint8_t)(at % SA_DVARS);
    g->sound_sig++;
    sv_msg(u, sv_layer_name(u, l));
}
static void sv_drum_sync(sa_ui *u)               /* the row picked: its slot */
{
    int rows[SA_LAYERS], n = sv_drum_rows(u, rows), total = sv_drow_count(u);
    u->drow = sv_clampi(u->drow, 0, total > 0 ? total - 1 : 0);
    u->layer = u->drow < n ? rows[u->drow] : -1;
}

/* a layer's pages, in FX's order */
static int sv_pages(const sa_ui *u, int *p)
{
    int n = 0;
    p[n++] = SVP_HOME;
    p[n++] = SVP_MIX;                              /* (first: VOLUME, TYPE, INSTR, MUTE) */
    p[n++] = SVP_HAND;                             /* (0.8.42) MANUAL: played by hand, nothing automatic */
    if (u->view == SV_V_DRUMS) {
        p[n++] = SVP_KIT;
        p[n++] = SVP_MOD;
        return n;
    }
    p[n++] = SVP_ARP;                              /* (every synth layer's, OFF at first) */
    p[n++] = SVP_PAN;
    p[n++] = SVP_3D;
    p[n++] = SVP_PUMP;
    p[n++] = SVP_GATE;
    p[n++] = SVP_MOD;
    return n;
}
static int sv_page_now(const sa_ui *u)
{
    int p[10], n = sv_pages(u, p);
    return p[sv_clampi(u->fx, 0, n - 1)];
}
static const char *sv_page_name(int pg)
{
    static const char *const N[] = {"HOME", "SOUND", "ARP", "PAN", "GATE", "MOD", "MIX", "KIT", "PAN 3D", "PUMP", "MANUAL"};
    return N[sv_clampi(pg, 0, 10)];
}
/* REC on a synth layer, off GATE: the steps take the cards and the panel */
static bool sv_steps_on(const sa_ui *u)
{
    return sv_synth_view(u) && u->page == SV_PG_EDIT;   /* (the keys change a pattern only in EDIT) */
}

/* the patterns ALGORITHM turns through on a layer: OFF, then its own kind (and, on LEAD, the bass lines too) */
static int sv_pat_list(const sa_ui *u, uint8_t *out)
{
    int n = 0, np = sa_synth_pats(), chord0 = np, lead0 = np;
    out[n++] = SA_PAT_OFF;
    if (u->view == SV_V_DRUMS) {
        for (int i = 0; i < sa_drum_pats(); ++i) out[n++] = (uint8_t)i;
        return n;
    }
    for (int i = 0; i < np; ++i) {
        const char *nm = sa_pat_name(SA_T_SYNTH, i);
        if (chord0 == np && !memcmp(nm, "STAB 1", 7)) chord0 = i;
        if (lead0 == np && !memcmp(nm, "TONES UP", 9)) lead0 = i;
    }
    if (u->view == SV_V_BASS) for (int i = 0; i < chord0; ++i) out[n++] = (uint8_t)i;
    if (u->view == SV_V_CHORD) for (int i = chord0; i < lead0; ++i) out[n++] = (uint8_t)i;
    if (u->view == SV_V_LEAD) {
        for (int i = lead0; i < np; ++i) out[n++] = (uint8_t)i;
        for (int i = 0; i < chord0; ++i) out[n++] = (uint8_t)i;
    }
    return n;
}

/* the sounds of a type (SAS_R_*), in the table's order */
static int sv_sounds_of(int role, uint8_t *out)
{
    int n = 0;
    for (int i = 0; i < SAS_NSOUNDS; ++i)
        if (sas_of(i)->role == role) out[n++] = (uint8_t)i;
    return n;
}

static uint16_t sv_ed_bits(sa_ui *u, int l);         /* (below: the copy's bar of a layer) */
static int sv_fills(const sa_ui *u);                 /* (below: EDIT's FILLs) */
static const char *sv_fill_name(const sa_ui *u, int f);
static int sv_now(const sa_groove *g) { return g->playing && g->tick >= 0 ? g->tick / SA_TPS : -1; }

static void sv_cards_of(sa_ui *u, sv_card *c)
{
    sa_groove *g = u->g;
    sa_settings *s = &g->s;
    char t[20];
    switch (u->page) {
    case SV_PG_MAIN:
        if (u->home2 == 1) {                       /* HOME, FX: SPACE, the whole mix's RESO and WIDTH */
            sv_card_num(&c[0], "RESO", s->mreso, 0, 100);
            sv_card_num(&c[1], "WIDTH", s->width, 0, 100);
            sv_card_off(&c[2]);
            sv_card_off(&c[3]);
            return;
        }
        if (u->home2 == 2) {                       /* HOME, FX twice: PARTS (0.8.32) */
            static const char *const AB[SA_ABAUTOS] = {"OFF", "8 BARS", "16 BARS"};
            static const char *const EV[SA_EVOLVES] = {"OFF", "LIGHT", "MORE"};
            sv_card_txt(&c[0], "PHRASE", s->phrase ? "OFF" : "ON", s->phrase ? 0 : 1000, false);
            sv_card_txt(&c[1], "A/B", AB[s->abauto % SA_ABAUTOS], s->abauto * 500, false);
            sv_card_txt(&c[2], "EVOLVE", EV[s->evolve % SA_EVOLVES], s->evolve * 500, false);
            sv_card_off(&c[3]);
            return;
        }
        sv_card_txt(&c[0], "FILTER", s->filter >= 100 ? "OPEN" : "", s->filter * 10, false);
        if (s->filter < 100) sv_fmt_int(c[0].val, s->filter);
        sv_card_num(&c[1], "FEEL", s->feel, 0, 60);
        sv_card_num(&c[2], "PUMP", s->pump, 0, 100);
        sv_card_num(&c[3], "SPACE", s->reverb, 0, 100);
        return;
    case SV_PG_KEY: {
        const sa_harm *e = &g->ed_harm, *l = &s->harm;
        sv_card_txt(&c[0], "KEY", sa_note_name(e->key), e->key * 1000 / 11, e->key != l->key);
        sv_card_txt(&c[1], "SCALE", sa_scale_name(e->scale), e->scale * 1000 / (SA_SCALES - 1), e->scale != l->scale);
        if (g->part_cue) {                         /* PART B: B's progression (0: as A) */
            int pb = g->ed_progb ? g->ed_progb - 1 : e->prog;
            sa_prog_name(pb, t, sizeof t);
            sv_card_txt(&c[2], "B CHORDS", t, pb * 1000 / (SA_PROGS - 1), g->ed_progb != s->progb);
        } else {
            sa_prog_name(e->prog, t, sizeof t);
            sv_card_txt(&c[2], "CHORDS", t, e->prog * 1000 / (SA_PROGS - 1), e->prog != l->prog);
        }
        sv_fmt_int(t, e->bars);
        sv_cat(t, e->bars == 1 ? " BAR" : " BARS", sizeof t);
        sv_card_txt(&c[3], "BARS", t, e->bars == 1 ? 0 : e->bars == 2 ? 500 : 1000, e->bars != l->bars);
        return;
    }
    case SV_PG_SETS: {
        static const char *const ACT[SV_SA_ACTS] = {"LOAD", "STORE", "NEW", "CLEAR"};
        if (u->set_slot == SV_LAST) sv_cpy(t, "LAST", sizeof t);
        else sv_fmt_int(t, u->set_slot + 1);
        sv_card_txt(&c[0], "SET", t, u->set_slot * 1000 / (SV_SLOTS - 1), false);
        sv_card_txt(&c[1], "ACTION", ACT[sv_wrap(u->set_act, SV_SA_ACTS)], -1, false);
        sv_card_off(&c[2]);
        sv_card_off(&c[3]);
        return;
    }
    case SV_PG_MIXER:
        if (u->mixmute == 6) {                     /* STYLE MIX: TO KEEP OUT IN */
            int k = sv_clampi(u->mix_pick, 0, SAS_NSTYLES - 1);
            sv_card_txt(&c[0], "TO", SAS_STYLE[k].name, k * 1000 / (SAS_NSTYLES - 1), g->mx_state != 0);
            sv_card_txt(&c[1], "KEEP", sv_mix_keep_name(u->mix_keep), u->mix_keep * 1000 / (SV_MIX_KEEPS - 1), false);
            sv_bars_txt(t, sv_mix_out_bars(u->mix_out));
            sv_card_txt(&c[2], "OUT", t, u->mix_out * 1000 / (SV_MIX_OUTS - 1), false);
            sv_bars_txt(t, sv_mix_in_bars(u->mix_in));
            sv_card_txt(&c[3], "IN", t, u->mix_in * 1000 / (SV_MIX_INS - 1), false);
            return;
        }
        if (u->mixmute == 4) {                     /* LIVE: STYLE START BUILD */
            static const char *const ST[SA_STARTS] = {"FULL", "KICK", "PAD", "EMPTY"}, *const BU[3] = {"HAND", "8 BARS",
                                                                                                  "16 BARS"};
            int k = sv_clampi(u->style_pick, 0, SAS_NSTYLES - 1);
            sv_card_txt(&c[0], "STYLE", SAS_STYLE[k].name, k * 1000 / (SAS_NSTYLES - 1), g->set_next != 0);
            sv_card_txt(&c[1], "START", ST[sv_clampi(u->live_start, 1, SA_STARTS - 1)], -1, false);
            sv_card_txt(&c[2], "BUILD", BU[sv_clampi(u->live_build, 0, 2)], u->live_build * 500, false);
            sv_card_off(&c[3]);
            return;
        }
        if (u->mixmute == 5) {                     /* SONG: STEP SET BARS ON */
            static const char *const BN[SV_SONG_BARS] = {"4 BARS", "8 BARS", "16 BARS", "32 BARS", "64 BARS"};
            int k = sv_clampi(u->song_pick, 0, SV_STEPS - 1), st = u->song.set[k];
            sv_fmt_int(t, k + 1);
            sv_card_txt(&c[0], "STEP", t, k * 1000 / (SV_STEPS - 1), false);
            if (st) { sv_cpy(t, "SET ", sizeof t); t[4] = (char)('0' + st); t[5] = 0; }
            else sv_cpy(t, "END", sizeof t);
            sv_card_txt(&c[1], "SET", t, st * 1000 / SV_SETS, false);
            sv_card_txt(&c[2], "BARS", BN[u->song.bars[k] % SV_SONG_BARS], u->song.bars[k] * 250, false);
            sv_card_txt(&c[3], "SONG", u->song.on ? "ON" : "OFF", u->song.on ? 1000 : 0, u->song.on);
            return;
        }
        if (u->mixmute == 3) {                     /* STYLE INTRO ARC */
            static const char *const IN[SA_INTROS] = {"OFF", "4 BARS", "8 BARS", "16 BARS"};
            static const char *const AR[SA_ARCS] = {"OFF", "16 BARS", "32 BARS", "64 BARS"};
            int k = sv_clampi(u->style_pick, 0, SAS_NSTYLES - 1);
            sv_card_txt(&c[0], "STYLE", SAS_STYLE[k].name, k * 1000 / (SAS_NSTYLES - 1), k != s->style || g->set_next);
            c[0].name = true;
            sv_card_txt(&c[1], "INTRO", IN[s->intro % SA_INTROS], s->intro * 1000 / (SA_INTROS - 1), false);
            sv_card_txt(&c[2], "ARC", AR[s->arc % SA_ARCS], s->arc * 1000 / (SA_ARCS - 1), false);
            sv_card_off(&c[3]);
            return;
        }
        for (int i = 0; i < 4; ++i) {
            static const char *const N[4] = {"LEAD", "CHORD", "BASS", "DRUMS"};
            bool m = i < 3 ? s->live[i].mute : s->dmute, so = (g->solo >> i) & 1u;
            int v = i < 3 ? s->live[i].level : s->dvol;
            if (u->mixmute == 2) sv_card_txt(&c[i], N[i], so ? "SOLO" : "-", -1, so);
            else if (u->mixmute) sv_card_txt(&c[i], N[i], m ? "MUTED" : "ON", -1, m);
            else sv_card_num(&c[i], N[i], v, 0, 100);
        }
        return;
    case SV_PG_MENU:
        if (u->menu2) {                            /* MIDI: each layer's channel */
            static const char *const N[4] = {"LEAD", "CHORD", "BASS", "DRUMS"};
            for (int i = 0; i < 4; ++i) {
                if (u->midi_ch[i]) {
                    char t[8] = "CH ";
                    sv_fmt_int(t + 3, u->midi_ch[i]);
                    sv_card_txt(&c[i], N[i], t, (u->midi_ch[i] - 1) * 1000 / 15, false);
                } else {
                    sv_card_txt(&c[i], N[i], "OFF", 0, true);
                }
            }
            return;
        }
        sv_card_txt(&c[0], "COLOR", UI_PALETTES[s->palette % NPALETTES].name, -1, false);
        sv_card_txt(&c[1], "LOOK", s->look ? "LINE" : "FLAT", -1, false);
        sv_card_txt(&c[2], "SNAP", u->g->snap ? "ON" : "OFF", u->g->snap ? 1000 : 0, false);   /* (0.8.37) */
        sv_card_txt(&c[3], "CLOCK", u->clock_ext ? "EXT" : "INT", -1, false);   /* (1.0.2) */
        return;
    default: break;
    }
    int l = u->layer;
    if (l < 0) {                                   /* DRUMS on + ADD */
        sv_card_txt(&c[0], "DRUM", "+ ADD", -1, false);
        for (int i = 1; i < 4; ++i) sv_card_off(&c[i]);
        return;
    }
    sa_live *v = &s->live[l];
    const sa_code *e = &g->ed[l], *lc = &g->s.code[l];
    if (u->page == SV_PG_EDIT && u->view == SV_V_DRUMS) {
        int st = sv_clampi(u->scur, 0, 15);
        bool on = e->type == SA_T_DRUM && ((sv_ed_bits(u, l) >> st) & 1u);
        sv_fmt_int(t, st + 1);
        sv_card_txt(&c[0], "STEP", t, st * 1000 / 15, false);
        sv_card_txt(&c[1], "FILL", sv_fill_name(u, u->fill), u->fill * 1000 / (sv_fills(u) - 1), u->fill > 0);
        sv_card_txt(&c[2], "HIT", on ? "ON" : "OFF", -1, false);
        sv_card_txt(&c[3], "SOLO", g->ed_solo == l + 1 ? "ON" : "OFF", g->ed_solo == l + 1 ? 1000 : 0,
                    g->ed_solo == l + 1);              /* (0.8.39) the drum alone, while editing */
        return;
    }
    if (sv_steps_on(u)) {
        sa_code o = *e;
        sa_own_make(g, &o);
        int st = sv_clampi(u->scur, 0, 15);
        bool chord = (o.chord >> st) & 1u, note = o.sn[st] != SA_NO_NOTE;
        sv_fmt_int(t, st + 1);
        sv_card_txt(&c[0], "STEP", t, st * 1000 / 15, false);
        if (chord || note) {
            sv_fmt_int(t, o.sl[st]);
            sv_cat(t, "/16", sizeof t);
            sv_card_txt(&c[1], "LENGTH", t, (o.sl[st] - 1) * 1000 / 15, false);
        } else {
            sv_card_txt(&c[1], "LENGTH", "-", -1, false);
        }
        if (chord) {
            sv_card_txt(&c[2], "NOTE", "CHORD", -1, false);
        } else if (note) {
            int nn = g->ed_harm.key + o.sn[st];
            char a[4];
            sv_cpy(t, sa_note_name(nn), sizeof t);
            sv_fmt_int(a, nn / 12 - 1);
            sv_cat(t, a, sizeof t);
            sv_card_txt(&c[2], "NOTE", t, -1, false);
        } else {
            sv_card_txt(&c[2], "NOTE", "-", -1, false);
        }
        sv_card_off(&c[3]);
        if (u->page == SV_PG_EDIT)                 /* (0.8.48) EDIT: STEP LENGTH NOTE FILL */
            sv_card_txt(&c[3], "FILL", sv_fill_name(u, u->fill), u->fill * 1000 / (sv_fills(u) - 1), u->fill > 0);
        return;
    }
    switch (sv_page_now(u)) {
    case SVP_HOME:
        if (u->view == SV_V_DRUMS) {
            sv_card_txt(&c[0], "DRUM", sa_row_name(l - SA_L_DRUM0), (l - SA_L_DRUM0) * 1000 / (SA_DROWS - 1), false);
            sv_card_txt(&c[1], "INSTR", sv_layer_name(u, l), -1, false);
            c[1].name = true;
            sv_card_num(&c[2], "VOLUME", v->level, 0, 100);
            sv_card_num(&c[3], "ACCENT", v->env, 0, 100);
        } else {
            sv_card_num(&c[0], "CUTOFF", v->cut, 0, 100);
            sv_card_num(&c[1], "RESO", v->reso, 0, 100);
            sv_card_num(&c[2], "DRIVE", v->drive, 0, 100);
            sv_card_num(&c[3], "ENV", v->env, 0, 100);
        }
        break;
    case SVP_ARP: {
        static const char *const SHORT[SA_AR_MODES] = {"OFF", "UP", "DOWN", "UP-DN", "DN-UP", "CONVRG", "DIVRG",
                                                       "RANDOM", "THUMB", "PINKY", "STRUM"};
        sv_card_txt(&c[0], "MODE", SHORT[e->amode % SA_AR_MODES], e->amode * 1000 / (SA_AR_MODES - 1),
                    e->amode != lc->amode);
        sv_card_txt(&c[1], "RATE", sa_rate_name(e->arate), e->arate * 1000 / (SA_RATES - 1), e->arate != lc->arate);
        sv_fmt_int(t, e->arange);
        sv_cat(t, " OCT", sizeof t);
        sv_card_txt(&c[2], "RANGE", t, (e->arange - 1) * 1000 / 3, e->arange != lc->arange);
        sv_card_num(&c[3], "GATE", e->agate, 10, 100);
        c[3].pend = e->agate != lc->agate;
        break;
    }
    case SVP_PAN: {
        const sa_pan *p = &s->pan[l];
        static const char *const SHORT[SA_PAN_MODES] = {"FIXED", "SWEEP", "PING", "RANDOM"};
        sv_card_txt(&c[0], "MODE", SHORT[p->mode % SA_PAN_MODES], p->mode * 1000 / (SA_PAN_MODES - 1), false);
        if (p->pos == 0) sv_cpy(t, "C", sizeof t);
        else { char a[8]; sv_cpy(t, p->pos < 0 ? "L" : "R", sizeof t); sv_fmt_int(a, p->pos < 0 ? -p->pos : p->pos);
               sv_cat(t, a, sizeof t); }
        sv_card_txt(&c[1], "PAN", t, (p->pos + 50) * 10, false);
        sv_card_num(&c[2], "WIDTH", p->width, 0, 100);
        if (p->mode == SA_PAN_SWEEP) {
            static const char *const R[SA_PRATES] = {"1/4", "1/2", "1 BAR", "2 BARS", "4 BARS"};
            sv_card_txt(&c[3], "RATE", R[p->rate % SA_PRATES], p->rate * 1000 / (SA_PRATES - 1), false);
        } else {
            sv_card_off(&c[3]);
        }
        if (sv_pan3d_on(u)) for (int k = 0; k < 4; ++k) c[k].dim = true;   /* (PAN 3D on: PAN greyed) */
        break;
    }
    case SVP_GATE: {
        const sa_gate *ge = &g->ed_gate[l], *gl = &s->gate[l];
        static const char *const SHORT[SA_GATES] = {"OFF", "16THS", "8THS", "OFFBT", "PULSE", "TRNC A", "TRNC B",
                                                    "DOTTED", "SYNCO", "BREATH", "STUTTR", "HALVES", "SPARSE"};
        sv_cpy(t, ge->own && ge->pat == 0 ? "MINE" : SHORT[ge->pat % SA_GATES], sizeof t);
        sv_card_txt(&c[0], "GATE", t, ge->own ? -1 : ge->pat * 1000 / (SA_GATES - 1),
                    ge->pat != gl->pat || ge->own != gl->own || ge->mask != gl->mask);
        sv_card_num(&c[1], "DEPTH", ge->depth, 0, 100);
        c[1].pend = ge->depth != gl->depth;
        sv_card_num(&c[2], "WIDTH", ge->width, 10, 100);
        c[2].pend = ge->width != gl->width;
        sv_card_num(&c[3], "SMOOTH", ge->smooth, 0, 100);
        c[3].pend = ge->smooth != gl->smooth;
        break;
    }
    case SVP_3D: {
        int pp = sv_clampi(lc->part, 0, SA_SYNTHS - 1);
        const sa_3d *d3 = &s->d3[pp];
        static const char *const MODE[SA_3D_MODES] = {"OFF", "FIXED", "ORBIT", "RANDOM", "FLY"};
        sv_card_txt(&c[0], "MODE", MODE[d3->on % SA_3D_MODES], d3->on * 1000 / (SA_3D_MODES - 1), false);
        int deg = d3->az * 5;
        if (d3->on == SA_3D_ORBIT || d3->on == SA_3D_FLY) {
            static const char *const R[SA_PRATES] = {"1/4", "1/2", "1 BAR", "2 BARS", "4 BARS"};
            sv_card_txt(&c[1], "RATE", R[s->d3rate[pp] % SA_PRATES], s->d3rate[pp] * 1000 / (SA_PRATES - 1), false);
        } else if (d3->on == SA_3D_RANDOM) {
            sv_card_off(&c[1]);
        } else {
            if (deg == 0) sv_cpy(t, "FRONT", sizeof t);
            else if (deg == 180) sv_cpy(t, "BACK", sizeof t);
            else { char a[8]; sv_cpy(t, deg < 180 ? "R " : "L ", sizeof t); sv_fmt_int(a, deg < 180 ? deg : 360 - deg);
                   sv_cat(t, a, sizeof t); }
            sv_card_txt(&c[1], "ANGLE", t, d3->az * 1000 / (SA_AZ_STEPS - 1), false);
        }
        if (d3->el == 0) sv_cpy(t, "LEVEL", sizeof t);
        else { char a[8]; sv_cpy(t, d3->el > 0 ? "UP " : "DOWN ", sizeof t); sv_fmt_int(a, d3->el > 0 ? d3->el : -d3->el);
               sv_cat(t, a, sizeof t); }
        sv_card_txt(&c[2], "HEIGHT", t, (d3->el + 50) * 10, false);
        sv_card_num(&c[3], "DIST", d3->dist, 0, 100);
        if (sv_pan_on(u)) for (int k = 0; k < 4; ++k) c[k].dim = true;      /* (PAN on: PAN 3D greyed) */
        break;
    }
    case SVP_PUMP: {
        const sa_pumpl *q = &s->pl[sv_clampi(lc->part, 0, SA_SYNTHS - 1)];
        sv_fmt_int(t, q->depth);
        sv_cat(t, " %", sizeof t);
        sv_card_txt(&c[0], "DEPTH", t, q->depth * 10, false);
        sv_card_num(&c[1], "RELEASE", q->rel, 0, 100);
        sv_card_txt(&c[2], "SOURCE", q->src == SA_PS_BEAT ? "BEAT" : "KICK", -1, false);
        sv_card_txt(&c[3], "PUMP", q->on ? "ON" : "OFF", -1, false);
        break;
    }
    case SVP_MOD:
        for (int i = 0; i < SA_MODS; ++i) {
            if (i == 3 && u->view == SV_V_DRUMS) {   /* (a drum's CHANCE: out of the way, beside its modifiers) */
                sv_fmt_int(t, v->cut);
                sv_cat(t, " %", sizeof t);
                sv_card_txt(&c[3], "CHANCE", t, v->cut * 10, false);
                break;
            }
            char lb[8] = "MOD 1";
            lb[4] = (char)('1' + i);
            sa_mod_name(e->mod[i], t, sizeof t);
            sv_card_txt(&c[i], lb, t, -1, e->mod[i].type != lc->mod[i].type || e->mod[i].val != lc->mod[i].val);
        }
        break;
    case SVP_MIX:                                  /* VOLUME TYPE INSTR MUTE (a drum: VOLUME, MUTE) */
        sv_card_num(&c[0], "VOLUME", v->level, 0, 100);
        if (u->view == SV_V_DRUMS) {
            sv_card_txt(&c[1], "KIT", sa_kit_name(s->kit), s->kit * 1000 / (SA_KITS - 1), false);
            sv_card_txt(&c[2], "INSTR", sv_layer_name(u, l), -1, false);
            c[2].name = true;
        } else {
            static const char *const ROLE[SAS_ROLES] = {"BASS", "KEYS", "PADS", "LEADS", "FX"};
            int role = sas_of(lc->sound)->role % SAS_ROLES;
            uint8_t list[SAS_NSOUNDS > 0 ? SAS_NSOUNDS : 1];
            int n = sv_sounds_of(role, list), at = 0;
            char a[8];
            sv_card_txt(&c[1], "TYPE", ROLE[role], role * 1000 / (SAS_ROLES - 1), false);
            for (int k = 0; k < n; ++k) if (list[k] == lc->sound) at = k;
            sv_fmt_int(t, at + 1);
            sv_cat(t, "/", sizeof t);
            sv_fmt_int(a, n);
            sv_cat(t, a, sizeof t);
            sv_card_txt(&c[2], "INSTR", sas_of(lc->sound)->name, n > 1 ? at * 1000 / (n - 1) : -1, false);
            c[2].name = true;                      /* (its name; the full one on the panel and in the header) */
        }
        sv_card_txt(&c[3], "MUTE", v->mute ? "ON" : "OFF", -1, false);
        break;
    case SVP_HAND:                                 /* MANUAL; the rest off */
        sv_card_txt(&c[0], "MANUAL", v->hand ? "ON" : "OFF", -1, false);
        for (int i = 1; i < 4; ++i) sv_card_off(&c[i]);
        break;
    case SVP_KIT:
        sv_card_txt(&c[0], "KIT", sa_kit_name(s->kit), s->kit * 1000 / (SA_KITS - 1), false);
        sv_card_num(&c[1], "TUNE", s->ktune, 0, 100);
        sv_card_num(&c[2], "TONE", s->ktone, 0, 100);
        sv_card_num(&c[3], "DECAY", s->kdecay, 0, 100);
        break;
    default:
        for (int i = 0; i < 4; ++i) sv_card_off(&c[i]);
        break;
    }
}

/* ---- the header: playing, the tempo, where you are, the drop's badge, the battery ---- */
static void sv_head_title(const sa_ui *u, char *t, unsigned n)
{
    t[0] = 0;
    if (u->msg_ms > 0) { sv_cpy(t, u->msg, n); return; }
    switch (u->page) {
    case SV_PG_MAIN:
        sv_cpy(t, u->home2 == 1 ? "HOME  SPACE" : u->home2 == 2 ? "HOME  PARTS" : u->home2 == 3 ? "HOME  VIEW" : u->g->playing ? u->g->chord_name : "HOME", n);
        sv_cat(t, u->g->part_b ? "  B" : u->g->part_cue ? "  >B" : "  A", n);
        if (u->g->part_b && !u->g->part_cue) sv_cat(t, ">A", n);
        break;
    case SV_PG_KEY: sv_cpy(t, "KEY", n); break;
    case SV_PG_SETS: sv_cpy(t, "SETS", n); break;
    case SV_PG_MIXER: sv_cpy(t, u->mixmute == 6 ? "STYLE MIX" : u->mixmute == 5 ? "SONG" : u->mixmute == 4 ? "LIVE" : u->mixmute == 3 ? "STYLE" : u->mixmute == 2 ? "MIXER  SOLO" : u->mixmute ? "MIXER  MUTE" : "MIXER", n); break;
    case SV_PG_EDIT:
        sv_cpy(t, SV_VIEW_NAME[u->view], n);
        sv_cat(t, "  EDIT", n);
        break;
    case SV_PG_MENU: sv_cpy(t, u->menu2 ? "MIDI OUT" : "MENU", n); break;
    default:
        sv_cpy(t, SV_VIEW_NAME[u->view], n);
        sv_cat(t, "  ", n);
        sv_cat(t, sv_steps_on(u) ? "STEPS" : sv_page_name(sv_page_now(u)), n);
        if (u->rec) sv_cat(t, "  REC", n);
        break;
    }
}
static void sv_draw_head(sa_ui *u)
{
    const sa_groove *g = u->g;
    char b[16], m[40], bd[16];
    bool hot = u->hot == 8 && u->hot_ms > 0;
    int pend = sa_pending(g);
    sv_head_title(u, m, sizeof m);
    bd[0] = 0;
    if (g->drop_armed || g->set_next) {
        sv_cpy(bd, "ON THE 1", sizeof bd);
    } else if (pend) {
        sv_fmt_int(bd, pend);
        sv_cat(bd, " TO DROP", sizeof bd);
    }
    /* (0.8.31) PART about to change: a badge, A>B or B>A, blinking on the beats two bars before, on the eighths in the
     * last bar (the tick: in time) */
    bool to_b = false;
    int left = sa_part_next(g, &to_b), ab = 0;
    if (left == 1 || left == 2) {
        int t = g->tick < 0 ? 0 : g->tick;
        ab = 1 + (left == 1 ? t % 12 < 6 : t % 24 < 12);   /* 1 dim, 2 lit */
    }
    uint32_t sig = sv_hash(sv_hash((uint32_t)g->playing * 3u + (uint32_t)g->s.bpm * 101u + ux.gen * 13u +
                                   (uint32_t)u->batt * 7777u + (uint32_t)hot * 23u + (uint32_t)ab * 50021u +
                                   (uint32_t)to_b * 4099u, m), bd);
    if (!u->force && sig == u->sig_head)
        return;
    u->sig_head = sig;
    cv_begin(240, SV_HEAD, T_BG);
    sv_icon_mid(8, SV_HEAD / 2, 16, g->playing ? ICON_X_PLAY : ICON_X_STOP, g->playing ? T_THEME : T_MID, T_BG);
    sv_fmt_int(b, g->s.bpm);
    cv_text_on(28, CAP_IN(M, SV_HEAD), &AF_M, b, hot ? T_ACCENT : T_TEXT, T_BG);
    int32_t right = 208;
    if (bd[0]) {
        int32_t w = text_w(&AF_S, bd) + 10;
        cv_rrect(right - w, 4, w, SV_HEAD - 8, 3, T_ACCENT, T_BG);
        cv_text_in(right - w, 4 + CAP_IN(S, SV_HEAD - 8), w, &AF_S, bd, T_INK, T_ACCENT);
        right -= w + 6;
    }
    if (ab) {
        const char *ps = to_b ? "A>B" : "B>A";
        int32_t w = text_w(&AF_S, ps) + 10;
        bool lit = ab == 2;
        cv_rrect(right - w, 4, w, SV_HEAD - 8, 3, lit ? T_ACCENT : ux_mix(T_BG, T_ACCENT, 25), T_BG);
        cv_text_in(right - w, 4 + CAP_IN(S, SV_HEAD - 8), w, &AF_S, ps, lit ? T_INK : T_MID,
                   lit ? T_ACCENT : ux_mix(T_BG, T_ACCENT, 25));
        right -= w + 6;
    }
    if (m[0])
        cv_free_text(62, CAP_IN(S, SV_HEAD), &AF_S, m, u->msg_ms > 0 ? T_TEXT : T_THEME, T_BG, right - 62);
    {
        static const uint8_t BAT[5] = {ICON_X_BAT0, ICON_X_BAT1, ICON_X_BAT3, ICON_X_BAT4, ICON_X_BAT_CHG};
        int lv = sv_clampi(u->batt, 0, 4);
        sv_icon_mid(214, SV_HEAD / 2, 24, BAT[lv], lv == 1 ? T_ACCENT : lv == 0 ? T_MID : T_THEME, T_BG);
    }
    cv_blit(0, 0);
}

/* ---- the footer: the bar as 16 dashes, where you are (a layer: a dot a page), what the next press does ---- */
static void sv_draw_foot(sa_ui *u)
{
    const sa_groove *g = u->g;
    int now = sv_now(g);
    char l[24], r[24];
    int pg[8], np = 0;
    l[0] = r[0] = 0;
    if (u->page == SV_PG_LAYER) {
        np = sv_pages(u, pg);
        sv_cpy(l, SV_VIEW_NAME[u->view], sizeof l);
    } else {
        sv_cpy(l, u->page == SV_PG_MAIN ? "HOME" : u->page == SV_PG_KEY ? "KEY" : u->page == SV_PG_SETS ? "SETS" :
                  u->page == SV_PG_MIXER ? "MIXER" : u->page == SV_PG_EDIT ? SV_VIEW_NAME[u->view] : "MENU", sizeof l);
    }
    if (u->page == SV_PG_SETS) {
        static const char *const ACT[SV_SA_ACTS] = {"REC: LOAD", "REC: STORE", "REC: NEW", "REC: CLEAR"};
        sv_cpy(r, g->set_next ? "IN ON THE 1" : ACT[sv_wrap(u->set_act, SV_SA_ACTS)], sizeof r);
    } else if (sa_pending(g)) {
        sv_cpy(r, g->drop_armed ? "DROPS ON THE 1" : "SEQ: DROP", sizeof r);
    } else if (u->rec) {
        sv_cpy(r, "KEYS: THE GATE", sizeof r);
    } else if (u->page == SV_PG_LAYER) {
        sv_cpy(r, "FX: ", sizeof r);
        sv_cat(r, sv_page_name(pg[(sv_clampi(u->fx, 0, np - 1) + 1) % np]), sizeof r);
    } else if (u->page == SV_PG_MAIN) {
        sv_cpy(r, u->home2 == 3 ? "HOME: BACK" : u->home2 == 2 ? "FX: FILTER" : u->home2 == 1 ? "FX: PARTS" : "FX: RESO", sizeof r);
    } else if (u->page == SV_PG_EDIT) {
        sv_cpy(r, "KEYS: WRITE", sizeof r);
    } else if (u->page == SV_PG_MIXER) {
        sv_cpy(r, u->mixmute == 6 ? (u->g->mx_state ? "MIXING" : "SEQ: MIX") : u->mixmute == 5 ? "GLO: BACK" : u->mixmute == 4 ? (u->g->set_next ? "IN ON THE 1" : u->g->playing && sa_build_peek(u->g) >= 0 ? "SEQ: NEXT IN" : "SEQ: SET IT UP") : u->mixmute == 3 ? (u->g->set_next ? "IN ON THE 1" : "SEQ: A NEW GROOVE") : u->mixmute == 2 ? "GLO: STYLE" :
                  u->mixmute ? "GLO: SOLO" : "GLO: MUTE", sizeof r);
    }
    uint32_t sig = sv_hash(sv_hash((uint32_t)(now + 1) * 7u + (uint32_t)g->playing * 3u + ux.gen * 11u +
                                   (uint32_t)u->fx * 101u + (uint32_t)np * 1009u, l), r);
    if (!u->force && sig == u->sig_foot)
        return;
    u->sig_foot = sig;
    cv_begin(240, SV_FOOT_H, T_BG);
    for (int i = 0; i < 16; ++i)
        cv_rect(10 + i * 14, 6, 10, 2, i < now ? T_THEME : i == now ? T_ACCENT : T_LINE);
    cv_free_text(10, 17, &AF_M, l, T_THEME, T_BG, 90);
    int32_t lw = 10 + text_w(&AF_M, l);
    for (int i = 0; i < np; ++i)                   /* the layer's pages: the one open lit */
        cv_rect(6 + lw + i * 7, 22, 4, 4, i == u->fx ? T_ACCENT : T_LINE);
    int32_t yb = 17 + AF_M_CAP_Y + AF_M_CAP_H - AF_S_CAP_Y - AF_S_CAP_H;
    if (r[0]) cv_text_r(230, yb, &AF_S, r, sa_pending(g) || g->set_next || u->rec ? T_ACCENT : T_MID, T_BG);
    cv_blit(0, SV_FOOT_Y);
}

/* ---- the panels ----------------------------------------------------------------------------------------------- */
static void sv_panel_begin(void)
{
    cv_begin(240, SV_PANEL_H, T_BG);
    cv_rrect(3, 0, 234, SV_PANEL_H, 4, T_SURF, T_BG);
}
static uint16_t sv_well(void) { return ux_mix(T_SURF, T_LINE, 100); }
static void sv_label(int32_t x, int32_t y, const char *s, uint16_t c) { cv_text_on(x, y, &AF_S, s, c, T_SURF); }

/* a bar as 16 cells: on, the one playing lit */
static void sv_strip(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t bits, int now, uint16_t on, bool muted)
{
    int32_t cw = w / 16;
    for (int i = 0; i < 16; ++i) {
        bool b = (bits >> i) & 1u;
        uint16_t c = b ? (muted ? T_DIM : on) : sv_well();
        if (i == now && !muted) c = b ? T_TEXT : ux_mix(T_SURF, T_TEXT, 30);
        cv_rect(x + i * cw + 1, y, cw - 2 > 1 ? cw - 2 : 1, h, c);
    }
}


static void sv_panel_steps(sa_ui *u)
{
    sa_groove *g = u->g;
    sa_code o = *sv_ed(u);
    sa_own_make(g, &o);                            /* (a ready pattern as its steps: the first key makes it own) */
    bool pend = sa_layer_pending(g, u->layer);
    int now = g->playing && g->tick >= 0 ? g->tick / SA_TPS : -1, lo = 127, hi = 0;
    for (int k = 0; k < 16; ++k)
        if (o.sn[k] != SA_NO_NOTE && !((o.chord >> k) & 1u)) {
            if (o.sn[k] < lo) lo = o.sn[k];
            if (o.sn[k] > hi) hi = o.sn[k];
        }
    if (lo > hi) { lo = 36; hi = 48; }
    if (hi - lo < 12) { int m = (lo + hi) / 2; lo = m - 6; hi = m + 6; }
    char t[32];
    sa_pat_label(sv_ed(u), t, sizeof t);
    cv_free_text(10, 3, &AF_S, t, pend ? T_ACCENT : T_THEME, T_SURF, 150);
    cv_text_r(230, 3, &AF_S, sa_chordy(&o) ? "KEYS: CHORDS" : "KEYS: NOTES", T_ACCENT, T_SURF);
    int32_t y0 = 18, h = 80;
    for (int k = 0; k < 16; ++k) {
        int32_t x = 8 + k * 14;
        cv_rect(x, y0, 1, h, k % 4 ? ux_mix(T_SURF, T_LINE, 60) : T_LINE);
        if (k == now) cv_rect(x + 6, y0, 1, h, T_ACCENT);
        bool chord = (o.chord >> k) & 1u, note = o.sn[k] != SA_NO_NOTE;
        int32_t w = o.sl[k] * 14 - 2;
        if (x + w > 232) w = 232 - x;
        uint16_t c = pend ? T_ACCENT : T_THEME;
        if (chord) {
            for (int r = 0; r < 3; ++r) cv_rrect(x + 1, y0 + h / 2 - 12 + r * 9, w, 6, 2, c, T_SURF);
        } else if (note) {
            int32_t y = y0 + h - 8 - (o.sn[k] - lo) * (h - 10) / (hi - lo);
            cv_rrect(x + 1, y, w, 6, 2, c, T_SURF);
        }
    }
    int st = sv_clampi(u->scur, 0, 15);
    cv_rrect(8 + st * 14 - 1, y0 - 2, 15, h + 4, 2, T_TEXT, T_SURF);
    cv_rrect(8 + st * 14, y0 - 1, 13, h + 2, 2, T_SURF, T_TEXT);
    for (int k = 0; k < 16; ++k) {                 /* (the cursor's frame redrawn over: its notes on top) */
        if (k != st) continue;
        bool chord = (o.chord >> k) & 1u, note = o.sn[k] != SA_NO_NOTE;
        int32_t x = 8 + k * 14, w = o.sl[k] * 14 - 2;
        if (x + w > 232) w = 232 - x;
        uint16_t c = pend ? T_ACCENT : T_THEME;
        if (chord) for (int r = 0; r < 3; ++r) cv_rrect(x + 1, y0 + h / 2 - 12 + r * 9, w, 6, 2, c, T_SURF);
        else if (note) cv_rrect(x + 1, y0 + h - 8 - (o.sn[k] - lo) * (h - 10) / (hi - lo), w, 6, 2, c, T_SURF);
    }
    cv_text_on(10, 104, &AF_S, "KNOB 1: STEP. KEY OR KNOB 3: NOTE", T_DIM, T_SURF);
}

static void sv_panel_gate(sa_ui *u)
{
    sa_groove *g = u->g;
    int l = sv_clampi(u->layer, 0, SA_LAYERS - 1);
    const sa_gate *e = &g->ed_gate[l];
    bool pend = memcmp(e, &g->s.gate[l], sizeof *e) != 0;
    int now = g->playing && g->tick >= 0 ? g->tick / SA_TPS : -1;
    uint16_t m = sa_gate_on(e) ? sa_gate_mask(e) : 0;
    char t[32];
    sa_gate_label(e, t, sizeof t);
    cv_text_on(10, 36, &AF_S, t, pend ? T_ACCENT : T_THEME, T_SURF);
    cv_text_r(230, 36, &AF_S, "NO NEW ATTACK", T_DIM, T_SURF);
    /* the gain over the bar: each step open for WIDTH, down by DEPTH between */
    int32_t y0 = 52, h = 30, closed = h * e->depth / 100;
    for (int k = 0; k < 16; ++k) {
        int32_t x = 8 + k * 14, ow = 14 * e->width / 100;
        bool on = (m >> k) & 1u;
        uint16_t c = pend ? T_ACCENT : T_THEME;
        cv_rect(x, y0 + h - 2, 14, 2, sv_well());
        if (on) cv_rect(x, y0, ow > 1 ? ow : 1, h, k == now ? T_TEXT : c);
        if (on && ow < 14) cv_rect(x + ow, y0 + closed, 14 - ow, h - closed, ux_mix(T_SURF, c, 30));
        if (!on) cv_rect(x, y0 + closed, 14, h - closed, ux_mix(T_SURF, c, 30));
    }
    for (int k = 0; k < 16; ++k) {                 /* the 16 step keys */
        int32_t x = 8 + k * 14;
        bool on = (m >> k) & 1u;
        cv_rrect(x, 88, 12, 12, 2, on ? (pend ? T_ACCENT : T_THEME) : sv_well(), T_SURF);
        if (k % 4 == 0) cv_rect(x, 102, 12, 2, T_DIM);
    }
    cv_text_on(10, 108, &AF_S, u->rec ? "KEYS: ITS STEPS" : "REC: ITS STEPS ON THE KEYS", T_DIM, T_SURF);
}

static void sv_panel_pan(sa_ui *u)
{
    const sa_groove *g = u->g;
    int l = sv_clampi(u->layer, 0, SA_LAYERS - 1);
    const sa_pan *p = &g->s.pan[l];
    const sa_code *c = &g->s.code[l];
    static const char *const SAY[SA_PAN_MODES] = {"WHERE PAN SAYS", "FROM SIDE TO SIDE OVER RATE",
                                                  "EACH NOTE ON THE OTHER SIDE", "EACH NOTE SOMEWHERE IN WIDTH"};
    bool blocked = sv_pan3d_on(u);
    cv_text_on(10, 38, &AF_S, blocked ? "OFF: PAN 3D MOVES THIS LAYER" : SAY[p->mode % SA_PAN_MODES],
               blocked ? T_ACCENT : T_MID, T_SURF);
    int32_t x0 = 20, w = 200, y = 74;
    cv_rect(x0, y, w, 2, T_LINE);
    cv_rect(x0 + w / 2, y - 6, 1, 14, T_DIM);
    cv_text_on(x0 - 12, y - 5, &AF_S, "L", T_MID, T_SURF);
    cv_text_on(x0 + w + 4, y - 5, &AF_S, "R", T_MID, T_SURF);
    if (p->mode != SA_PAN_FIXED) {                 /* the range it moves in */
        int32_t a = x0 + (p->pos - p->width / 2 + 50) * w / 100, b = x0 + (p->pos + p->width / 2 + 50) * w / 100;
        if (a < x0) a = x0;
        if (b > x0 + w) b = x0 + w;
        cv_rect(a, y - 1, b - a, 4, ux_mix(T_SURF, T_ACCENT, 50));
    }
    int now = c->type == SA_T_SYNTH && c->part < SA_SYNTHS ? g->pan_now[c->part] : 0;
    int32_t px = x0 + (now + 64) * w / 128;
    cv_rrect(px - 5, y - 6, 10, 14, 3, T_ACCENT, T_SURF);
    cv_text_on(10, 100, &AF_S, blocked ? "PAN 3D OFF FIRST, TO USE PAN" : "AT ONCE, AS THE SOUND", T_DIM, T_SURF);
}

static void sv_panel_arp(sa_ui *u)
{
    sa_groove *g = u->g;
    const sa_code *e = sv_ed(u);
    int32_t Y = 36;
    char t[24];
    sv_cpy(t, "CHORD ", sizeof t);
    sv_cat(t, g->chord_name, sizeof t);
    sv_label(10, Y, t, T_MID);
    if (e->amode == SA_AR_OFF) {
        sv_label(10, Y + 24, "OFF: TURN MODE TO START IT", T_MID);
        return;
    }
    /* the order over the chord's tones and the octaves, a bar's worth of the rate (up to 32) */
    static const uint8_t RT[SA_RATES] = {24, 12, 8, 6, 4, 3};
    int ns = 3 * sv_clampi(e->arange, 1, 4), steps = 96 / RT[e->arate % SA_RATES];
    if (steps > 32) steps = 32;
    int32_t cw = 224 / steps, h = 64;
    for (int i = 0; i < steps; ++i) {
        int k;
        switch (e->amode) {                        /* (as sa_layer.c arp_pick, for the picture) */
        case SA_AR_DOWN: k = ns - 1 - i % ns; break;
        case SA_AR_UPDOWN: { int p = i % (2 * ns - 2); k = p < ns ? p : 2 * ns - 2 - p; break; }
        case SA_AR_DOWNUP: { int p = i % (2 * ns - 2); k = ns - 1 - (p < ns ? p : 2 * ns - 2 - p); break; }
        case SA_AR_CONVERGE: { int q = i % ns; k = q % 2 ? ns - 1 - q / 2 : q / 2; break; }
        case SA_AR_DIVERGE: { int q = ns - 1 - i % ns; k = q % 2 ? ns - 1 - q / 2 : q / 2; break; }
        case SA_AR_RANDOM: k = (int)((uint32_t)(i * 2654435761u) >> 28) % ns; break;
        case SA_AR_THUMB: { int q = i % (2 * ns - 2); k = q % 2 ? 1 + q / 2 : 0; break; }
        case SA_AR_PINKY: { int q = i % (2 * ns - 2); k = q % 2 ? q / 2 : ns - 1; break; }
        case SA_AR_STRUM: k = -1; break;
        default: k = i % ns; break;
        }
        int32_t x = 8 + i * cw;
        if (k < 0) {
            for (int s = 0; s < ns; ++s)
                cv_rect(x + 1, Y + 16 + h - 6 - s * (h - 8) / ns, cw - 2 > 1 ? cw - 2 : 1, 4, T_ACCENT);
        } else {
            cv_rect(x + 1, Y + 16 + h - 6 - k * (h - 8) / ns, cw - 2 > 1 ? cw - 2 : 1, 4, T_ACCENT);
        }
    }
}

static void sv_panel_key(sa_ui *u)
{
    sa_groove *g = u->g;
    const sa_harm *e = &g->ed_harm;
    char t[32];
    sv_cpy(t, sa_note_name(e->key), sizeof t);
    sv_cat(t, " ", sizeof t);
    sv_cat(t, sa_scale_name(e->scale), sizeof t);
    cv_text_on(10, 6, &AF_M, t, memcmp(e, &g->s.harm, sizeof *e) ? T_ACCENT : T_THEME, T_SURF);
    sa_prog_name(e->prog, t, sizeof t);
    sv_label(10, 34, t, T_TEXT);
    sv_label(10, 56, "THE SYNTH LAYERS FOLLOW THE", T_MID);
    sv_label(10, 72, "CHORD, THE DRUMS DO NOT.", T_MID);
    sv_label(10, 92, "IT DROPS WITH REC, ON THE 1.", T_MID);
    if (g->playing) {
        sv_cpy(t, "NOW ", sizeof t);
        sv_cat(t, g->chord_name, sizeof t);
        cv_text_r(230, 10, &AF_S, t, T_ACCENT, T_SURF);
    }
}

/* SETS: the eight sets and LAST, a row each (its tempo and key), the one picked lit; beside them what it holds (its
 * kit, its synth sounds) */
static void sv_panel_sets(sa_ui *u)
{
    for (int i = 0; i < SV_SLOTS; ++i) {
        const sv_set_info *si = &u->sets[i];
        int32_t y = 4 + i * 13;
        bool on = i == u->set_slot;
        char t[24], a[8];
        if (on) cv_rect(6, y - 1, 108, 13, sv_well());
        uint16_t bg = on ? sv_well() : T_SURF;
        if (i == SV_LAST) sv_cpy(a, "L", sizeof a);
        else sv_fmt_int(a, i + 1);
        cv_text_on(10, y + 1, &AF_S, a, on ? T_ACCENT : i == SV_LAST ? T_THEME : T_MID, bg);
        if (si->wait) {
            sv_cpy(t, "WAITS", sizeof t);
        } else if (!si->used) {
            sv_cpy(t, i == SV_LAST ? "LAST: NONE" : "EMPTY", sizeof t);
        } else {
            sv_fmt_int(t, si->bpm);
            sv_cat(t, " ", sizeof t);
            sv_cat(t, sa_note_name(si->key % 12), sizeof t);
            sv_cat(t, si->scale == SA_SC_MAJOR ? " MAJ" : si->scale == SA_SC_DORIAN ? " DOR" :
                      si->scale == SA_SC_PHRYGIAN ? " PHR" : " MIN", sizeof t);
        }
        cv_text_fit(26, y + 1, &AF_S, t, si->wait ? T_ACCENT : on ? T_ACCENT : si->used ? T_TEXT : T_DIM, bg, 86);
    }
    cv_rect(120, 6, 1, 110, T_LINE);
    int k = sv_clampi(u->set_slot, 0, SV_SLOTS - 1);
    const sv_set_info *si = &u->sets[k];
    if (si->wait) {
        sv_label(128, 6, si->wait == 2 ? "CLEARS WHEN" : "SAVES WHEN", T_ACCENT);
        sv_label(128, 22, "STOPPED", T_ACCENT);
        return;
    }
    if (k == SV_LAST) {
        sv_label(128, 6, "LAST", T_THEME);
        if (!si->used) {
            sv_label(128, 26, "KEPT BY ITSELF", T_MID);
            sv_label(128, 42, "WHEN STOPPED", T_MID);
            return;
        }
    } else if (!si->used) {
        sv_label(128, 6, "EMPTY", T_DIM);
        sv_label(128, 26, "STORE KEEPS", T_MID);
        sv_label(128, 42, "WHAT PLAYS", T_MID);
        return;
    }
    int32_t y0 = k == SV_LAST ? 22 : 6;
    cv_text_fit(128, y0, &AF_S, sa_kit_name(si->kit), T_MID, T_SURF, 104);
    for (int n = 0; n < si->nsyn && n < SA_SYNTHS; ++n)
        cv_text_fit(128, y0 + 20 + n * 16, &AF_S, sas_of(si->snd[n])->name, T_TEXT, T_SURF, 104);
}

/* MIDI OUT: what went out, the newest on top (the layer, its channel, the note or the drum, the velocity) */
static void sv_panel_midi(sa_ui *u)
{
    static const char *const N[4] = {"LEAD", "CHORD", "BASS", "DRUMS"}, *const NN[12] = {"C", "C#", "D", "D#", "E", "F",
                                                                                         "F#", "G", "G#", "A", "A#", "B"};
    uint32_t n = u->ml_n;
    if (!n) {
        sv_label(10, 40, "NOTHING SENT YET", T_DIM);
        sv_label(10, 56, "USB MIDI: THE LAYERS' NOTES", T_DIM);
        return;
    }
    for (uint32_t k = 0; k < 6 && k < n; ++k) {
        uint32_t i = (n - 1u - k) % SV_MLOG;
        int32_t y = 4 + (int32_t)k * 19;
        int l = u->ml_layer[i] % 4, note = u->ml_note[i], vel = u->ml_vel[i];
        uint16_t c = k == 0 ? T_TEXT : k < 3 ? T_MID : T_DIM;
        char t[20], a[8];
        cv_text_on(10, y, &AF_S, N[l], k == 0 ? T_THEME : c, T_SURF);
        sv_cpy(t, "CH ", sizeof t);
        sv_fmt_int(a, u->ml_ch[i]);
        sv_cat(t, a, sizeof t);
        cv_text_on(64, y, &AF_S, t, c, T_SURF);
        if (l == 3) {                              /* a drum: its name */
            const char *nm = "DRUM";
            for (int v = 0; v < SA_DVS; ++v)
                if (sa_dv_gm(v) == note || (v == SA_DV_TAMB && note == 54) || (v == SA_DV_BONGO && note == 60) ||
                    (v == SA_DV_WOOD && note == 76) || (v == SA_DV_COWBELL && note == 56) ||
                    (v == SA_DV_CABASA && note == 69) || (v == SA_DV_ZAP && note == 80)) { nm = sa_dv_name(v); break; }
            sv_cpy(t, nm, sizeof t);
        } else {
            sv_cpy(t, NN[note % 12], sizeof t);
            sv_fmt_int(a, note / 12 - 1);
            sv_cat(t, a, sizeof t);
        }
        cv_free_text(112, y, &AF_S, t, c, T_SURF, 80);
        sv_fmt_int(a, vel);
        cv_text_r(232, y, &AF_S, a, c, T_SURF);
    }
}

static void sv_panel_menu(sa_ui *u)
{
    if (u->menu2) { sv_panel_midi(u); return; }
    cv_text_on(30, 10, &AF_M, "SARDINE " SA_VERSION, T_TEXT, T_SURF);
    cv_text_on(30, 34, &AF_S, "LIVE CLUB MIXING FOR THE FM-1", T_MID, T_SURF);
    cv_text_on(30, 50, &AF_S, "ON FELUCCA, AFTER TUNA", T_MID, T_SURF);
    if (u->clock_ext) {                            /* (1.0.2) CLOCK EXT: the clock coming in, or none */
        char t[32], a[8];
        bool in = u->clock_in != 0;
        sv_cpy(t, "CLOCK IN: ", sizeof t);
        if (in) { sv_fmt_int(a, u->g->s.bpm); sv_cat(t, a, sizeof t); sv_cat(t, " BPM", sizeof t); }
        else sv_cat(t, "NONE YET", sizeof t);
        cv_text_on(30, 66, &AF_S, t, in ? T_THEME : T_DIM, T_SURF);
    } else {
        cv_text_on(30, 66, &AF_S, "GPL-3.0, HOME: MIDI OUT", T_DIM, T_SURF);
    }
    {   /* the CPU: the audio's load, its average over a tenth of a second, and its worst since the start (0.8.33: the
         * peak on a line of its own, under CPU: beside it, it ran into the percentage) */
        char t[24], a[8];
        cv_rect(30, 82, 180, 1, T_LINE);
        cv_text_on(30, 88, &AF_M, "CPU", T_TEXT, T_SURF);
        sv_fmt_int(t, sv_clampi(u->cpu, 0, 100));
        sv_cat(t, " %", sizeof t);
        cv_text_r(210, 88, &AF_M, t, u->cpu >= 85 ? T_ACCENT : T_THEME, T_SURF);
        sv_cpy(t, "PEAK ", sizeof t);
        sv_fmt_int(a, sv_clampi(u->cpu_peak, 0, 999));
        sv_cat(t, a, sizeof t);
        sv_cat(t, " %", sizeof t);
        if (u->shed) { sv_cat(t, ", SHED ", sizeof t); sv_fmt_int(a, u->shed); sv_cat(t, a, sizeof t); }
        cv_text_on(30, 108, &AF_S, t, T_DIM, T_SURF);
    }
}

/* the copy's bar of a layer, worked out again when it changed */
static uint16_t sv_code_bits(sa_ui *u, int l, const sa_code *c);
static uint16_t sv_ed_bits(sa_ui *u, int l) { return sv_code_bits(u, l, &u->g->ed[l]); }
static uint16_t sv_code_bits(sa_ui *u, int l, const sa_code *c)   /* (cached: a layer's last) */
{
    if (u->force || memcmp(&u->ed_code[l], c, sizeof *c)) {
        SV_LOCK();
        u->ed_bits[l] = c->type == SA_T_NONE ? 0 : sa_preview(u->g, c, l, 0);
        SV_UNLOCK();
        u->ed_code[l] = *c;
    }
    return u->ed_bits[l];
}
/* what a layer played this bar so far, and the bar before after that (playing); stopped: its code's bar */
static uint16_t sv_play_bits(sa_ui *u, int l)
{
    const sa_groove *g = u->g;
    int now = sv_now(g);
    if (!g->playing) return u->main_bits[l];
    uint32_t m = (2u << (now < 0 ? 0 : now)) - 1u;
    return (uint16_t)((g->hits[l] & m) | (g->hits_last[l] & ~m));
}

/* HOME: every layer playing. LEAD CHORD BASS a row each (its sound, its bar), DRUMS a thin bar per drum */
static void sv_panel_view(sa_ui *u);
static void sv_panel_home(sa_ui *u)
{
    if (u->home2 == 3) { sv_panel_view(u); return; }
    sa_groove *g = u->g;
    int now = sv_now(g);
    if (u->main_ms <= 0) {                         /* (stopped: the code's bars, worked out a few times a second) */
        for (int l = 0; l < SA_LAYERS; ++l)
            if (memcmp(&u->main_code[l], &g->s.code[l], sizeof g->s.code[l])) {
                SV_LOCK();
                u->main_bits[l] = sa_preview(g, &g->s.code[l], l, 0);
                SV_UNLOCK();
                u->main_code[l] = g->s.code[l];
            }
        u->main_ms = 250;
    }
    for (int v = SV_V_LEAD; v <= SV_V_BASS; ++v) {
        int l = sv_slot_of_view(v);
        int32_t y = 5 + (v - 1) * 20;
        const sa_code *c = &g->s.code[l];
        bool mute = g->s.live[l].mute, off = c->pat == SA_PAT_OFF && !c->own, pend = sa_layer_pending(g, l);
        sv_label(10, y + 2, SV_VIEW_NAME[v], T_MID);
        cv_free_text(58, y + 2, &AF_S, sas_of(c->sound)->name, mute || off ? T_DIM : pend ? T_ACCENT : T_TEXT, T_SURF, 84);
        if (off) cv_text_on(146, y + 2, &AF_S, "OFF", T_DIM, T_SURF);   /* (OFF: its sound still shown) */
        else sv_strip(146, y + 4, 88, 8, sv_play_bits(u, l), now, v == SV_V_BASS ? T_THEME : T_ACCENT, mute);
        if (mute) cv_rect(146, y + 7, 88, 2, T_ACCENT);
    }
    int32_t y = 66;
    sv_label(10, y + 2, "DRUMS", T_MID);
    int rows[SA_LAYERS], n = sv_drum_rows(u, rows);
    if (n == 0) cv_text_on(58, y + 2, &AF_S, "NONE", T_DIM, T_SURF);
    int per = n > 6 ? 6 : 8;                       /* (up to nine drums: thinner bars) */
    for (int i = 0; i < n; ++i) {
        int l = rows[i];
        const sa_code *c = &g->s.code[l];
        bool mute = g->s.live[l].mute || c->type != SA_T_DRUM, off = c->pat == SA_PAT_OFF && !c->own;
        int32_t yy = y + 2 + i * per;
        sv_strip(146, yy, 88, per - 2, off ? 0 : sv_play_bits(u, l), now, T_THEME, mute || off);
    }
    if (n) {
        char t[8];
        sv_fmt_int(t, n);
        cv_text_on(58, y + 2, &AF_S, t, T_TEXT, T_SURF);
    }
}

/* a layer's top: its sound now, as a wave (silent: a flat line); its pattern, and what SEQ drops when it differs */
/* a layer's code with a B pattern in place of its pattern (b: 0 as it is, SA_PATB_OFF OFF, else the pattern + 1) */
static const sa_code *sv_with_b(sa_code *tmp, const sa_code *c, uint8_t b)
{
    if (b == 0) return c;
    *tmp = *c;
    tmp->own = 0;
    tmp->pat = b == SA_PATB_OFF ? SA_PAT_OFF : (uint8_t)(b - 1);
    return tmp;
}

/* a code's bar (a bit a sixteenth), worked out again when it changed (slot 0: what plays, 1: what waits) */
static uint16_t sv_pv(sa_ui *u, int slot, const sa_code *c, int l)
{
    if (u->force || u->pv_layer != l || memcmp(&u->pv_code[slot], c, sizeof *c)) {
        if (u->pv_layer != l) memset(u->pv_code, 0xFF, sizeof u->pv_code);
        u->pv_layer = l;
        SV_LOCK();
        u->pv_bits[slot] = c->type == SA_T_NONE ? 0 : sa_preview(u->g, c, l, 0);
        SV_UNLOCK();
        u->pv_code[slot] = *c;
    }
    return u->pv_bits[slot];
}

/* a layer's top: its pattern's name and bar (the step playing lit); what SEQ drops below it, in the accent */
static void sv_layer_top(sa_ui *u, int l)
{
    sa_groove *g = u->g;
    static sa_code tb[2];
    const sa_code *e = &g->ed[l], *lv = &g->s.code[l];
    if (g->part_cue && !g->s.live[l].hand) {       /* PART B: B's patterns (MANUAL: A's, always) */
        e = sv_with_b(&tb[0], e, g->ed_patb[l]);
        lv = sv_with_b(&tb[1], lv, g->s.patb[l]);
    }
    bool pend = sa_layer_pending(g, l), mute = g->s.live[l].mute;
    bool off = lv->type == SA_T_NONE || (lv->pat == SA_PAT_OFF && !lv->own);
    int now = sv_now(g);
    char t[24];
    sa_pat_label(lv, t, sizeof t);
    cv_free_text(10, 4, &AF_S, mute ? "MUTED" : off ? "OFF" : t, mute ? T_ACCENT : off ? T_DIM : T_TEXT, T_SURF, 66);
    sv_strip(80, 5, 152, 9, off ? 0 : sv_pv(u, 0, lv, l), now, T_THEME, mute || off);
    if (pend) {
        bool gone = e->type == SA_T_NONE || (e->pat == SA_PAT_OFF && !e->own);
        if (e->type == SA_T_NONE) sv_cpy(t, "GOES", sizeof t);
        else sa_pat_label(e, t, sizeof t);
        cv_free_text(10, 19, &AF_S, gone ? "OFF" : t, T_ACCENT, T_SURF, 66);
        sv_strip(80, 20, 152, 9, gone ? 0 : sv_pv(u, 1, e, l), now, T_ACCENT, gone);
    }
}

/* the scope: the layer's sound now, a cycle or two, across the bottom */
static void sv_scope(sa_ui *u, int32_t x0, int32_t cy, int32_t w, int32_t h, uint16_t c)
{
    cv_rect(x0, cy, w, 1, ux_mix(T_SURF, T_LINE, 60));
    if (u->wave_n < 2) return;
    int32_t py = cy - u->wave[0] * h / 120;
    for (int i = 1; i < u->wave_n; ++i) {
        int32_t x = x0 + i * (w - 1) / (u->wave_n - 1), y = cy - u->wave[i] * h / 120;
        cv_line_t(x0 + (i - 1) * (w - 1) / (u->wave_n - 1), py, x, y, c, 2);
        py = y;
    }
}

static void sv_panel_mod(sa_ui *u)
{
    const sa_code *e = sv_ed(u);
    bool drum = u->view == SV_V_DRUMS;
    int n = 0, nm = drum ? 3 : SA_MODS;
    for (int i = 0; i < nm; ++i) {
        char t[20], a[4];
        sv_fmt_int(a, i + 1);
        sv_label(10, 40 + i * 16, a, T_DIM);
        sa_mod_name(e->mod[i], t, sizeof t);
        bool on = e->mod[i].type != SA_M_OFF;
        n += on;
        cv_text_on(26, 40 + i * 16, &AF_S, t, on ? T_TEXT : T_DIM, T_SURF);
    }
    if (drum) {
        char t[20];
        sv_cpy(t, "CHANCE ", sizeof t);
        char a[8];
        sv_fmt_int(a, u->g->s.live[u->layer].cut);
        sv_cat(t, a, sizeof t);
        sv_cat(t, " %", sizeof t);
        cv_text_on(10, 40 + 3 * 16, &AF_S, t, u->g->s.live[u->layer].cut < 100 ? T_ACCENT : T_DIM, T_SURF);
    }
    static const char *const HINT[2][4] = {{"FAST SLOW REV", "EVERY THIN", "EUCLID SHIFT", "LEN OCT PLY"},
                                           {"IN ORDER,", "EACH ON WHAT", "CAME BEFORE", ""}};
    for (int i = 0; i < 4; ++i) sv_label(124, 40 + i * 16, HINT[n ? 1 : 0][i], T_DIM);
    sv_label(124, 106, "SEQ DROPS THEM", T_DIM);
}

/* 3D: you from above (FRONT up), the layer a dot around you, near the centre or far; beside it, from the side, its
 * height */
static void sv_ring(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    for (int k = 0; k < SA_AZ_STEPS; k += 3)
        cv_line_t(cx + sa_sin5(k) * r / 127, cy - sa_sin5(k + 18) * r / 127, cx + sa_sin5(k + 3) * r / 127,
                  cy - sa_sin5(k + 21) * r / 127, c, 1);
}
static void sv_panel_3d(sa_ui *u)
{
    int pp = sv_clampi(sv_live(u)->part, 0, SA_SYNTHS - 1);
    sa_3d dd = u->g->s.d3[pp];
    const sa_3d *d = &dd;
    if (dd.on) { dd.az = u->g->az_now[pp]; dd.el = u->g->el_now[pp]; }   /* (where it is now, moving) */
    uint16_t dot = d->on ? T_ACCENT : T_DIM;
    int32_t cx = 74, cy = 78, R = 36;
    sv_ring(cx, cy, R, T_MID);
    sv_ring(cx, cy, R * 55 / 100, ux_mix(T_SURF, T_LINE, 60));
    sv_label(cx - 14, 34, "FRONT", T_DIM);
    cv_rrect(cx - 5, cy - 5, 10, 10, 5, T_MID, T_SURF);               /* you */
    cv_rect(cx - 1, cy - 9, 2, 4, T_MID);                             /* your nose: the front */
    int32_t rr = 8 + d->dist * (R - 8) / 100;
    int32_t x = cx + sa_sin5(d->az) * rr / 127, y = cy - sa_sin5(d->az + 18) * rr / 127;
    int32_t sz = 8 + d->el * 4 / 50;                                  /* (above: a bigger dot, below: smaller) */
    cv_rrect(x - sz / 2, y - sz / 2, sz, sz, sz / 2, dot, T_SURF);
    if (sv_pan_on(u)) {                            /* (PAN on: PAN 3D greyed, why) */
        sv_label(130, 44, "OFF: PAN IS ON", T_ACCENT);
        sv_label(130, 62, "SET PAN TO", T_DIM);
        sv_label(130, 78, "FIXED, C, TO", T_DIM);
        sv_label(130, 94, "USE PAN 3D", T_DIM);
        return;
    }
    /* the side view: you, the height */
    int32_t sx = 186, sy = 78;
    cv_rect(sx - 30, sy, 60, 1, T_LINE);
    cv_rrect(sx - 5, sy - 5, 10, 10, 5, T_MID, T_SURF);
    sv_label(sx - 22, 34, d->el > 0 ? "ABOVE" : d->el < 0 ? "BELOW" : "LEVEL", T_DIM);
    int32_t hy = sy - d->el * 30 / 50, hx = sx + 22;
    cv_line_t(sx, sy, hx, hy, ux_mix(T_SURF, dot, 50), 1);
    cv_rrect(hx - 4, hy - 4, 8, 8, 4, dot, T_SURF);
    sv_label(140, 108, d->on ? "HEADPHONES" : "MODE: KNOB 1", T_DIM);
}

/* PUMP: the layer's gain over a beat, down on the kick (or the beat) and back over RELEASE */
static void sv_panel_pump(sa_ui *u)
{
    const sa_groove *g = u->g;
    const sa_pumpl *q = &g->s.pl[sv_clampi(sv_live(u)->part, 0, SA_SYNTHS - 1)];
    float depth = q->on ? (float)g->s.pump / 100.0f * 0.85f * (float)q->depth / 100.0f : 0.0f;
    float back = 0.25f + (float)q->rel * 0.009f;
    int32_t x0 = 12, w = 216, y0 = 40, h = 52, py = -1;
    for (int32_t x = 0; x < w; ++x) {
        float t = (float)x / (float)w, dd;
        if (t < 0.02f) dd = depth * t / 0.02f;
        else { float k = (t - 0.02f) / back; dd = k < 1.0f ? depth * (1.0f - k) * (1.0f - k) : 0.0f; }
        int32_t y = y0 + (int32_t)(dd * (float)h);
        if (py >= 0) cv_line_t(x0 + x - 1, py, x0 + x, y, q->on ? T_ACCENT : T_DIM, 2);
        py = y;
    }
    cv_rect(x0, y0 - 2, 2, h + 4, T_THEME);
    sv_label(10, 100, q->src == SA_PS_BEAT ? "ON EVERY BEAT" : "UNDER THE KICK LAYER'S HITS", T_DIM);
}

static void sv_panel_hand(sa_ui *u)          /* (0.8.42) MANUAL: what it means */
{
    bool on = u->g->s.live[sv_clampi(u->layer, 0, SA_LAYERS - 1)].hand;
    cv_free_text(10, 40, &AF_M, on ? "BY HAND" : "SARDINE PLAYS ALONG", on ? T_THEME : T_DIM, T_SURF, 220);
    sv_label(10, 72, on ? "PLAYS AS IT IS: NO PHRASE, NO B," : "IT MAY CHANGE: PHRASE, A/B, EVOLVE,", T_MID);
    sv_label(10, 88, on ? "NO EVOLVE, INTRO, ARC, LIVE" : "INTRO, ARC AND LIVE", T_DIM);
}

static void sv_panel_mix(sa_ui *u)
{
    const sa_live *v = &u->g->s.live[sv_clampi(u->layer, 0, SA_LAYERS - 1)];
    cv_free_text(10, 40, &AF_M, u->view == SV_V_DRUMS ? sv_layer_name(u, u->layer) : sas_of(sv_live(u)->sound)->name,
                 v->mute ? T_DIM : T_THEME, T_SURF, 160);
    int32_t w = 212, f = v->level * w / 100;
    cv_rect(14, 78, w, 6, sv_well());
    cv_rect(14, 78, f > 0 ? f : 1, 6, v->mute ? T_DIM : T_THEME);
    cv_rect(14 + 80 * w / 100, 74, 1, 14, T_MID);
    sv_label(10, 98, "AT ONCE: VOLUME FADES IT", T_DIM);
}

static void sv_panel_synth_home(sa_ui *u)
{
    const sa_code *lc = sv_live(u);
    bool mute = u->g->s.live[u->layer].mute, off = lc->pat == SA_PAT_OFF && !lc->own;
    cv_free_text(10, 40, &AF_M, sas_of(lc->sound)->name, mute || off ? T_DIM : T_THEME, T_SURF, 220);
    sv_scope(u, 8, 92, 224, 26, mute || off ? T_DIM : T_THEME);   /* (the bottom, the whole width) */
}

/* DRUMS: every row (its drum, its bar, the bar playing through it; OFF dimmed), KNOB 1's pick lit */
static void sv_panel_drums(sa_ui *u)
{
    sa_groove *g = u->g;
    int rows[SA_LAYERS], n = sv_drum_rows(u, rows), total = sv_drow_count(u), now = sv_now(g);
    int vis = 7, first = u->drow >= vis ? u->drow - vis + 1 : 0;
    cv_rrect(5, 5 + (u->drow - first) * 16 - 2, 230, 15, 3, ux_mix(T_SURF, T_THEME, 22), T_SURF);   /* (first: under it all) */
    for (int i = first; i < total && i < first + vis; ++i) {
        int32_t y = 5 + (i - first) * 16;
        bool sel = i == u->drow;
        uint16_t bg = sel ? ux_mix(T_SURF, T_THEME, 22) : T_SURF;
        int l = rows[i];
        static sa_code tb;
        const sa_code *e = &g->ed[l];
        if (g->part_cue && !g->s.live[l].hand) e = sv_with_b(&tb, e, g->ed_patb[l]);   /* (PART B: the rows' B
                                                    * patterns; MANUAL: A's) */
        bool pend = sa_layer_pending(g, l), mute = g->s.live[l].mute, gone = e->type == SA_T_NONE;
        bool off = e->pat == SA_PAT_OFF && !e->own;
        cv_free_text(10, y + 1, &AF_S, sv_layer_name(u, l), mute || gone || off ? T_DIM : sel ? T_TEXT : T_MID, bg, 64);
        uint16_t bits = gone || off ? 0 : sv_code_bits(u, l, e);
        bool out = mute || ((g->build_out >> l) & 1u);   /* (muted, or a LIVE build's not in yet) */
        sv_strip(80, y + 2, 152, 8, bits, now, pend ? T_ACCENT : sel ? T_THEME : ux_mix(T_SURF, T_THEME, 70),
                 (out && !pend) || gone);              /* (a new pattern waiting shows lit: SEQ brings it in) */
        if (mute && !pend) cv_rect(80, y + 5, 152, 2, T_ACCENT);
    }
}

static void sv_panel_layer(sa_ui *u)
{
    if (u->layer < 0) { sv_panel_drums(u); return; }
    if (sv_steps_on(u)) {
        sv_panel_steps(u);
        return;
    }
    int pg = sv_page_now(u);
    if (u->view == SV_V_DRUMS && pg == SVP_HOME) {
        sv_panel_drums(u);
        return;
    }
    sv_layer_top(u, u->layer);
    switch (pg) {
    case SVP_HOME: sv_panel_synth_home(u); break;
    case SVP_ARP: sv_panel_arp(u); break;
    case SVP_PAN: sv_panel_pan(u); break;
    case SVP_3D: sv_panel_3d(u); break;
    case SVP_PUMP: sv_panel_pump(u); break;
    case SVP_GATE: sv_panel_gate(u); break;
    case SVP_MOD: sv_panel_mod(u); break;
    case SVP_MIX: sv_panel_mix(u); break;
    case SVP_HAND: sv_panel_hand(u); break;
    default:                                       /* KIT */
        cv_free_text(10, 42, &AF_M, sa_kit_name(u->g->s.kit), T_THEME, T_SURF, 220);
        sv_label(10, 72, "TUNE TONE DECAY: THE WHOLE KIT", T_MID);
        sv_label(10, 88, "(EVERY DRUM PLAYS ON IT)", T_DIM);
        break;
    }
}

/* (0.8.44) ARC on the STYLE page: the INTRO (if on) and one cycle drawn as its energy, the groove full, the break low,
 * the build rising to the drop; where the groove is now (playing) a line through it, and the part and the bars to the
 * drop under it */
static float sv_arc_energy(int n, int len, float b)   /* 0..1 at bar b (from PLAY) */
{
    if (b < (float)n) return 0.2f + 0.8f * b / (float)n;
    float p = b - (float)n, q = (float)len / 4.0f;
    p -= (float)len * (float)(int)(p / (float)len);
    return p < 2.0f * q ? 1.0f : p < 3.0f * q ? 0.3f : 0.3f + 0.7f * (p - 3.0f * q) / q;
}
static void sv_arc_draw(sa_ui *u, int32_t x0, int32_t y0, int32_t w, int32_t h, int32_t ty)
{
    const sa_groove *g = u->g;
    int n = sa_intro_bars(g->s.intro), len = sa_arc_bars(g->s.arc), total = n + len;
    float now = -1.0f;
    if (g->playing && g->bar >= 0) {
        now = (float)g->bar + (float)g->tick / (float)SA_TPB;
        if (now >= (float)n) now = (float)n + (now - (float)n) - (float)len * (float)(int)((now - (float)n) / (float)len);
    }
    int32_t nx = now < 0.0f ? -1 : (int32_t)(now * (float)w / (float)total);
    for (int32_t x = 0; x < w; ++x) {
        float b = ((float)x + 0.5f) * (float)total / (float)w, p = b - (float)n;
        float q = (float)len / 4.0f;
        uint16_t col = b < (float)n ? T_MID : p < 2.0f * q ? T_THEME : p < 3.0f * q ? T_DIM : T_ACCENT;
        int32_t e = (int32_t)(sv_arc_energy(n, len, b) * (float)h + 0.5f);
        if (e < 2) e = 2;
        bool past = nx >= 0 && x <= nx;
        cv_rect(x0 + x, y0 + h - e, 1, e, ux_mix(T_SURF, col, past ? 55 : 28));
        cv_rect(x0 + x, y0 + h - e, 1, 2, col);
    }
    cv_rect(x0 + w - 2, y0 - 4, 2, h + 4, T_ACCENT);   /* the drop */
    if (n) cv_rect(x0 + n * w / total, y0, 1, h, T_RULE);   /* INTRO's end */
    if (nx >= 0) cv_rect(x0 + (nx < w - 1 ? nx : w - 1), y0 - 4, 1, h + 8, T_TEXT);
    char t[40], a[8];
    if (now < 0.0f) {                              /* stopped: what a cycle is */
        sv_cpy(t, "ARC ", sizeof t);
        sv_fmt_int(a, len);
        sv_cat(t, a, sizeof t);
        sv_cat(t, ": GROOVE, BREAK, BUILD, DROP", sizeof t);
    } else if (now < (float)n) {
        sv_cpy(t, "INTRO, THE ARC IN ", sizeof t);
        sv_fmt_int(a, n - (int)now);
        sv_cat(t, a, sizeof t);
    } else {
        int p = (int)(now - (float)n), q = len / 4;
        sv_cpy(t, p < 2 * q ? "GROOVE" : p < 3 * q ? "BREAK" : "BUILD", sizeof t);
        sv_cat(t, ", THE DROP IN ", sizeof t);
        sv_fmt_int(a, len - p);
        sv_cat(t, a, sizeof t);
    }
    sv_label(10, ty, t, T_MID);
}
static void sv_arc_graph(sa_ui *u) { sv_arc_draw(u, 10, 58, 220, 34, 100); }

/* (0.8.47) HOME's VIEW (HOME again on HOME): a scope for each layer (LEAD CHORD BASS, the drums together), and under
 * them what comes by itself: ARC drawn small; else the next 16 bars (the phrases, where PART turns) and a line on
 * what is under way (STYLE MIX, INTRO, a LIVE build, A/B and EVOLVE, the phrase) */
static void sv_panel_view(sa_ui *u)
{
    const sa_groove *g = u->g;
    static const char *const NM[4] = {"LEAD", "CHORD", "BASS", "DRUMS"};
    static const uint8_t SL[4] = {SA_L_LEAD, SA_L_CHORD, SA_L_BASS, SA_L_DRUM0};
    for (int p = 0; p < 4; ++p) {
        int32_t x = 8 + p * 57, y = 16, w = 53, h = 40, my = y + h / 2;
        bool mute = p < 3 ? g->s.live[SL[p]].mute != 0 : g->s.dmute != 0;
        uint16_t col = mute ? T_DIM : p == 3 ? T_ACCENT : T_THEME;
        sv_label(x, 4, NM[p], u->scope_on[p] && !mute ? T_TEXT : T_DIM);
        cv_rect(x, y, w, h, sv_well());
        cv_rect(x, my, w, 1, T_LINE);
        if (!u->scope_on[p]) continue;
        int32_t py = my - u->scopes[p][0];
        for (int32_t i = 1; i < 54 && i < w; ++i) {
            int32_t yy = my - u->scopes[p][i];
            cv_line_t(x + i - 1, py, x + i, yy, col, 1);
            py = yy;
        }
    }
    if (g->s.arc) { sv_arc_draw(u, 10, 70, 220, 24, 102); return; }
    int left = 0;
    bool to_b = false;
    if (g->playing) left = sa_part_next(g, &to_b);
    int b0 = g->bar < 0 ? 0 : g->bar;
    for (int i = 0; i < 16; ++i) {                 /* the next 16 bars: this one lit, the phrases' starts taller, PART's
                                                    * turn in the second colour */
        int bar = b0 + i;
        bool ph = bar % 4 == 0, turn = left > 0 && i == left;
        uint16_t c = i == 0 && g->playing ? T_THEME : turn ? T_ACCENT : T_LINE;
        cv_rect(10 + i * 14, ph ? 70 : 74, 11, ph ? 18 : 14, turn ? T_ACCENT : c);
    }
    char t[40], a[8];
    int intro = sa_intro_bars(g->s.intro), nb = sa_build_peek(g);
    if (!g->playing) sv_cpy(t, "STOPPED: PLAY", sizeof t);
    else if (g->mx_state) sv_cpy(t, g->mx_state == 1 ? "STYLE MIX: FADING OUT" : "STYLE MIX: FADING IN", sizeof t);
    else if (g->bar < intro) {
        sv_cpy(t, "INTRO: ALL IN IN ", sizeof t);
        sv_fmt_int(a, intro - g->bar);
        sv_cat(t, a, sizeof t);
    } else if (nb >= 0) {
        sv_cpy(t, "LIVE: ", sizeof t);
        sv_cat(t, sa_build_name(g, nb), sizeof t);
        sv_cat(t, " NEXT", sizeof t);
    } else if (left > 0) {
        sv_cpy(t, to_b ? "PART B IN " : "PART A IN ", sizeof t);
        sv_fmt_int(a, left);
        sv_cat(t, a, sizeof t);
        if (g->s.evolve) sv_cat(t, g->s.evolve == 2 ? ", EVOLVE MORE" : ", EVOLVE", sizeof t);
    } else if (g->s.phrase == 0) {
        sv_cpy(t, "PHRASE: BAR ", sizeof t);
        sv_fmt_int(a, b0 % 4 + 1);
        sv_cat(t, a, sizeof t);
        sv_cat(t, " OF 4", sizeof t);
    } else {
        sv_cpy(t, "NOTHING CHANGES BY ITSELF", sizeof t);
    }
    sv_label(10, 102, t, T_MID);
}

/* the STYLE page: the style KNOB 1 picks, its tempo; INTRO and ARC */
static void sv_panel_style(sa_ui *u)
{
    const sa_settings *s = &u->g->s;
    int k = sv_clampi(u->style_pick, 0, SAS_NSTYLES - 1);
    char t[40], a[8];
    cv_free_text(10, 6, &AF_M, SAS_STYLE[k].name, T_THEME, T_SURF, 220);
    cv_free_text(10, 30, &AF_S, SAS_STYLE[k].say, T_TEXT, T_SURF, 220);
    sv_fmt_int(t, SAS_STYLE[k].bpm_lo);
    sv_cat(t, "-", sizeof t);
    sv_fmt_int(a, SAS_STYLE[k].bpm_hi);
    sv_cat(t, a, sizeof t);
    sv_cat(t, " BPM", sizeof t);
    sv_label(10, 46, t, T_MID);
    if (s->arc) { sv_arc_graph(u); return; }
    sv_label(10, 66, "SEQ: A FRESH GROOVE IN IT, ON THE 1", T_DIM);
    sv_label(10, 86, s->intro ? "INTRO: THE LAYERS COME IN FROM PLAY" : "INTRO OFF: ALL IN FROM PLAY", T_DIM);
    sv_label(10, 102, "ARC OFF: THE GROOVE AS IT IS", T_DIM);
}

void sv_song_default(sv_song *s)                   /* sets 1..4, 16 bars each */
{
    memset(s, 0, sizeof *s);
    for (int k = 0; k < 4; ++k) {
        s->set[k] = (uint8_t)(k + 1);
        s->bars[k] = 2;
    }
}
int sv_song_bars(int i) { static const uint8_t B[SV_SONG_BARS] = {4, 8, 16, 32, 64}; return B[sv_clampi(i, 0, SV_SONG_BARS - 1)]; }

/* STYLE MIX (0.8.25) */
#define SV_DRUM_BITS ((uint16_t)(((1u << SA_DROWS) - 1u) << SA_L_DRUM0))
#define SV_KICK_BIT ((uint16_t)(1u << (SA_L_DRUM0 + SA_DR_KICK)))
const char *sv_mix_keep_name(int k)
{
    static const char *const N[SV_MIX_KEEPS] = {"BASS", "KICK", "BASS+KICK", "DRUMS", "BASS+DRUMS", "NOTHING"};
    return N[sv_clampi(k, 0, SV_MIX_KEEPS - 1)];
}
uint16_t sv_mix_keep_mask(int k)
{
    const uint16_t B = (uint16_t)(1u << SA_L_BASS);
    static const uint16_t M[SV_MIX_KEEPS] = {B, SV_KICK_BIT, B | SV_KICK_BIT, SV_DRUM_BITS, B | SV_DRUM_BITS, 0};
    return M[sv_clampi(k, 0, SV_MIX_KEEPS - 1)];
}
int sv_mix_out_bars(int i) { static const uint8_t B[SV_MIX_OUTS] = {1, 2, 4}; return B[sv_clampi(i, 0, SV_MIX_OUTS - 1)]; }
int sv_mix_in_bars(int i) { static const uint8_t B[SV_MIX_INS] = {2, 4, 8, 16}; return B[sv_clampi(i, 0, SV_MIX_INS - 1)]; }
static void sv_bars_txt(char *t, int n)
{
    sv_fmt_int(t, n);
    sv_cat(t, n == 1 ? " BAR" : " BARS", 12);
}

/* STYLE MIX: from the style playing to the one picked; each layer's level in the mix (the kept ones in the accent),
 * where it is */
static void sv_panel_smix(sa_ui *u)
{
    const sa_groove *g = u->g;
    int k = sv_clampi(g->mx_state == 1 && g->mx_next ? g->mx_next->style : g->mx_state == 2 ? g->s.style : u->mix_pick,
                     0, SAS_NSTYLES - 1),
        from = sv_clampi(g->mx_state ? g->mx_from : g->s.style, 0, SAS_NSTYLES - 1);
    uint16_t keep = g->mx_state ? g->mx_keep : sv_mix_keep_mask(u->mix_keep);
    char t[40];
    sv_cpy(t, "FROM ", sizeof t);
    sv_cat(t, SAS_STYLE[from].name, sizeof t);
    sv_label(10, 4, t, T_MID);
    cv_free_text(10, 18, &AF_M, SAS_STYLE[k].name, T_THEME, T_SURF, 220);
    static const char *const N[4] = {"LEAD", "CHORD", "BASS", "DRUMS"};
    for (int i = 0; i < 4; ++i) {
        int32_t y = 44 + i * 11, x = 58, w = 150;
        float v = 1.0f;
        bool kept;
        if (i < 3) {
            v = g->mx_state ? g->mx_gain[i] : 1.0f;
            kept = (keep >> i) & 1u;
        } else {
            float sum = 0.0f;
            for (int r = 0; r < SA_DROWS; ++r) sum += g->mx_state ? g->mx_gain[SA_L_DRUM0 + r] : 1.0f;
            v = sum / (float)SA_DROWS;
            kept = (keep & SV_DRUM_BITS) != 0;
        }
        int f = (int)(v * (float)w + 0.5f);
        cv_free_text(10, y, &AF_S, i == 3 && kept && (keep & SV_DRUM_BITS) == SV_KICK_BIT ? "KICK" : N[i],
                     kept ? T_ACCENT : T_MID, T_SURF, 46);
        cv_rect(x, y + 2, w, 6, sv_well());
        cv_rect(x, y + 2, f > 0 ? f : 1, 6, kept ? T_ACCENT : T_THEME);
    }
    if (g->mx_state) {
        int n = g->mx_state == 1 ? g->mx_out : g->mx_in;
        float x = sa_mix_pos(g);
        sv_cpy(t, g->mx_state == 1 ? (g->bar < g->mx_bar0 ? "OUT ON THE 1" : "FADING OUT, ") : "FADING IN, ", sizeof t);
        if (g->mx_state == 2 || g->bar >= g->mx_bar0) {
            char a[12];
            sv_fmt_int(a, (int)(x * (float)n) + 1 > n ? n : (int)(x * (float)n) + 1);
            sv_cat(t, "BAR ", sizeof t);
            sv_cat(t, a, sizeof t);
            sv_cat(t, " OF ", sizeof t);
            sv_fmt_int(a, n);
            sv_cat(t, a, sizeof t);
        }
        sv_label(10, 92, t, T_ACCENT);
        cv_rect(10, 108, 220, 4, sv_well());
        int done = (int)((g->mx_state == 1 ? x * 0.5f : 0.5f + x * 0.5f) * 220.0f);
        cv_rect(10, 108, done > 0 ? done : 1, 4, T_ACCENT);
    } else {
        sv_label(10, 92, g->playing ? "SEQ: MIX INTO IT, FROM THE 1" : "SEQ: IT PLAYS, FADING IN", T_DIM);   /* (1.0.2) */
        sv_label(10, 106, "THE KEPT ONES PLAY ON THROUGH", T_DIM);
    }
}

/* LIVE: the build, in its order: the layers in lit, the next in the accent, the rest dim; before one, how it goes */
static void sv_panel_live(sa_ui *u)
{
    const sa_groove *g = u->g;
    if (!g->build_n) {
        sv_label(10, 8, "A MIX BUILT BY HAND:", T_MID);
        sv_label(10, 26, "SEQ HERE SETS IT UP: THE START IN,", T_DIM);
        sv_label(10, 40, "THE REST WAITING. PLAY, THEN SEQ", T_DIM);
        sv_label(10, 54, "ON ANY PAGE: THE NEXT LAYER IN,", T_DIM);
        sv_label(10, 68, "ON THE 1 (OR BUILD: BY ITSELF)", T_DIM);
        return;
    }
    int next = sa_build_peek(g);
    for (int i = 0; i < g->build_n && i < 10; ++i) {
        int l = g->build_order[i];
        int32_t x = 10 + (i / 5) * 112, y = 6 + (i % 5) * 16;
        bool in = !((g->build_out >> l) & 1u);
        char t[4];
        sv_fmt_int(t, i + 1);
        cv_text_on(x, y, &AF_S, t, T_DIM, T_SURF);
        cv_text_on(x + 18, y, &AF_S, sa_build_name(g, l), l == next ? T_ACCENT : in ? T_THEME : T_DIM, T_SURF);
        if (in) cv_rrect(x + 92, y + 1, 7, 7, 3, T_THEME, T_SURF);
    }
    sv_label(10, 92, next < 0 ? "ALL IN" : "SEQ: THE NEXT IN, ON THE 1", next < 0 ? T_THEME : T_DIM);
}

/* SONG: the eight steps, two columns; the one picked lit, the one playing in the accent */
static void sv_panel_song(sa_ui *u)
{
    for (int k = 0; k < SV_STEPS; ++k) {
        int32_t x = 6 + (k / 4) * 116, y = 6 + (k % 4) * 20;
        bool sel = k == u->song_pick, play = k == u->song_step && u->song.on;
        uint16_t bg = sel ? ux_mix(T_SURF, T_THEME, 18) : T_SURF;
        if (sel) cv_rrect(x - 2, y - 2, 112, 18, 2, bg, T_SURF);
        char t[16], a[8];
        sv_fmt_int(t, k + 1);
        cv_text_on(x + 2, y + 2, &AF_S, t, T_DIM, bg);
        int st = u->song.set[k];
        if (st) {
            sv_cpy(t, "SET ", sizeof t);
            t[4] = (char)('0' + st);
            t[5] = 0;
            bool used = u->sets[st - 1].used || u->sets[st - 1].wait;
            cv_text_on(x + 18, y + 2, &AF_S, used ? t : "EMPTY", play ? T_ACCENT : used ? T_TEXT : T_DIM, bg);
            sv_fmt_int(a, sv_song_bars(u->song.bars[k]));
            cv_text_on(x + 78, y + 2, &AF_S, a, play ? T_ACCENT : T_MID, bg);
        } else {
            cv_text_on(x + 18, y + 2, &AF_S, "END", T_DIM, bg);
        }
    }
    sv_label(10, 92, u->song.on ? "PLAY: THE STEPS IN TURN, ROUND" : "SONG ON: THE SETS IN TURN", T_DIM);
    sv_label(10, 108, "EMPTY AND END ONES ARE PASSED", T_DIM);
}

static void sv_panel_mixer(sa_ui *u)
{
    if (u->mixmute == 6) { sv_panel_smix(u); return; }
    if (u->mixmute == 5) { sv_panel_song(u); return; }
    if (u->mixmute == 4) { sv_panel_live(u); return; }
    if (u->mixmute == 3) { sv_panel_style(u); return; }
    const sa_groove *g = u->g;
    const sa_settings *s = &g->s;
    static const char *const N[4] = {"LEAD", "CHORD", "BASS", "DRUMS"};
    int now = sv_now(g);
    for (int i = 0; i < 4; ++i) {                  /* a row each: the name, the level across, the beat's dot */
        int32_t y = 5 + i * 13, x = 58, w = 150;
        bool m = (i < 3 ? s->live[i].mute : s->dmute) || (g->solo && !((g->solo >> i) & 1u));
        int v = i < 3 ? s->live[i].level : s->dvol, f = v * w / 100;
        bool hit = false, so = (g->solo >> i) & 1u;
        if (now >= 0 && i < 3) hit = (g->hits[i] >> now) & 1u;
        for (int l = SA_L_DRUM0; i == 3 && now >= 0 && l < SA_LAYERS; ++l) hit |= (g->hits[l] >> now) & 1u;
        cv_free_text(10, y, &AF_S, so ? "SOLO" : m ? "MUTED" : N[i], so || m ? T_ACCENT : T_MID, T_SURF, 46);
        cv_rect(x, y + 2, w, 6, sv_well());
        cv_rect(x, y + 2, f > 0 ? f : 1, 6, m ? T_DIM : i == 3 ? T_THEME : T_ACCENT);
        cv_rect(x + 80 * w / 100, y, 1, 10, T_MID);          /* (80: as it is) */
        cv_rrect(218, y + 1, 8, 8, 4, hit && !m ? T_TEXT : sv_well(), T_SURF);
    }
    sv_scope(u, 8, 92, 224, 24, T_THEME);          /* the whole mix */
}

/* EDIT, a drum: its 16 steps large, the step KNOB 1 is on framed */
static void sv_panel_edit_drum(sa_ui *u)
{
    sa_groove *g = u->g;
    int l = u->layer, now = sv_now(g), st = sv_clampi(u->scur, 0, 15);
    const sa_code *e = &g->ed[l];
    bool pend = sa_layer_pending(g, l);
    uint16_t bits = e->type == SA_T_DRUM ? sv_ed_bits(u, l) : 0;
    cv_free_text(10, 6, &AF_M, sv_layer_name(u, l), pend ? T_ACCENT : T_THEME, T_SURF, 150);
    for (int k = 0; k < 16; ++k) {
        int32_t x = 8 + k * 14;
        bool on = (bits >> k) & 1u;
        uint16_t c = on ? (pend ? T_ACCENT : T_THEME) : sv_well();
        if (k == now) c = on ? T_TEXT : ux_mix(T_SURF, T_TEXT, 30);
        if (k == st) cv_rrect(x - 2, 34, 16, 48, 3, T_TEXT, T_SURF);
        cv_rrect(x, 36, 12, 44, 3, c, T_SURF);
        if (k % 4 == 0) cv_rect(x, 86, 12, 2, T_DIM);
    }
    sv_label(10, 98, "KEYS: THE 16 LOWEST ARE THE STEPS", T_ACCENT);
}

static void sv_draw_panel(sa_ui *u)
{
    sv_panel_begin();
    switch (u->page) {
    case SV_PG_MAIN: sv_panel_home(u); break;
    case SV_PG_LAYER: sv_panel_layer(u); break;
    case SV_PG_KEY: sv_panel_key(u); break;
    case SV_PG_SETS: sv_panel_sets(u); break;
    case SV_PG_MIXER: sv_panel_mixer(u); break;
    case SV_PG_EDIT: if (u->view == SV_V_DRUMS) sv_panel_edit_drum(u); else sv_panel_steps(u); break;
    default: sv_panel_menu(u); break;
    }
    uint32_t sig = sv_hashb(2166136261u, cv_px, cv_w * cv_h * 2u);   /* (the same as on the screen: not sent again) */
    if (!u->force && sig == u->sig_panel)
        return;
    u->sig_panel = sig;
    cv_blit(0, SV_PANEL_Y);
}

static void sv_background(void)
{
    cv_begin(240, 40, T_BG);
    for (int y = 0; y < 240; y += 40)
        cv_blit(0, (uint32_t)y);
    if (ux.style == ST_LINE) {
        cv_begin(240, 1, T_RULE);
        cv_blit(0, SV_HEAD + 1);
        cv_blit(0, SV_CARD_Y + SV_CARD_H + 1);
        cv_blit(0, SV_FOOT_Y - 3);
        cv_begin(1, SV_CARD_H, T_RULE);
        for (int c = 1; c < 4; ++c)
            cv_blit((uint32_t)(SV_CARD_X(c) - 2), SV_CARD_Y);
    }
}

void sv_draw(sa_ui *u)
{
    if (u->page == SV_PG_LAYER && u->view == SV_V_DRUMS) sv_drum_sync(u);
    if (u->bg_gen != ux.gen * 2u + ux.style + 1u) {   /* the background only for a new look: a page change draws its
                                                      * parts over what was there, no blank in between */
        sv_background();
        u->bg_gen = ux.gen * 2u + ux.style + 1u;
        u->force = true;
    }
    sv_draw_head(u);
    sv_card c[4];
    sv_cards_of(u, c);
    for (int i = 0; i < 4; ++i)
        sv_draw_card(u, i, &c[i]);
    sv_draw_panel(u);
    sv_draw_foot(u);
    u->force = false;
    u->frame++;
}

void sv_draw_all(sa_ui *u)
{
    u->force = true;
    sv_draw(u);
}

int sv_page_button(const sa_ui *u)
{
    switch (u->page) {
    case SV_PG_MAIN: return SV_B_HOME;
    case SV_PG_KEY: return SV_B_SCL;
    case SV_PG_SETS: return SV_B_SAVE;
    case SV_PG_MIXER: return SV_B_GLO;
    case SV_PG_EDIT: return SV_B_EDIT;
    case SV_PG_LAYER: return u->fx > 0 ? SV_B_FX : -1;
    default: return -1;
    }
}

/* ---- input ------------------------------------------------------------------------------------------------------ */
static void sv_msg(sa_ui *u, const char *m)
{
    sv_cpy(u->msg, m, sizeof u->msg);
    u->msg_ms = 1500;
}

static void sv_goto(sa_ui *u, int page)
{
    if (page != u->page) {
        u->page = page;
        u->force = true;
    }
}
/* a layer (or HOME), on its home page, step recording off */
static void sv_view(sa_ui *u, int v)
{
    v = sv_clampi(v, 0, SV_VIEWS - 1);
    u->view = v;
    u->fx = 0;
    u->rec = false;
    u->force = true;
    u->home2 = 0;
    if (v == SV_V_HOME) {
        u->page = SV_PG_MAIN;
        u->layer = -1;
        return;
    }
    u->page = SV_PG_LAYER;
    if (v == SV_V_DRUMS) sv_drum_sync(u);
    else u->layer = sv_slot_of_view(v);
}

static void sv_apply_style(sa_ui *u)
{
    ux.style = (uint8_t)(u->g->s.look ? ST_LINE : ST_FLAT);
    palette_set((uint32_t)u->g->s.palette % NPALETTES);
    u->force = true;
}

void sv_init(sa_ui *u, sa_groove *g)
{
    memset(u, 0, sizeof *u);
    u->g = g;
    u->page = SV_PG_MAIN;
    u->layer = -1;
    u->hot = -1;
    u->batt = 3;
    u->pv_layer = -1;
    u->req_act = -1;
    u->song_step = -1;
    u->live_start = SA_START_KICK;
    u->mix_out = 1;                                /* (STYLE MIX: 2 bars out, 4 in, the BASS kept) */
    u->mix_in = 1;
    u->midi_ch[0] = 1, u->midi_ch[1] = 2, u->midi_ch[2] = 3, u->midi_ch[3] = 10;
    sv_song_default(&u->song);
    for (int k = 0; k < 27; ++k)
        u->key_sent[k] = -1;
    memset(u->main_code, 0xFF, sizeof u->main_code);
    sv_apply_style(u);
}

/* EDIT's FILLs: ready steps the pattern becomes (in the copy, waits for SEQ). A drum: hits over the bar as it was
 * (OR: its own kept) or in place of it; a line: notes on scale degrees (7: the octave up); CHORD: the chord's steps */
typedef struct { const char *name; uint16_t mask; uint8_t keep; } sv_dfill;
static const sv_dfill SV_DFILL[] = {
    {"ROLL", 0xF000, 1},                  /* the last beat in sixteenths */
    {"BUILD", 0xF511, 0},                 /* 1 5 9 11 13 14 15 16: closing in */
    {"SNARE", 0xE110, 0},                 /* 5 9 13 14 15 16 */
    {"BREAK", 0x00FF, 2},                 /* the first half only */
    {"STUTTER", 0x0F00, 1},               /* the third beat in sixteenths */
    {"GALLOP", 0xDDDD, 0},                /* 1 3 4 per beat */
    {"FLAM", 0x8000, 1},                  /* one more on the last sixteenth */
    {"DOUBLE", 0xFFFF, 3},                /* every hit twice: on the next sixteenth too */
};
#define SV_NDFILL ((int)(sizeof SV_DFILL / sizeof SV_DFILL[0]))
typedef struct { const char *name; uint8_t n; struct { uint8_t st, len; int8_t deg; } s[16]; } sv_mfill;
static const sv_mfill SV_MFILL[] = {
    {"ROOT 8THS", 8, {{0, 2, 0}, {2, 2, 0}, {4, 2, 0}, {6, 2, 0}, {8, 2, 0}, {10, 2, 0}, {12, 2, 0}, {14, 2, 0}}},
    {"ARP UP", 8, {{0, 2, 0}, {2, 2, 2}, {4, 2, 4}, {6, 2, 7}, {8, 2, 0}, {10, 2, 2}, {12, 2, 4}, {14, 2, 7}}},
    {"ARP DOWN", 8, {{0, 2, 7}, {2, 2, 4}, {4, 2, 2}, {6, 2, 0}, {8, 2, 7}, {10, 2, 4}, {12, 2, 2}, {14, 2, 0}}},
    {"OCTAVES", 8, {{0, 2, 0}, {2, 2, 7}, {4, 2, 0}, {6, 2, 7}, {8, 2, 0}, {10, 2, 7}, {12, 2, 0}, {14, 2, 7}}},
    {"WALK", 4, {{0, 4, 0}, {4, 4, 1}, {8, 4, 2}, {12, 4, 4}}},
    {"RIFF", 6, {{0, 2, 0}, {3, 1, 0}, {6, 2, 2}, {8, 2, 4}, {11, 1, 3}, {14, 2, 2}}},
    {"CALL", 4, {{0, 3, 4}, {4, 2, 2}, {6, 2, 0}, {12, 3, 1}}},
    {"PULSE 16", 16, {{0, 1, 0}, {1, 1, 0}, {2, 1, 0}, {3, 1, 0}, {4, 1, 0}, {5, 1, 0}, {6, 1, 0}, {7, 1, 0},
                     {8, 1, 0}, {9, 1, 0}, {10, 1, 0}, {11, 1, 0}, {12, 1, 0}, {13, 1, 0}, {14, 1, 0}, {15, 1, 0}}},
};
#define SV_NMFILL ((int)(sizeof SV_MFILL / sizeof SV_MFILL[0]))
typedef struct { const char *name; uint16_t mask; uint8_t len; } sv_cfill;
static const sv_cfill SV_CFILL[] = {
    {"STABS", 0x0408, 2}, {"OFFBEATS", 0x4444, 2}, {"PUSH", 0x4001, 3}, {"PULSE", 0x1111, 2}, {"8THS", 0x5555, 1},
    {"16THS", 0xFFFF, 1}, {"DOTTED", 0x1249, 2}, {"HOLD", 0x0001, 16},
};
#define SV_NCFILL ((int)(sizeof SV_CFILL / sizeof SV_CFILL[0]))
static int sv_fills(const sa_ui *u) { return 1 + (u->view == SV_V_DRUMS ? SV_NDFILL : u->view == SV_V_CHORD ? SV_NCFILL : SV_NMFILL); }
static const char *sv_fill_name(const sa_ui *u, int f)
{
    if (f <= 0) return "CURR";
    return u->view == SV_V_DRUMS ? SV_DFILL[(f - 1) % SV_NDFILL].name : u->view == SV_V_CHORD ? SV_CFILL[(f - 1) % SV_NCFILL].name
                                                                    : SV_MFILL[(f - 1) % SV_NMFILL].name;
}
static void sv_fill_apply(sa_ui *u)
{
    sa_groove *g = u->g;
    int l = u->layer;
    sa_code *e = &g->ed[l];
    *e = u->fill_orig;                             /* (CURR: as it was) */
    if (u->fill <= 0) return;
    sa_code o = u->fill_orig;
    sa_own_make(g, &o);                            /* (its steps as they were) */
    if (u->view == SV_V_DRUMS) {
        const sv_dfill *f = &SV_DFILL[(u->fill - 1) % SV_NDFILL];
        uint16_t h = o.hits;
        if (f->keep == 1) h |= f->mask;
        else if (f->keep == 2) h &= f->mask;
        else if (f->keep == 3) h |= (uint16_t)(h << 1);
        else h = f->mask;
        o.hits = h;
        o.acc &= h;
    } else if (u->view == SV_V_CHORD) {
        const sv_cfill *f = &SV_CFILL[(u->fill - 1) % SV_NCFILL];
        for (int k = 0; k < 16; ++k) { o.sn[k] = SA_NO_NOTE; o.sl[k] = f->len; }
        o.chord = f->mask;
        o.acc = 0;
    } else {
        const sv_mfill *f = &SV_MFILL[(u->fill - 1) % SV_NMFILL];
        int base = 12 * ((u->view == SV_V_BASS ? 2 : 5) + 1);   /* (above the key's C: bass in 2, lead in 5) */
        for (int k = 0; k < 16; ++k) { o.sn[k] = SA_NO_NOTE; o.sl[k] = 1; }
        o.chord = o.acc = 0;
        for (int i = 0; i < f->n; ++i) {
            o.sn[f->s[i].st] = (int8_t)(base + sa_degree(&g->ed_harm, f->s[i].deg));
            o.sl[f->s[i].st] = f->s[i].len;
        }
    }
    o.own = 1;
    *e = o;
}

/* ALGORITHM on a layer: its pattern (OFF first; in the copy, waits for SEQ) */
/* the pattern ALGORITHM turned to, briefly in the header (B: B's), whatever the page shows */
static void sv_algo_say(sa_ui *u, uint8_t pat, bool b)
{
    char t[24];
    sa_code c = u->g->ed[u->layer];
    c.pat = pat;
    c.own = 0;
    if (pat == SA_PAT_OFF) sv_cpy(t, "OFF", sizeof t);
    else sa_pat_label(&c, t, sizeof t);
    if (b) {
        char t2[24] = "B: ";
        sv_cat(t2, t, sizeof t2);
        sv_msg(u, t2);
    } else {
        sv_msg(u, t);
    }
}

static void sv_algo(sa_ui *u, int d)
{
    sa_groove *g = u->g;
    if (u->layer < 0) return;
    sa_code *e = sv_ed(u);
    uint8_t list[64];
    int n = sv_pat_list(u, list), at = -1;
    if (g->part_cue && e->type != SA_T_NONE && !g->s.live[u->layer].hand) {   /* PART B: the layer's B pattern
                                                    * (SEQ drops it; MANUAL: its one pattern) */
        uint8_t *b = &g->ed_patb[u->layer];
        int cur = *b == 0 ? (e->own ? -1 : e->pat) : *b == SA_PATB_OFF ? SA_PAT_OFF : *b - 1;
        at = 0;
        for (int i = 0; i < n; ++i) if (list[i] == cur) at = i;
        int to = sv_clampi(at + d, 0, n - 1);
        *b = list[to] == SA_PAT_OFF ? SA_PATB_OFF : (uint8_t)(list[to] + 1);
        u->changed = true;
        sv_algo_say(u, list[to], true);
        return;
    }
    if (e->type == SA_T_NONE) at = -1;
    else for (int i = 0; i < n; ++i) if (list[i] == e->pat) at = i;
    if (at < 0 && e->type != SA_T_NONE) at = 0;
    int to = at + d, lo = 0;
    to = sv_clampi(to, lo, n - 1);
    if (to < 0) {                                  /* REMOVE (a drum): it goes with SEQ */
        e->type = SA_T_NONE;
    } else {
        if (e->type == SA_T_NONE) *e = g->s.code[u->layer];
        e->pat = list[to];
        e->own = 0;
    }
    u->changed = true;
    if (to >= 0) sv_algo_say(u, list[to], false);
}

/* SOUND's TYPE and SOUND: at once */
static void sv_sound_turn(sa_ui *u, int knob, int d)
{
    sa_groove *g = u->g;
    int l = u->layer, cur = g->s.code[l].sound, role = sas_of(cur)->role % SAS_ROLES;
    uint8_t list[SAS_NSOUNDS > 0 ? SAS_NSOUNDS : 1];
    if (knob == 0) {
        role = sv_clampi(role + d, 0, SAS_ROLES - 1);
        if (role == sas_of(cur)->role) return;
        int n = sv_sounds_of(role, list);
        if (n) sa_sound_now(g, l, list[0]);
    } else {
        int n = sv_sounds_of(role, list), at = 0;
        for (int i = 0; i < n; ++i) if (list[i] == cur) at = i;
        at = sv_clampi(at + d, 0, n - 1);
        if (n && list[at] != cur) sa_sound_now(g, l, list[at]);
    }
    if (g->s.code[l].sound != cur) {               /* another sound: its name, briefly, in the header */
        char m[32];
        sv_cpy(m, SV_VIEW_NAME[u->view], sizeof m);
        sv_cat(m, ": ", sizeof m);
        sv_cat(m, sas_of(g->s.code[l].sound)->name, sizeof m);
        sv_msg(u, m);
    }
    if (g->s.code[l].sound != cur && g->s.code[l].part < SA_SYNTHS) {   /* another sound: its type's pump */
        g->s.pl[g->s.code[l].part].depth = sas_pump_of(g->s.code[l].sound);
        g->s.pl[g->s.code[l].part].on = 1;
    }
}

void sv_knob(sa_ui *u, int k, int d)
{
    sa_groove *g = u->g;
    sa_settings *s = &g->s;
    if (k == SV_K_SELECT) {                        /* the tempo, everywhere */
        s->bpm = (uint8_t)sv_clampi(s->bpm + d, 60, 200);
        u->hot = 8;
        u->hot_ms = 700;
        u->changed = true;
        return;
    }
    if (k == SV_K_PRESETS) {
        if (u->page == SV_PG_SETS) {
            u->set_slot = sv_clampi(u->set_slot + d, 0, SV_SLOTS - 1);
            return;
        }
        bool here = u->page == SV_PG_LAYER || u->page == SV_PG_MAIN;
        int v = here ? u->view : SV_V_HOME, to = sv_clampi(v + d, 0, SV_VIEWS - 1);
        if (to != v || !here) sv_view(u, to);     /* (at either end: nothing, no redraw) */
        return;
    }
    if (k == SV_K_ALGO) {
        if (u->page == SV_PG_LAYER) sv_algo(u, d);
        else if (u->page == SV_PG_MAIN && (d > 0) != g->part_cue) {   /* HOME: PART A or B, on the next phrase */
            sa_part(g, d > 0);
            sv_msg(u, g->playing ? (d > 0 ? "PART B ON THE PHRASE" : "PART A ON THE PHRASE") : d > 0 ? "PART B" : "PART A");
            u->force = true;
        }
        return;
    }
    if (k < SV_K1 || k > SV_K4) return;
    int i = k - SV_K1;
    {   /* (0.8.34) a knob over an empty card (a dash) does nothing: no card lit, no redraw */
        sv_card c[4];
        sv_cards_of(u, c);
        if (c[i].off) return;
    }
    u->hot = i;
    u->hot_ms = 700;
    switch (u->page) {
    case SV_PG_MAIN:
        u->changed = true;
        if (u->home2 == 1) {
            if (i == 0) s->mreso = (uint8_t)sv_clampi(s->mreso + d * 2, 0, 100);
            else if (i == 1) s->width = (uint8_t)sv_clampi(s->width + d * 2, 0, 100);
            return;
        }
        if (u->home2 == 2) {
            if (i == 0) s->phrase = (uint8_t)(d > 0 ? 0 : 1);
            else if (i == 1) s->abauto = (uint8_t)sv_clampi(s->abauto + d, 0, SA_ABAUTOS - 1);
            else if (i == 2) s->evolve = (uint8_t)sv_clampi(s->evolve + d, 0, SA_EVOLVES - 1);
            return;
        }
        if (i == 0) s->filter = (uint8_t)sv_clampi(s->filter + d * 2, 0, 100);
        else if (i == 1) s->feel = (uint8_t)sv_clampi(s->feel + d * 2, 0, 60);
        else if (i == 2) s->pump = (uint8_t)sv_clampi(s->pump + d * 2, 0, 100);
        else s->reverb = s->delay = (uint8_t)sv_clampi(s->reverb + d * 2, 0, 100);
        return;
    case SV_PG_KEY: {
        sa_harm *h = &g->ed_harm;
        u->changed = true;
        if (i == 0) h->key = (uint8_t)sv_wrap(h->key + d, 12);
        else if (i == 1) h->scale = (uint8_t)sv_clampi(h->scale + d, 0, SA_SCALES - 1);
        else if (i == 2 && g->part_cue)
            g->ed_progb = (uint8_t)(sv_wrap((g->ed_progb ? g->ed_progb - 1 : h->prog) + d, SA_PROGS) + 1);
        else if (i == 2) h->prog = (uint8_t)sv_wrap(h->prog + d, SA_PROGS);
        else { static const uint8_t BARS[3] = {1, 2, 4}; int b = h->bars >= 4 ? 2 : h->bars >= 2 ? 1 : 0;
               h->bars = BARS[sv_clampi(b + d, 0, 2)]; }
        return;
    }
    case SV_PG_SETS:
        if (i == 0) u->set_slot = sv_clampi(u->set_slot + d, 0, SV_SLOTS - 1);
        else if (i == 1) u->set_act = sv_clampi(u->set_act + d, 0, SV_SA_ACTS - 1);
        return;
    case SV_PG_MIXER:
        u->changed = true;
        if (u->mixmute == 6) {                     /* STYLE MIX */
            if (i == 0) u->mix_pick = sv_clampi(u->mix_pick + d, 0, SAS_NSTYLES - 1);
            else if (i == 1) u->mix_keep = sv_clampi(u->mix_keep + d, 0, SV_MIX_KEEPS - 1);
            else if (i == 2) u->mix_out = sv_clampi(u->mix_out + d, 0, SV_MIX_OUTS - 1);
            else u->mix_in = sv_clampi(u->mix_in + d, 0, SV_MIX_INS - 1);
            return;
        }
        if (u->mixmute == 4) {                     /* LIVE */
            if (i == 0) u->style_pick = sv_clampi(u->style_pick + d, 0, SAS_NSTYLES - 1);
            else if (i == 1) u->live_start = sv_clampi(u->live_start + d, 1, SA_STARTS - 1);
            else if (i == 2) {
                u->live_build = sv_clampi(u->live_build + d, 0, 2);
                g->build_auto = (uint8_t)(u->live_build == 1 ? 8 : u->live_build == 2 ? 16 : 0);
            }
            return;
        }
        if (u->mixmute == 5) {                     /* SONG */
            int k = sv_clampi(u->song_pick, 0, SV_STEPS - 1);
            if (i == 0) u->song_pick = sv_clampi(k + d, 0, SV_STEPS - 1);
            else if (i == 1) u->song.set[k] = (uint8_t)sv_clampi(u->song.set[k] + d, 0, SV_SETS);
            else if (i == 2) u->song.bars[k] = (uint8_t)sv_clampi(u->song.bars[k] + d, 0, SV_SONG_BARS - 1);
            else u->song.on = (uint8_t)(d > 0);
            return;
        }
        if (u->mixmute == 3) {                     /* STYLE (SEQ makes it), INTRO, ARC */
            if (i == 0) u->style_pick = sv_clampi(u->style_pick + d, 0, SAS_NSTYLES - 1);
            else if (i == 1) s->intro = (uint8_t)sv_clampi(s->intro + d, 0, SA_INTROS - 1);
            else if (i == 2) s->arc = (uint8_t)sv_clampi(s->arc + d, 0, SA_ARCS - 1);
            return;
        }
        if (u->mixmute == 2) {                     /* SOLO: right on, left off (not kept: a live move) */
            if (d > 0) g->solo |= (uint8_t)(1u << i);
            else g->solo &= (uint8_t)~(1u << i);
        } else if (u->mixmute) {
            if (i < 3) sa_mute(g, i, d > 0);
            else s->dmute = (uint8_t)(d > 0);
        } else {
            uint8_t *f = i < 3 ? &s->live[i].level : &s->dvol;
            *f = (uint8_t)sv_clampi(*f + d * 2, 0, 100);
        }
        return;
    case SV_PG_MENU:
        if (u->menu2) {
            u->midi_ch[i] = (uint8_t)sv_clampi(u->midi_ch[i] + d, 0, 16);
            u->changed = true;
            return;
        }
        if (i == 0) s->palette = (uint8_t)sv_wrap(s->palette + d, (int)NPALETTES);
        else if (i == 1) s->look = (uint8_t)!s->look;
        else if (i == 2) {                         /* SNAP: the keys to the scale, or as pressed */
            u->g->snap = d > 0;
            sv_msg(u, u->g->snap ? "KEYS SNAP TO THE SCALE" : "KEYS AS PRESSED");
            u->changed = true;
            return;
        } else if (i == 3) {                       /* (1.0.2) CLOCK: Sardine's own (sent out), or MIDI in's */
            u->clock_ext = d > 0;
            sv_msg(u, u->clock_ext ? "CLOCK: FOLLOWS MIDI IN" : "CLOCK: SARDINE'S, SENT OUT");
            u->changed = true;
            return;
        }
        u->changed = true;
        sv_apply_style(u);
        return;
    default: break;
    }
    /* a layer */
    if (u->page == SV_PG_LAYER && u->view == SV_V_DRUMS && i == 0 && sv_page_now(u) == SVP_HOME) {   /* DRUM: which row */
        u->drow = sv_clampi(u->drow + d, 0, sv_drow_count(u) - 1);
        sv_drum_sync(u);
        return;
    }
    int l = u->layer;
    if (l < 0) return;
    sa_live *v = &s->live[l];
    sa_code *e = &g->ed[l];
    u->changed = true;
#define SV_ADJ(f, step, lo, hi) (f) = (uint8_t)sv_clampi((f) + d * (step), (lo), (hi))
    if (u->page == SV_PG_EDIT && i == (u->view == SV_V_DRUMS ? 1 : 3)) {   /* EDIT: FILL (CURR, the ready fills; a synth
                                                    * layer's on KNOB 4, 0.8.48) */
        u->fill = sv_clampi(u->fill + d, 0, sv_fills(u) - 1);
        sv_fill_apply(u);
        return;
    }
    if (u->page == SV_PG_EDIT && u->view == SV_V_DRUMS) {   /* EDIT, a drum: KNOB 1 the step, KNOB 3 its hit */
        if (i == 0) u->scur = sv_wrap(u->scur + d, 16);
        else if (i == 2 && g->ed[l].type == SA_T_DRUM) {
            int st = sv_clampi(u->scur, 0, 15);
            bool on = (sv_ed_bits(u, l) >> st) & 1u;
            if (on != (d > 0)) sa_step_drum(g, l, st);
        }
        else if (i == 3) {                         /* SOLO: the drum edited alone (EDIT only: off on leaving it) */
            g->ed_solo = d > 0 ? (uint8_t)(l + 1) : 0;
            sv_msg(u, g->ed_solo ? "SOLO: THIS DRUM ALONE" : "SOLO OFF");
        }
        return;
    }
    if (sv_steps_on(u)) {                          /* (0.8.48) KNOB 1 the step, KNOB 2 its length, KNOB 3 its note */
        if (i == 0) u->scur = sv_wrap(u->scur + d, 16);
        else if (i == 1) sa_step_len(g, l, sv_clampi(u->scur, 0, 15), d);
        else if (i == 2) sa_step_turn(g, l, sv_clampi(u->scur, 0, 15), d);   /* NOTE */
        u->changed = true;
        return;
    }
    switch (sv_page_now(u)) {
    case SVP_HOME:
        if (u->view == SV_V_DRUMS) {
            if (i == 1) sv_instr_turn(u, d);
            else if (i == 2) SV_ADJ(v->level, 2, 0, 100);
            else SV_ADJ(v->env, 2, 0, 100);
        } else {
            uint8_t *f = i == 0 ? &v->cut : i == 1 ? &v->reso : i == 2 ? &v->drive : &v->env;
            SV_ADJ(*f, 2, 0, 100);
        }
        break;
    case SVP_ARP:
        if (i == 0 && e->amode == SA_AR_OFF && d > 0) {   /* ARP on: PINKY, 1/8, one octave, gate 70 */
            e->amode = SA_AR_PINKY;
            e->arate = SA_RATE_8;
            e->arange = 1;
            e->agate = 70;
        } else if (i == 0) {
            e->amode = (uint8_t)sv_clampi(e->amode + d, 0, SA_AR_MODES - 1);
        }
        else if (i == 1) e->arate = (uint8_t)sv_clampi(e->arate + d, 0, SA_RATES - 1);
        else if (i == 2) e->arange = (uint8_t)sv_clampi(e->arange + d, 1, 4);
        else SV_ADJ(e->agate, 5, 10, 100);
        break;
    case SVP_PAN: {
        sa_pan *p = &s->pan[l];
        if (sv_pan3d_on(u)) { sv_msg(u, "PAN 3D IS ON: OFF FIRST"); break; }
        if (i == 0 && p->mode == SA_PAN_FIXED && d > 0) {   /* moving: SWEEP, from the centre, all the way, a bar */
            p->mode = SA_PAN_SWEEP;
            p->pos = 0;
            p->width = 100;
            p->rate = SA_PRATE_BAR;
        } else if (i == 0) p->mode = (uint8_t)sv_clampi(p->mode + d, 0, SA_PAN_MODES - 1);
        else if (i == 1) p->pos = (int8_t)sv_clampi(p->pos + d * 2, -50, 50);
        else if (i == 2) SV_ADJ(p->width, 5, 0, 100);
        else p->rate = (uint8_t)sv_clampi(p->rate + d, 0, SA_PRATES - 1);
        break;
    }
    case SVP_3D: {                                 /* (turning ANGLE HEIGHT DISTANCE turns it on) */
        int pp = sv_clampi(e->part, 0, SA_SYNTHS - 1);
        sa_3d *d3 = &s->d3[pp];
        if (sv_pan_on(u)) { sv_msg(u, "PAN IS ON: FIXED C FIRST"); break; }
        if (!d3->on && (i > 0 || d > 0)) {         /* on: ORBIT round you over 2 bars, level, near (10) */
            d3->on = SA_3D_ORBIT;
            s->d3rate[pp] = SA_PRATE_2BARS;
            d3->el = 0;
            d3->dist = 10;
            if (i == 0) break;
        }
        if (i == 0) { d3->on = (uint8_t)sv_clampi(d3->on + d, 0, SA_3D_MODES - 1); break; }
        if (i == 1 && (d3->on == SA_3D_ORBIT || d3->on == SA_3D_FLY))
            s->d3rate[pp] = (uint8_t)sv_clampi(s->d3rate[pp] + d, 0, SA_PRATES - 1);
        else if (i == 1) d3->az = (uint8_t)sv_wrap(d3->az + d, SA_AZ_STEPS);
        else if (i == 2) d3->el = (int8_t)sv_clampi(d3->el + d * 5, -50, 50);
        else SV_ADJ(d3->dist, 5, 0, 100);
        break;
    }
    case SVP_PUMP: {
        sa_pumpl *q = &s->pl[sv_clampi(e->part, 0, SA_SYNTHS - 1)];
        if (i == 0) SV_ADJ(q->depth, 5, 0, 100);
        else if (i == 1) SV_ADJ(q->rel, 5, 0, 100);
        else if (i == 2) q->src = (uint8_t)(d > 0 ? SA_PS_BEAT : SA_PS_KICK);
        else q->on = (uint8_t)(d > 0);
        if (i < 2) q->on = 1;
        break;
    }
    case SVP_GATE: {
        sa_gate *gt = &g->ed_gate[l];
        if (i == 0) sa_gate_pick(g, l, d);
        else if (i == 1) SV_ADJ(gt->depth, 5, 0, 100);
        else if (i == 2) SV_ADJ(gt->width, 5, 10, 100);
        else SV_ADJ(gt->smooth, 5, 0, 100);
        break;
    }
    case SVP_MOD: {
        if (i == 3 && u->view == SV_V_DRUMS) { SV_ADJ(v->cut, 5, 0, 100); break; }   /* CHANCE */
        int t = e->type == SA_T_DRUM ? SA_T_DRUM : SA_T_SYNTH;
        int at = sa_mod_index(t, e->mod[i]);
        e->mod[i] = sa_mod_opt(t, sv_clampi(at + d, 0, sa_mod_opts(t) - 1));
        break;
    }
    case SVP_MIX:
        if (i == 0) SV_ADJ(v->level, 2, 0, 100);
        else if (i == 3) sa_mute(g, l, d > 0);
        else if (u->view != SV_V_DRUMS) sv_sound_turn(u, i - 1, d);   /* TYPE, INSTR: at once */
        else if (i == 1) s->kit = (uint8_t)sv_clampi(s->kit + d, 0, SA_KITS - 1);
        else sv_instr_turn(u, d);
        break;
    case SVP_HAND:
        if (i == 0) v->hand = (uint8_t)(d > 0);
        break;
    case SVP_KIT:
        if (i == 0) s->kit = (uint8_t)sv_clampi(s->kit + d, 0, SA_KITS - 1);
        else if (i == 1) SV_ADJ(s->ktune, 2, 0, 100);
        else if (i == 2) SV_ADJ(s->ktone, 2, 0, 100);
        else SV_ADJ(s->kdecay, 2, 0, 100);
        break;
    default: break;
    }
#undef SV_ADJ
}

void sv_button(sa_ui *u, int b, bool down)
{
    sa_groove *g = u->g;
    if (b == SV_B_SEQ) {                           /* (held a second: a new groove, sv_tick; a tap acts as ever) */
        u->seq_down = down;
        u->seq_ms = 0;
        if (down) u->seq_fired = false;
    }
    if (b == SV_B_PLAY) {                          /* (0.8.38) stopped: PLAY at once; playing: a tap stops (on letting
                                                    * go), held half a second FADE (sv_tick): out over a bar */
        if (down) {
            if (!g->playing) {
                sa_play(g, true);
                u->play_down = false;
            } else {
                u->play_down = true;
                u->play_ms = 0;
            }
            return;
        }
        if (u->play_down) {
            u->play_down = false;
            sa_play(g, false);
        }
        return;
    }
    if (b == SV_B_GLO) {                           /* (0.8.25) held: STYLE MIX (sv_tick); a tap on letting go */
        if (down) {
            u->glo_down = true;
            u->glo_ms = 0;
            return;
        }
        if (!u->glo_down) return;
        u->glo_down = false;
        down = true;
    }
    if (b == SV_B_HOME) {
        if (down) {
            u->home_down = true;
            u->home_ms = 0;
        } else if (u->home_down) {
            u->home_down = false;
            if (u->home_ms < SV_MENU_HOLD_MS && u->page == SV_PG_MENU && !u->menu2) {   /* MENU: HOME its next page */
                u->menu2 = true;
                u->force = true;
            } else if (u->home_ms < SV_MENU_HOLD_MS && u->page != SV_PG_MAIN) {
                sv_view(u, SV_V_HOME);
            } else if (u->home_ms < SV_MENU_HOLD_MS) {   /* (0.8.47) on HOME: its VIEW, and back */
                u->home2 = u->home2 == 3 ? 0 : 3;
                u->force = true;
            }
        }
        return;
    }
    {   /* MOVES: OCT- OCT+ EDIT REC act on a tap (let go before SV_MOVE_MS); held longer, a move (sv_tick) */
        int mv = b == SV_B_OCTDN ? SV_MV_WASH : b == SV_B_OCTUP ? SV_MV_THROW : b == SV_B_EDIT ? SV_MV_STOP :
                 b == SV_B_REC ? SV_MV_WOBBLE : -1;
        if (mv >= 0) {
            uint8_t bit = (uint8_t)(1u << mv);
            if (down) {
                u->hb_down |= bit;
                u->hb_ms[mv] = 0;
                return;
            }
            if (!(u->hb_down & bit)) return;
            u->hb_down &= (uint8_t)~bit;
            if (u->moves & bit) {                  /* a move: over */
                u->moves &= (uint8_t)~bit;
                return;
            }
            down = true;                           /* a tap: the button's own job, now */
        }
    }
    if (b == SV_B_ENV) {                           /* the drum fill: held, from the next beat */
        sa_fill(g, down);
        return;
    }
    if (b == SV_B_ARP) {                           /* BREAK: held, the drums and the bass out; back on the 1 */
        sa_break(g, down);
        return;
    }
    if (b == SV_B_LFO) {                           /* SWEEP: held, the filter closes; let go: open */
        sa_sweep(g, down);
        if (down) sv_msg(u, "SWEEP");
        return;
    }
    if (!down)
        return;
    switch (b) {
    case SV_B_SEQ:                                 /* drop what waits; on STYLE: a fresh groove in it */
        if (u->page == SV_PG_MIXER && u->mixmute == 6) {   /* STYLE MIX: a fresh groove in TO, mixed into */
            if (g->mx_state) { sv_msg(u, "MIXING"); break; }
            u->req_act = SV_SA_MIX;
            u->req_slot = sv_clampi(u->mix_pick, 0, SAS_NSTYLES - 1);
            u->hot = 0, u->hot_ms = 700;           /* (the style lit: SEQ took it, changed or not) */
            break;
        }
        if (u->page == SV_PG_MIXER && u->mixmute == 4 && !(g->playing && sa_build_peek(g) >= 0)) {
            /* LIVE: the mix set up, its START in, the rest waiting (a build playing: SEQ brings the next in) */
            u->req_act = SV_SA_LIVE;
            u->req_slot = sv_clampi(u->style_pick, 0, SAS_NSTYLES - 1);
            u->hot = 0, u->hot_ms = 700;
            break;
        }
        if (u->page == SV_PG_MIXER && u->mixmute == 3) {
            u->req_act = SV_SA_STYLE;
            u->req_slot = sv_clampi(u->style_pick, 0, SAS_NSTYLES - 1);
            u->hot = 0, u->hot_ms = 700;
            break;
        }
        sa_drop(g);
        u->changed = true;
        break;
    case SV_B_REC:
        if (u->page == SV_PG_SETS) {               /* SETS' action: the glue does it (the flash is its) */
            const sv_set_info *si = &u->sets[sv_clampi(u->set_slot, 0, SV_SLOTS - 1)];
            if (u->set_slot == SV_LAST && (u->set_act == SV_SA_STORE || u->set_act == SV_SA_CLEAR)) {
                sv_msg(u, "LAST KEEPS ITSELF");
                break;
            }
            if ((u->set_act == SV_SA_LOAD || u->set_act == SV_SA_CLEAR) && !si->used && !si->wait) {
                sv_msg(u, "THAT SET IS EMPTY");
                break;
            }
            u->req_act = u->set_act;
            u->req_slot = u->set_slot;
            break;
        }
        if (u->page == SV_PG_LAYER && u->layer >= 0 && sv_page_now(u) == SVP_GATE) {   /* GATE: its steps on the keys */
            u->rec = !u->rec;
            u->force = true;
            sv_msg(u, u->rec ? "REC: THE GATE ON THE KEYS" : "REC OFF: THE KEYS PLAY");
        } else if (u->page == SV_PG_LAYER && u->layer >= 0) {
            sv_button(u, SV_B_EDIT, true);         /* (elsewhere on a layer: its editor, where the keys write; */
            sv_button(u, SV_B_EDIT, false);        /* EDIT acts on its release, a tap) */
        } else if (u->page == SV_PG_EDIT) {
            sv_msg(u, "EDIT: THE KEYS WRITE");
        }
        break;
    case SV_B_EDIT:                                /* a layer's pattern editor, again: back to its page */
        if (u->page == SV_PG_EDIT) { u->page = SV_PG_LAYER; u->rec = false; u->force = true; }
        else if (u->page == SV_PG_LAYER && u->layer >= 0) {
            u->page = SV_PG_EDIT;
            u->rec = false;
            u->scur = 0;
            u->fill = 0;
            u->fill_orig = u->g->ed[u->layer];     /* (CURR: the pattern as it is now) */
            u->force = true;
        }
        break;
    case SV_B_GLO:                                 /* GLOBAL: MIXER, MUTE, SOLO, STYLE, LIVE, SONG, each press the next */
        if (u->page == SV_PG_MIXER && u->mixmute < 5) {
            u->mixmute++;
            if (u->mixmute == 3) u->style_pick = g->s.style;
            u->force = true;
        } else if (u->page == SV_PG_MIXER) {
            if (u->mback == SV_PG_LAYER) { u->page = SV_PG_LAYER; u->force = true; }
            else sv_view(u, SV_V_HOME);
        } else {
            u->mback = u->page == SV_PG_LAYER ? SV_PG_LAYER : SV_PG_MAIN;
            u->mixmute = 0;
            sv_goto(u, SV_PG_MIXER);
        }
        break;
    case SV_B_FX:
        if (u->page == SV_PG_MAIN) {               /* HOME: its pages, SPACE and PARTS */
            u->home2 = u->home2 == 3 ? 0 : (u->home2 + 1) % 3;   /* (VIEW: back to FILTER FEEL PUMP SPACE) */
            u->force = true;
            break;
        }
        if (u->page == SV_PG_MIXER) break;         /* (GLO turns its pages) */
        if (u->page == SV_PG_LAYER) {
            int p[10], n = sv_pages(u, p);
            u->fx = (u->fx + 1) % n;
            u->force = true;
        }
        break;
    case SV_B_SCL:
        if (u->page == SV_PG_KEY) {
            if (u->back == SV_PG_LAYER) { u->page = SV_PG_LAYER; u->force = true; }
            else sv_view(u, SV_V_HOME);
        } else {
            u->back = u->page == SV_PG_LAYER ? SV_PG_LAYER : SV_PG_MAIN;
            sv_goto(u, SV_PG_KEY);
        }
        break;
    case SV_B_SAVE:
        if (u->page == SV_PG_SETS) {
            if (u->sback == SV_PG_LAYER) { u->page = SV_PG_LAYER; u->force = true; sv_view(u, u->view); }
            else sv_view(u, SV_V_HOME);
        } else {
            u->sback = u->page == SV_PG_LAYER ? SV_PG_LAYER : SV_PG_MAIN;
            sv_goto(u, SV_PG_SETS);
        }
        break;
    case SV_B_OCTDN: case SV_B_OCTUP: {
        char m[16], a[8];
        u->oct = sv_clampi(u->oct + (b == SV_B_OCTUP ? 1 : -1), -2, 2);
        sv_cpy(m, "KEYS OCT ", sizeof m);
        if (u->oct > 0) sv_cat(m, "+", sizeof m);
        sv_fmt_int(a, u->oct);
        sv_cat(m, a, sizeof m);
        sv_msg(u, m);
        break;
    }
    default: break;
    }
}

void sv_key(sa_ui *u, int note, bool down)
{
    if (note < SV_KEY_LO || note > SV_KEY_HI)
        return;
    int k = note - SV_KEY_LO;
    bool lay = sv_on_layer(u) && u->layer >= 0;
    if (lay && u->page == SV_PG_EDIT && u->view == SV_V_DRUMS && k < 16) {   /* EDIT: the 16 lowest keys are the steps
                                                                              * (lit) */
        if (!down || u->g->ed[u->layer].type != SA_T_DRUM) return;
        int st = k;
        bool was = (sv_ed_bits(u, u->layer) >> st) & 1u;
        sa_step_drum(u->g, u->layer, st);
        if (!was) sa_key_drum(u->g, k, u->g->ed[u->layer].sound, true), sa_key_drum(u->g, k, u->g->ed[u->layer].sound, false);
        u->scur = st;
        u->changed = true;
        return;
    }
    if (!lay && sv_drum_view(u)) {                 /* DRUMS with no drum picked: silent, never LEAD (a held key let go) */
        if (!down && u->key_sent[k] >= 0) {
            sa_key(u->g, k, u->key_sent[k], false, SA_L_LEAD);
            u->key_sent[k] = -1;
        }
        return;
    }
    int layer = lay ? u->layer : SA_L_LEAD;
    if (lay && u->rec && u->page == SV_PG_LAYER) {   /* (outside EDIT the keys play; only GATE's REC sets its steps) */
        if (sv_page_now(u) == SVP_GATE && k < 16) {  /* the gate's steps */
            if (down) {
                sa_gate_step(u->g, u->layer, k);
                u->changed = true;
            }
            return;
        }
    }
    if (lay && sv_steps_on(u)) {
        /* EDIT, a synth layer's step: the key's note there (CHORD: the chord), heard as it is pressed */
        if (down) {
            sa_step_note(u->g, u->layer, sv_clampi(u->scur, 0, 15), note + 12 * u->oct);
            u->scur = (u->scur + 1) % 16;
            u->changed = true;
            u->key_sent[k] = note + 12 * u->oct;
            sa_key(u->g, k, u->key_sent[k], true, layer);
        } else if (u->key_sent[k] >= 0) {
            sa_key(u->g, k, u->key_sent[k], false, layer);
            u->key_sent[k] = -1;
        }
        return;
    }
    if (lay && u->view == SV_V_DRUMS) {            /* DRUMS: every key plays the drum picked (nothing else, ever) */
        if (u->g->ed[u->layer].type != SA_T_DRUM) return;
        int dv = u->g->ed[u->layer].sound;
        if (down) {
            char m[32];
            sv_cpy(m, "KEYS: DRUMS, ", sizeof m);
            sv_cat(m, sa_dv_name(dv), sizeof m);
            sv_msg(u, m);
        }
        sa_key_drum(u->g, k, dv, down);
        return;
    }
    if (down) {
        char m[32];                                /* what the keys play, briefly in the header */
        sv_cpy(m, "KEYS: ", sizeof m);
        sv_cat(m, lay ? SV_VIEW_NAME[u->view] : "LEAD", sizeof m);
        sv_cat(m, ", ", sizeof m);
        sv_cat(m, sv_layer_name(u, layer), sizeof m);
        sv_msg(u, m);
        u->key_sent[k] = note + 12 * u->oct;
        sa_key(u->g, k, u->key_sent[k], true, layer);
    } else {
        sa_key(u->g, k, u->key_sent[k], false, layer);
        u->key_sent[k] = -1;
    }
}

bool sv_key_leds(const sa_ui *u, uint32_t *full, uint32_t *dim)
{
    *full = *dim = 0;
    if (!sv_on_layer(u) || u->layer < 0) return false;
    if (u->page != SV_PG_EDIT && !(u->rec && sv_page_now(u) == SVP_GATE)) return false;   /* (EDIT; GATE's REC) */
    const sa_groove *g = u->g;
    int l = u->layer;
    if (u->view == SV_V_DRUMS) {
        if (g->ed[l].type != SA_T_DRUM) return false;
        *dim = 0xFFFFu;                            /* the step keys */
        *full = u->ed_code[l].type == SA_T_DRUM && !memcmp(&u->ed_code[l], &g->ed[l], sizeof u->ed_code[l])
                    ? u->ed_bits[l] : 0;            /* the steps set (as last drawn) */
        return true;
    }
    if (sv_page_now(u) == SVP_GATE) {
        const sa_gate *e = &g->ed_gate[l];
        *dim = 0xFFFFu;
        *full = sa_gate_on(e) ? sa_gate_mask(e) : 0;
        return true;
    }
    const sa_code *e = &g->ed[l];
    int st = sv_clampi(u->scur, 0, 15);
    for (int k = 0; k < 27; ++k) {                 /* the scale's keys dim, the step's note full */
        int nn = SV_KEY_LO + k + 12 * u->oct;
        if (sa_snap(&g->ed_harm, nn) == nn) *dim |= 1u << k;
        if (e->own && e->sn[st] != SA_NO_NOTE && !((e->chord >> st) & 1u) && g->ed_harm.key + e->sn[st] == nn)
            *full |= 1u << k;
    }
    return true;
}

uint32_t sv_play_keys(const sa_ui *u)
{
    const sa_groove *g = u->g;
    if (!sv_on_layer(u) || u->layer < 0) return 0;
    if (u->view != SV_V_DRUMS && !g->playing) return 0;
    if (u->view == SV_V_DRUMS) {                   /* the drum picked: its own note (General MIDI), on the keys */
        const sa_code *e = &g->ed[u->layer];
        if (e->type != SA_T_DRUM) return 0;
        int k = sa_dv_gm(e->sound);
        while (k < SV_KEY_LO) k += 12;
        while (k > SV_KEY_HI) k -= 12;
        return 1u << (k - SV_KEY_LO);
    }
    int part = g->s.code[u->layer].part, n = g->nheld;
    uint32_t m = 0;
    for (int i = 0; i < n && i < SA_HELD; ++i) {   /* (the audio's list, read as it is: only lights) */
        if (g->held[i].part != part) continue;
        int k = g->held[i].note - 12 * u->oct;
        while (k < SV_KEY_LO) k += 12;
        while (k > SV_KEY_HI) k -= 12;
        m |= 1u << (k - SV_KEY_LO);
    }
    return m;
}

uint32_t sv_play_dim(const sa_ui *u)
{
    (void)u;
    return 0;
}

void sv_tick(sa_ui *u, int ms)
{
    sa_groove *g = u->g;
    if (g->msg_new) {
        g->msg_new = 0;
        sv_msg(u, g->msg);
    }
    if (u->hot_ms > 0) u->hot_ms -= ms;
    if (u->msg_ms > 0) u->msg_ms -= ms;
    if (u->main_ms > 0) u->main_ms -= ms;
    if (u->seq_down && !u->seq_fired && (u->seq_ms += ms) >= SV_NEW_MS && u->req_act < 0) {   /* SEQ held: NEW */
        u->seq_fired = true;
        u->req_slot = 0;
        if (u->page == SV_PG_MAIN) {               /* (0.8.32) on HOME: the part resting, new */
            u->req_act = SV_SA_PART;
            sv_msg(u, g->part_b ? "NEW A" : "NEW B");
        } else {
            u->req_act = SV_SA_NEW;
            sv_msg(u, "A NEW GROOVE");
        }
    }
    for (int mv = 0; mv < SV_MOVES; ++mv)          /* MOVES: held long enough */
        if ((u->hb_down >> mv) & 1u && !((u->moves >> mv) & 1u) && (u->hb_ms[mv] += ms) >= SV_MOVE_MS) {
            static const char *const N[SV_MOVES] = {"WASH", "THROW", "TAPE STOP", "WOBBLE"};
            u->moves |= (uint8_t)(1u << mv);
            sv_msg(u, N[mv]);
        }
    if (u->g->ed_solo) {                           /* EDIT's SOLO: only in a drum's EDIT; another drum picked: it */
        int l = u->layer;
        if (u->page != SV_PG_EDIT || u->view != SV_V_DRUMS || l < SA_L_DRUM0 || l >= SA_LAYERS) u->g->ed_solo = 0;
        else u->g->ed_solo = (uint8_t)(l + 1);
    }
    if (u->play_down && (u->play_ms += ms) >= 500) {   /* PLAY held: FADE */
        u->play_down = false;
        sa_fade_out(u->g);
        sv_msg(u, "FADING OUT");
    }
    if (u->glo_down && (u->glo_ms += ms) >= SV_MENU_HOLD_MS) {   /* GLO held: STYLE MIX */
        u->glo_down = false;
        if (u->page != SV_PG_MIXER) u->mback = u->page == SV_PG_LAYER ? SV_PG_LAYER : SV_PG_MAIN;
        u->mixmute = 6;
        u->rec = false;
        if (u->mix_pick == u->g->s.style) u->mix_pick = (u->g->s.style + 1) % SAS_NSTYLES;   /* (TO: another one) */
        if (u->page != SV_PG_MIXER) sv_goto(u, SV_PG_MIXER);
        else u->force = true;
    }
    if (u->home_down) {
        u->home_ms += ms;
        if (u->home_ms >= SV_MENU_HOLD_MS && u->page != SV_PG_MENU) {
            sv_goto(u, SV_PG_MENU);
            u->menu2 = false;
            u->rec = false;
            u->home_down = false;
        }
    }
}
