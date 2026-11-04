/* SPDX-License-Identifier: GPL-3.0-only */
/* SARDINE screen and controls (sa_ui.h), drawn with Felucca's gfx.c in the family's frame. Included after gfx.c (the
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

/* ---- what a layer is ---------------------------------------------------------------------------------------- */
/* the code the screen edits (the copy) and what plays */
static sa_code *sv_ed(sa_ui *u) { return &u->g->ed[sv_clampi(u->layer, 0, SA_LAYERS - 1)]; }
static const sa_code *sv_live(const sa_ui *u) { return &u->g->s.code[sv_clampi(u->layer, 0, SA_LAYERS - 1)]; }
/* the layer's type: the copy's, else what plays (a layer about to go keeps its type on the screen) */
static int sv_type(const sa_ui *u, int l)
{
    const sa_groove *g = u->g;
    if (l < 0 || l >= SA_LAYERS) return SA_T_NONE;
    return g->ed[l].type != SA_T_NONE ? g->ed[l].type : g->s.code[l].type;
}
static bool sv_is_new(const sa_ui *u) { return u->page == SV_PG_LAYER && !sa_used(u->g, u->layer); }
static const char *sv_layer_name(const sa_ui *u, int l)
{
    const sa_groove *g = u->g;
    const sa_code *c = g->ed[l].type != SA_T_NONE ? &g->ed[l] : &g->s.code[l];
    if (c->type == SA_T_NONE) return "NEW";
    return sas_name(c->type, c->sound);
}
/* the screens SELECT goes through: MAIN (-1), the layers in use, NEW (the first free one) */
static int sv_screens(const sa_ui *u, int *list)
{
    int n = 0, fr = -1;
    list[n++] = -1;
    for (int l = 0; l < SA_LAYERS; ++l) {
        if (sa_used(u->g, l)) list[n++] = l;
        else if (fr < 0) fr = l;
    }
    if (fr >= 0) list[n++] = fr;
    return n;
}

/* ---- the chips of a layer's code: SOUND PATTERN, the modifiers, + (room for one more), ARP (synth) ---- */
enum { CH_SOUND, CH_PAT, CH_MOD, CH_PLUS, CH_ARP };
typedef struct { uint8_t kind, idx; } sv_chip;
static int sv_chips(const sa_ui *u, sv_chip *out)
{
    const sa_code *c = &u->g->ed[u->layer];
    int n = 0, t = sv_type(u, u->layer), nm = 0;
    out[n].kind = CH_SOUND; out[n++].idx = 0;
    if (sv_is_new(u) && c->type == SA_T_NONE) return n;
    out[n].kind = CH_PAT; out[n++].idx = 0;
    for (int i = 0; i < SA_MODS; ++i)
        if (c->mod[i].type != SA_M_OFF) { out[n].kind = CH_MOD; out[n++].idx = (uint8_t)i; nm++; }
    if (nm < SA_MODS) { out[n].kind = CH_PLUS; out[n++].idx = 0; }
    if (t == SA_T_SYNTH) { out[n].kind = CH_ARP; out[n++].idx = 0; }
    return n;
}
static void sv_chip_text(const sa_ui *u, const sa_code *c, sv_chip ch, char *t, unsigned n)
{
    int type = sv_type(u, u->layer);
    switch (ch.kind) {
    case CH_SOUND:
        if (c->type == SA_T_NONE) sv_cpy(t, sv_is_new(u) ? "SOUND?" : "REMOVE", n);
        else sv_cpy(t, sas_name(c->type, c->sound), n);
        break;
    case CH_PAT: sv_cpy(t, sa_pat_name(type, c->pat), n); break;
    case CH_MOD: sa_mod_name(c->mod[ch.idx], t, (int)n); break;
    case CH_PLUS: sv_cpy(t, "+", n); break;
    default:
        sv_cpy(t, "ARP ", n);
        sv_cat(t, sa_arp_name(c->amode), n);
        break;
    }
}
/* the chip differs between the copy and what plays */
static bool sv_chip_pending(const sa_ui *u, sv_chip ch)
{
    const sa_code *e = &u->g->ed[u->layer], *l = &u->g->s.code[u->layer];
    switch (ch.kind) {
    case CH_SOUND: return e->type != l->type || e->sound != l->sound;
    case CH_PAT: return e->pat != l->pat || e->type != l->type;
    case CH_MOD: return e->mod[ch.idx].type != l->mod[ch.idx].type || e->mod[ch.idx].val != l->mod[ch.idx].val;
    case CH_ARP: return e->amode != l->amode || e->arate != l->arate || e->arange != l->arange || e->agate != l->agate;
    default: return false;
    }
}

/* the list a new layer's SOUND turns through: the drums, then the synth sounds (while a part is free) */
static int sv_new_sounds(const sa_ui *u) { return SA_DVS + (sa_synth_free_part(u->g, u->layer) >= 0 ? SAS_NSOUNDS : 0); }

/* ---- the cards ---------------------------------------------------------------------------------------------- */
typedef struct { char label[12], val[20]; int ratio; bool pend, off; } sv_card;
static void sv_card_num(sv_card *c, const char *label, int v, int lo, int hi)
{
    sv_cpy(c->label, label, sizeof c->label);
    sv_fmt_int(c->val, v);
    c->ratio = hi > lo ? (v - lo) * 1000 / (hi - lo) : -1;
    c->pend = c->off = false;
}
static void sv_card_txt(sv_card *c, const char *label, const char *val, int ratio, bool pend)
{
    sv_cpy(c->label, label, sizeof c->label);
    sv_cpy(c->val, val, sizeof c->val);
    c->ratio = ratio;
    c->pend = pend;
    c->off = false;
}
static void sv_card_off(sv_card *c)
{
    memset(c, 0, sizeof *c);
    c->off = true;
    c->ratio = -1;
}

static void sv_cards_of(sa_ui *u, sv_card *c)
{
    sa_groove *g = u->g;
    const sa_settings *s = &g->s;
    char t[20];
    switch (u->page) {
    case SV_PG_MAIN:
        sv_card_num(&c[0], "TEMPO", s->bpm, 60, 200);
        if (s->filter >= 100) sv_card_txt(&c[1], "FILTER", "OPEN", 1000, false);
        else sv_card_num(&c[1], "FILTER", s->filter, 0, 100);
        sv_card_num(&c[2], "FEEL", s->feel, 0, 60);
        sv_card_txt(&c[3], "KIT", sa_kit_name(s->kit), s->kit * 1000 / (SA_KITS - 1), false);
        break;
    case SV_PG_LAYER: {
        int l = u->layer, type = sv_type(u, l);
        const sa_live *v = &s->live[l];
        if (type == SA_T_SYNTH) {
            sv_card_num(&c[0], "CUTOFF", v->cut, 0, 100);
            sv_card_num(&c[1], "RESO", v->reso, 0, 100);
            sv_card_num(&c[2], "DRIVE", v->drive, 0, 100);
            sv_card_num(&c[3], "ENV", v->env, 0, 100);
        } else if (type == SA_T_DRUM) {
            sv_card_num(&c[0], "LEVEL", v->level, 0, 100);
            sv_card_num(&c[1], "TUNE", s->ktune, 0, 100);
            sv_card_num(&c[2], "TONE", s->ktone, 0, 100);
            sv_card_num(&c[3], "DECAY", s->kdecay, 0, 100);
        } else {                                   /* NEW: SOUND and PATTERN on the first two */
            const sa_code *e = &g->ed[l];
            sv_card_txt(&c[0], "SOUND", e->type == SA_T_NONE ? "-" : sas_name(e->type, e->sound), -1, e->type != SA_T_NONE);
            sv_card_txt(&c[1], "PATTERN", e->type == SA_T_NONE ? "-" : sa_pat_name(e->type, e->pat), -1,
                        e->type != SA_T_NONE);
            sv_card_off(&c[2]);
            sv_card_off(&c[3]);
        }
        break;
    }
    case SV_PG_ARP: {
        const sa_code *e = sv_ed(u), *l = sv_live(u);
        static const char *const SHORT[SA_AR_MODES] = {"OFF", "UP", "DOWN", "UP-DN", "DN-UP", "CONVRG", "DIVRG",
                                                       "RANDOM", "THUMB", "PINKY", "STRUM"};
        sv_card_txt(&c[0], "MODE", SHORT[e->amode % SA_AR_MODES], e->amode * 1000 / (SA_AR_MODES - 1),
                    e->amode != l->amode);
        sv_card_txt(&c[1], "RATE", sa_rate_name(e->arate), e->arate * 1000 / (SA_RATES - 1), e->arate != l->arate);
        sv_fmt_int(t, e->arange);
        sv_cat(t, " OCT", sizeof t);
        sv_card_txt(&c[2], "RANGE", t, (e->arange - 1) * 1000 / 3, e->arange != l->arange);
        sv_card_num(&c[3], "GATE", e->agate, 10, 100);
        c[3].pend = e->agate != l->agate;
        break;
    }
    case SV_PG_KEY: {
        const sa_harm *e = &g->ed_harm, *l = &s->harm;
        sv_card_txt(&c[0], "KEY", sa_note_name(e->key), e->key * 1000 / 11, e->key != l->key);
        sv_card_txt(&c[1], "SCALE", sa_scale_name(e->scale), e->scale * 1000 / (SA_SCALES - 1), e->scale != l->scale);
        sa_prog_name(e->prog, t, sizeof t);
        sv_card_txt(&c[2], "CHORDS", t, e->prog * 1000 / (SA_PROGS - 1), e->prog != l->prog);
        sv_fmt_int(t, e->bars);
        sv_cat(t, e->bars == 1 ? " BAR" : " BARS", sizeof t);
        sv_card_txt(&c[3], "BARS", t, e->bars == 1 ? 0 : e->bars == 2 ? 500 : 1000, e->bars != l->bars);
        break;
    }
    default:                                       /* MENU */
        sv_card_txt(&c[0], "COLOR", UI_PALETTES[s->palette % NPALETTES].name, -1, false);
        sv_card_txt(&c[1], "LOOK", s->look ? "LINE" : "FLAT", -1, false);
        sv_card_off(&c[2]);
        sv_card_off(&c[3]);
        break;
    }
}

static void sv_draw_card(sa_ui *u, int i, const sv_card *c)
{
    bool hot = u->hot == i && u->hot_ms > 0;
    uint16_t lc = hot || c->pend ? T_ACCENT : T_MID, vc = hot || c->pend ? T_ACCENT : T_THEME;
    uint32_t sig = sv_hash(sv_hash(2166136261u + (uint32_t)i * 7u + ux.gen * 131u + (uint32_t)hot * 3u +
                                   (uint32_t)c->pend * 5u + (uint32_t)c->off * 11u + (uint32_t)u->page * 977u, c->label),
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
        cv_text_fit(5, vf == &AF_M ? 17 : 19, vf, c->val, vc, T_SURF, SV_CARD_W - 8);
        if (c->ratio >= 0) {
            int32_t w = SV_CARD_W - 10, r = sv_clampi(c->ratio, 0, 1000) * w / 1000;
            cv_rect(5, 37, w, 3, T_LINE);
            cv_rect(5, 37, r > 0 ? r : 1, 3, vc);
        }
    }
    cv_blit((uint32_t)SV_CARD_X(i), SV_CARD_Y);
}

/* ---- the header: playing, the tempo, the screen's name, the drop's badge, the battery ---- */
static void sv_head_title(const sa_ui *u, char *t, unsigned n)
{
    char a[8];
    t[0] = 0;
    if (u->msg_ms > 0) { sv_cpy(t, u->msg, n); return; }
    switch (u->page) {
    case SV_PG_MAIN: sv_cpy(t, u->g->playing ? u->g->chord_name : "MAIN", n); break;
    case SV_PG_LAYER: case SV_PG_ARP:
        if (sv_is_new(u)) { sv_cpy(t, "NEW LAYER", n); break; }
        sv_fmt_int(a, u->layer + 1);
        sv_cpy(t, a, n);
        sv_cat(t, "  ", n);
        sv_cat(t, sv_layer_name(u, u->layer), n);
        if (u->page == SV_PG_ARP) sv_cat(t, "  ARP", n);
        break;
    case SV_PG_KEY: sv_cpy(t, "KEY", n); break;
    default: sv_cpy(t, "MENU", n); break;
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
    if (g->drop_armed) {
        sv_cpy(bd, "ON THE 1", sizeof bd);
    } else if (pend) {
        sv_fmt_int(bd, pend);
        sv_cat(bd, " TO DROP", sizeof bd);
    }
    uint32_t sig = sv_hash(sv_hash((uint32_t)g->playing * 3u + (uint32_t)g->s.bpm * 101u + ux.gen * 13u +
                                   (uint32_t)u->batt * 7777u + (uint32_t)hot * 23u, m), bd);
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
    if (m[0])
        cv_free_text(62, CAP_IN(S, SV_HEAD), &AF_S, m, u->msg_ms > 0 ? T_TEXT : T_THEME, T_BG, right - 62);
    {
        static const uint8_t BAT[5] = {ICON_X_BAT0, ICON_X_BAT1, ICON_X_BAT3, ICON_X_BAT4, ICON_X_BAT_CHG};
        int lv = sv_clampi(u->batt, 0, 4);
        sv_icon_mid(214, SV_HEAD / 2, 24, BAT[lv], lv == 1 ? T_ACCENT : lv == 0 ? T_MID : T_THEME, T_BG);
    }
    cv_blit(0, 0);
}

/* ---- the footer: the bar as 16 dashes, where we are, what REC does ---- */
static void sv_draw_foot(sa_ui *u)
{
    const sa_groove *g = u->g;
    int now = g->playing && g->tick >= 0 ? g->tick / SA_TPS : -1;
    char l[24], r[20], a[8];
    l[0] = r[0] = 0;
    int list[SA_LAYERS + 2], n = sv_screens(u, list), at = 0;
    for (int i = 0; i < n; ++i)
        if (list[i] == (u->page == SV_PG_MAIN ? -1 : u->layer)) at = i;
    switch (u->page) {
    case SV_PG_MAIN: sv_cpy(l, "MAIN", sizeof l); break;
    case SV_PG_LAYER: case SV_PG_ARP:
        sv_cpy(l, sv_is_new(u) ? "NEW " : "LAYER ", sizeof l);
        sv_fmt_int(a, at);
        sv_cat(l, a, sizeof l);
        sv_cat(l, " / ", sizeof l);
        sv_fmt_int(a, n - 1);
        sv_cat(l, a, sizeof l);
        if (u->page == SV_PG_LAYER && !sv_is_new(u) && g->s.live[u->layer].mute) sv_cat(l, "  MUTED", sizeof l);
        break;
    case SV_PG_KEY: sv_cpy(l, "KEY", sizeof l); break;
    default: sv_cpy(l, "MENU", sizeof l); break;
    }
    if (sa_pending(g)) sv_cpy(r, g->drop_armed ? "DROPS ON THE 1" : "REC: DROP", sizeof r);
    else if (u->page == SV_PG_MAIN) sv_cpy(r, "SELECT: LAYERS", sizeof r);
    else if (u->page != SV_PG_MENU) sv_cpy(r, "HOME: MAIN", sizeof r);
    uint32_t sig = sv_hash(sv_hash((uint32_t)(now + 1) * 7u + (uint32_t)g->playing * 3u + ux.gen * 11u, l), r);
    if (!u->force && sig == u->sig_foot)
        return;
    u->sig_foot = sig;
    cv_begin(240, SV_FOOT_H, T_BG);
    for (int i = 0; i < 16; ++i)
        cv_rect(10 + i * 14, 6, 10, 2, i < now ? T_THEME : i == now ? T_ACCENT : T_LINE);
    cv_free_text(10, 17, &AF_M, l, T_THEME, T_BG, 130);
    int32_t yb = 17 + AF_M_CAP_Y + AF_M_CAP_H - AF_S_CAP_Y - AF_S_CAP_H;
    if (r[0]) cv_text_r(230, yb, &AF_S, r, sa_pending(g) ? T_ACCENT : T_MID, T_BG);
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

/* the preview bits of a code (a bar of it), worked out again only when it changed */
static uint16_t sv_preview(sa_ui *u, int slot, const sa_code *c, int layer)
{
    int bar = u->g->playing ? u->g->bar : 0;
    if (u->pv_layer == layer && u->pv_bar == bar && !memcmp(&u->pv_code[slot], c, sizeof *c) && !u->force)
        return u->pv_bits[slot];
    SV_LOCK();
    uint16_t b = sa_preview(u->g, c, layer, bar);
    SV_UNLOCK();
    u->pv_code[slot] = *c;
    u->pv_bits[slot] = b;
    if (slot == 1) {
        u->pv_layer = layer;
        u->pv_bar = bar;
    }
    return b;
}

static void sv_panel_main(sa_ui *u)
{
    sa_groove *g = u->g;
    int now = g->playing && g->tick >= 0 ? g->tick / SA_TPS : -1, row = 0;
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
    int used = 0;
    for (int l = 0; l < SA_LAYERS; ++l) used += sa_used(g, l);
    bool wide = used <= 6;                         /* up to six: one column, the bars wide; more: two */
    for (int l = 0; l < SA_LAYERS; ++l) {
        if (!sa_used(g, l)) continue;
        int32_t col = row / 6, r = row % 6, x = 8 + col * 115, y = 5 + r * 19;
        const sa_code *c = &g->s.code[l];
        bool mute = g->s.live[l].mute, pend = sa_layer_pending(g, l);
        uint16_t lc = c->type == SA_T_SYNTH ? T_ACCENT : T_THEME;
        char n[4];
        sv_fmt_int(n, l + 1);
        sv_label(x, y + 2, n, T_DIM);
        cv_free_text(x + 14, y + 2, &AF_S, sv_layer_name(u, l), mute ? T_DIM : pend ? T_ACCENT : T_TEXT, T_SURF,
                     wide ? 76 : 42);
        uint16_t bits = c->type == SA_T_NONE ? 0 : g->playing ? (uint16_t)((g->hits[l] & ((2u << (now < 0 ? 0 : now)) - 1u)) |
                                                                           (g->hits_last[l] & ~((2u << (now < 0 ? 0 : now)) - 1u)))
                                                              : u->main_bits[l];
        int32_t sx = wide ? x + 94 : x + 58, sw = wide ? 128 : 48;
        sv_strip(sx, y + 4, sw, 8, bits, now, lc, mute || c->type == SA_T_NONE);
        if (mute) cv_rect(sx, y + 7, sw, 2, T_ACCENT);
        row++;
        if (row >= 12) break;
    }
    if (row == 0)
        sv_label(10, 50, "TURN SELECT FOR A NEW LAYER", T_MID);
}

static void sv_panel_layer(sa_ui *u)
{
    sa_groove *g = u->g;
    int l = u->layer, now = g->playing && g->tick >= 0 ? g->tick / SA_TPS : -1;
    const sa_code *e = &g->ed[l], *lv = &g->s.code[l];
    bool pend = sa_layer_pending(g, l), mute = g->s.live[l].mute;
    if (sv_is_new(u) && e->type == SA_T_NONE) {
        sv_label(10, 8, "A NEW LAYER", T_THEME);
        sv_label(10, 30, "KNOB 1: ITS SOUND (A DRUM OR", T_MID);
        sv_label(10, 46, "A SYNTH), KNOB 2: ITS PATTERN.", T_MID);
        sv_label(10, 70, "REC DROPS IT IN ON THE 1.", T_MID);
        if (sa_synth_free_part(g, l) < 0)
            sv_label(10, 94, "THREE SYNTH LAYERS: DRUMS ONLY", T_ACCENT);
        return;
    }
    uint16_t ub = sv_preview(u, 0, lv, l), eb = sv_preview(u, 1, e, l);
    uint16_t on = sv_type(u, l) == SA_T_SYNTH ? T_ACCENT : T_THEME;
    sv_label(10, 4, mute ? "MUTED" : "PLAYING", mute ? T_ACCENT : T_MID);
    sv_strip(80, 5, 152, 9, lv->type == SA_T_NONE ? 0 : ub, now, T_THEME, mute || lv->type == SA_T_NONE);
    if (pend) {
        sv_label(10, 20, e->type == SA_T_NONE ? "GONE ON REC" : "AFTER REC", T_ACCENT);
        sv_strip(80, 21, 152, 9, e->type == SA_T_NONE ? 0 : eb, now, T_ACCENT, e->type == SA_T_NONE);
    }
    (void)on;
    /* the code as chips, in rows; the one PRESETS picked framed */
    sv_chip ch[SA_MODS + 4];
    int n = sv_chips(u, ch);
    if (u->chip >= n) u->chip = n - 1;
    int32_t x = 8, y = pend ? 40 : 26;
    for (int i = 0; i < n; ++i) {
        char t[20];
        sv_chip_text(u, e, ch[i], t, sizeof t);
        int32_t w = text_w(&AF_S, t) + 12;
        if (x + w > 232) { x = 8; y += 21; }
        if (y + 17 > SV_PANEL_H - 2) break;
        bool p = sv_chip_pending(u, ch[i]), sel = i == u->chip;
        uint16_t edge = sel ? T_TEXT : p ? T_ACCENT : ch[i].kind == CH_PLUS ? T_DIM : T_THEME;
        cv_rrect(x, y, w, 17, 3, edge, T_SURF);
        cv_rrect(x + (sel ? 2 : 1), y + (sel ? 2 : 1), w - (sel ? 4 : 2), 17 - (sel ? 4 : 2), 3, T_SURF, edge);
        cv_text_in(x, y + CAP_IN(S, 17), w, &AF_S, t, p ? T_ACCENT : ch[i].kind == CH_PLUS ? T_DIM : T_TEXT, T_SURF);
        x += w + 4;
    }
    if (y + 21 + 14 < SV_PANEL_H)
        sv_label(10, SV_PANEL_H - 18, "PRESETS PICKS, ALGORITHM TURNS", T_DIM);
}

static void sv_panel_arp(sa_ui *u)
{
    sa_groove *g = u->g;
    const sa_code *e = sv_ed(u);
    char t[24];
    sv_cpy(t, "CHORD ", sizeof t);
    sv_cat(t, g->chord_name, sizeof t);
    sv_label(10, 4, t, T_MID);
    if (e->amode == SA_AR_OFF) {
        sv_label(10, 50, "TURN MODE TO START IT", T_MID);
        return;
    }
    /* the order over the chord's tones and the octaves, a bar's worth of the rate (up to 32) */
    static const uint8_t RT[SA_RATES] = {24, 12, 8, 6, 4, 3};
    int ns = 3 * sv_clampi(e->arange, 1, 4), steps = 96 / RT[e->arate % SA_RATES];
    if (steps > 32) steps = 32;
    int32_t cw = 224 / steps, h = 90;
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
                cv_rect(x + 1, 22 + h - 6 - s * (h - 8) / ns, cw - 2 > 1 ? cw - 2 : 1, 4, T_ACCENT);
        } else {
            cv_rect(x + 1, 22 + h - 6 - k * (h - 8) / ns, cw - 2 > 1 ? cw - 2 : 1, 4, T_ACCENT);
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

static void sv_panel_menu(sa_ui *u)
{
    (void)u;
    cv_text_on(30, 16, &AF_M, "SARDINE " SA_VERSION, T_TEXT, T_SURF);
    cv_text_on(30, 42, &AF_S, "LIVE CLUB MIXING FOR THE FM-1", T_MID, T_SURF);
    cv_text_on(30, 60, &AF_S, "ON FELUCCA, AFTER TUNA", T_MID, T_SURF);
    cv_text_on(30, 80, &AF_S, "GPL-3.0, HOME CLOSES", T_DIM, T_SURF);
}

static void sv_draw_panel(sa_ui *u)
{
    sv_panel_begin();
    switch (u->page) {
    case SV_PG_MAIN: sv_panel_main(u); break;
    case SV_PG_LAYER: sv_panel_layer(u); break;
    case SV_PG_ARP: sv_panel_arp(u); break;
    case SV_PG_KEY: sv_panel_key(u); break;
    default: sv_panel_menu(u); break;
    }
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
    if (u->force)
        sv_background();
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
    case SV_PG_ARP: return SV_B_ARP;
    case SV_PG_KEY: return SV_B_SCL;
    default: return -1;
    }
}

/* ---- input ------------------------------------------------------------------------------------------------------ */
static void sv_msg(sa_ui *u, const char *m)
{
    sv_cpy(u->msg, m, sizeof u->msg);
    u->msg_ms = 1500;
}

static void sv_goto(sa_ui *u, int page, int layer)
{
    if (page != u->page || layer != u->layer) {
        u->page = page;
        u->layer = layer;
        u->force = true;
        if (page == SV_PG_LAYER) u->chip = 0;
    }
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
    for (int k = 0; k < 27; ++k)
        u->key_sent[k] = -1;
    memset(u->main_code, 0xFF, sizeof u->main_code);
    sv_apply_style(u);
}

/* SELECT: the next or the previous screen */
static void sv_select(sa_ui *u, int d)
{
    int list[SA_LAYERS + 2], n = sv_screens(u, list), at = 0;
    int cur = u->page == SV_PG_MAIN ? -1 : u->layer;
    for (int i = 0; i < n; ++i)
        if (list[i] == cur) at = i;
    if (u->page == SV_PG_KEY || u->page == SV_PG_MENU) at = 0;
    at = sv_clampi(at + d, 0, n - 1);               /* (a fast turn: several screens) */
    if (list[at] < 0) sv_goto(u, SV_PG_MAIN, -1);
    else sv_goto(u, SV_PG_LAYER, list[at]);
}

/* a new layer's SOUND: the drums, then the synth sounds; turning it makes the layer (in the copy) */
static void sv_new_sound(sa_ui *u, int d)
{
    sa_groove *g = u->g;
    sa_code *e = sv_ed(u);
    int n = sv_new_sounds(u), cur;
    if (e->type == SA_T_NONE) cur = d > 0 ? -1 : n;
    else cur = e->type == SA_T_DRUM ? e->sound : SA_DVS + e->sound;
    cur += d;
    if (cur < 0 || cur >= n) {                     /* past either end: no layer again */
        memset(e, 0, sizeof *e);
        return;
    }
    if (cur < SA_DVS) {
        if (e->type != SA_T_DRUM) {
            memset(e, 0, sizeof *e);
            e->arate = SA_RATE_16;
            e->arange = 1;
            e->agate = 60;
        }
        e->type = SA_T_DRUM;
        e->part = SA_PART_DRUMS;
        e->sound = (uint8_t)cur;
    } else {
        if (e->type != SA_T_SYNTH) {
            int p = sa_synth_free_part(g, u->layer);
            if (p < 0) return;
            memset(e, 0, sizeof *e);
            e->part = (uint8_t)p;
            e->arate = SA_RATE_16;
            e->arange = 2;
            e->agate = 60;
        }
        e->type = SA_T_SYNTH;
        e->sound = (uint8_t)(cur - SA_DVS);
    }
    g->s.live[u->layer].mute = 0;
}

/* ALGORITHM on the chip picked */
static void sv_chip_turn(sa_ui *u, int d)
{
    sa_groove *g = u->g;
    sa_code *e = sv_ed(u);
    sv_chip ch[SA_MODS + 4];
    int n = sv_chips(u, ch), type = sv_type(u, u->layer);
    if (n == 0) return;
    sv_chip c = ch[sv_clampi(u->chip, 0, n - 1)];
    u->hot = 9;
    u->hot_ms = 700;
    switch (c.kind) {
    case CH_SOUND: {
        if (sv_is_new(u)) { sv_new_sound(u, d); break; }
        int max = type == SA_T_DRUM ? SA_DVS : SAS_NSOUNDS;
        int v = e->type == SA_T_NONE ? -1 : e->sound;
        v = sv_clampi(v + d, -1, max - 1);          /* -1: REMOVE (the layer goes on the drop) */
        if (v < 0) {
            e->type = SA_T_NONE;
        } else {
            const sa_code *l = sv_live(u);
            if (e->type == SA_T_NONE) {             /* back from REMOVE: the layer as it plays */
                *e = l->type != SA_T_NONE ? *l : *e;
                e->type = (uint8_t)type;
            }
            e->sound = (uint8_t)v;
        }
        break;
    }
    case CH_PAT: {
        int np = type == SA_T_DRUM ? sa_drum_pats() : sa_synth_pats();
        e->pat = (uint8_t)sv_wrap(e->pat + d, np);
        break;
    }
    case CH_MOD: {
        int no = sa_mod_opts(type), i = sa_mod_index(type, e->mod[c.idx]) + d;
        i = sv_clampi(i, 0, no - 1);
        e->mod[c.idx] = sa_mod_opt(type, i);
        if (e->mod[c.idx].type == SA_M_OFF) {       /* OFF: it goes, the ones after it move up */
            for (int k = c.idx; k < SA_MODS - 1; ++k) e->mod[k] = e->mod[k + 1];
            e->mod[SA_MODS - 1].type = SA_M_OFF;
            e->mod[SA_MODS - 1].val = 0;
        }
        break;
    }
    case CH_PLUS: {
        int k = 0;
        while (k < SA_MODS && e->mod[k].type != SA_M_OFF) ++k;
        if (k >= SA_MODS) break;
        e->mod[k] = sa_mod_opt(type, d > 0 ? 1 : sa_mod_opts(type) - 1);
        break;
    }
    default:
        e->amode = (uint8_t)sv_wrap(e->amode + d, SA_AR_MODES);
        break;
    }
    (void)g;
    u->changed = true;
}

void sv_knob(sa_ui *u, int k, int d)
{
    sa_groove *g = u->g;
    sa_settings *s = &g->s;
    if (k == SV_K_SELECT) {
        sv_select(u, d);
        return;
    }
    if (k == SV_K_PRESETS) {
        if (u->page == SV_PG_LAYER) {
            sv_chip ch[SA_MODS + 4];
            int n = sv_chips(u, ch);
            u->chip = sv_clampi(u->chip + d, 0, n - 1);
            u->hot = 9;
            u->hot_ms = 700;
        }
        return;
    }
    if (k == SV_K_ALGO) {
        if (u->page == SV_PG_LAYER) sv_chip_turn(u, d);
        return;
    }
    if (k < SV_K1 || k > SV_K4) return;
    int i = k - SV_K1;
    u->hot = i;
    u->hot_ms = 700;
    u->changed = true;
    switch (u->page) {
    case SV_PG_MAIN:
        if (i == 0) { s->bpm = (uint8_t)sv_clampi(s->bpm + d, 60, 200); u->hot = 8; u->hot_ms = 700; }
        else if (i == 1) s->filter = (uint8_t)sv_clampi(s->filter + d * 2, 0, 100);
        else if (i == 2) s->feel = (uint8_t)sv_clampi(s->feel + d * 2, 0, 60);
        else s->kit = (uint8_t)sv_clampi(s->kit + d, 0, SA_KITS - 1);
        break;
    case SV_PG_LAYER: {
        int l = u->layer, type = sv_type(u, l);
        sa_live *v = &s->live[l];
        if (sv_is_new(u)) {
            sa_code *e = sv_ed(u);
            if (i == 0) sv_new_sound(u, d);
            else if (i == 1 && e->type != SA_T_NONE)
                e->pat = (uint8_t)sv_wrap(e->pat + d, e->type == SA_T_DRUM ? sa_drum_pats() : sa_synth_pats());
            break;
        }
        if (type == SA_T_SYNTH) {
            uint8_t *f = i == 0 ? &v->cut : i == 1 ? &v->reso : i == 2 ? &v->drive : &v->env;
            *f = (uint8_t)sv_clampi(*f + d * 2, 0, 100);
        } else {
            uint8_t *f = i == 0 ? &v->level : i == 1 ? &s->ktune : i == 2 ? &s->ktone : &s->kdecay;
            *f = (uint8_t)sv_clampi(*f + d * 2, 0, 100);
        }
        break;
    }
    case SV_PG_ARP: {
        sa_code *e = sv_ed(u);
        if (i == 0) e->amode = (uint8_t)sv_wrap(e->amode + d, SA_AR_MODES);
        else if (i == 1) e->arate = (uint8_t)sv_clampi(e->arate + d, 0, SA_RATES - 1);
        else if (i == 2) e->arange = (uint8_t)sv_clampi(e->arange + d, 1, 4);
        else e->agate = (uint8_t)sv_clampi(e->agate + d * 5, 10, 100);
        break;
    }
    case SV_PG_KEY: {
        sa_harm *h = &g->ed_harm;
        if (i == 0) h->key = (uint8_t)sv_wrap(h->key + d, 12);
        else if (i == 1) h->scale = (uint8_t)sv_clampi(h->scale + d, 0, SA_SCALES - 1);
        else if (i == 2) h->prog = (uint8_t)sv_wrap(h->prog + d, SA_PROGS);
        else { static const uint8_t BARS[3] = {1, 2, 4}; int b = h->bars >= 4 ? 2 : h->bars >= 2 ? 1 : 0;
               h->bars = BARS[sv_clampi(b + d, 0, 2)]; }
        break;
    }
    default:                                       /* MENU */
        if (i == 0) s->palette = (uint8_t)sv_wrap(s->palette + d, (int)NPALETTES);
        else if (i == 1) s->look = (uint8_t)!s->look;
        sv_apply_style(u);
        break;
    }
}

void sv_button(sa_ui *u, int b, bool down)
{
    sa_groove *g = u->g;
    if (b == SV_B_HOME) {
        if (down) {
            u->home_down = true;
            u->home_ms = 0;
        } else if (u->home_down) {
            u->home_down = false;
            if (u->home_ms < SV_MENU_HOLD_MS) sv_goto(u, SV_PG_MAIN, -1);
        }
        return;
    }
    if (!down)
        return;
    switch (b) {
    case SV_B_PLAY: sa_play(g, !g->playing); break;
    case SV_B_REC:
        sa_drop(g);
        u->changed = true;
        break;
    case SV_B_FX:
        if ((u->page == SV_PG_LAYER || u->page == SV_PG_ARP) && !sv_is_new(u)) {
            bool m = !g->s.live[u->layer].mute;
            sa_mute(g, u->layer, m);
            sv_msg(u, m ? "MUTED" : "BACK IN");
            u->changed = true;
        }
        break;
    case SV_B_ARP:
        if (u->page == SV_PG_ARP) sv_goto(u, SV_PG_LAYER, u->layer);
        else if (u->page == SV_PG_LAYER && sv_type(u, u->layer) == SA_T_SYNTH) sv_goto(u, SV_PG_ARP, u->layer);
        else if (u->page == SV_PG_LAYER) sv_msg(u, "ARP: SYNTH LAYERS");
        break;
    case SV_B_SCL:
        if (u->page == SV_PG_KEY) {
            if (u->back >= 0) sv_goto(u, SV_PG_LAYER, u->back);
            else sv_goto(u, SV_PG_MAIN, -1);
        } else {
            u->back = u->page == SV_PG_MAIN || u->page == SV_PG_MENU ? -1 : u->layer;
            sv_goto(u, SV_PG_KEY, u->layer);
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
    int layer = u->page == SV_PG_LAYER || u->page == SV_PG_ARP ? u->layer : -1;
    if (down) {
        u->key_sent[k] = note + 12 * u->oct;
        sa_key(u->g, k, u->key_sent[k], true, layer);
    } else {
        sa_key(u->g, k, u->key_sent[k], false, layer);
        u->key_sent[k] = -1;
    }
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
    if (u->home_down) {
        u->home_ms += ms;
        if (u->home_ms >= SV_MENU_HOLD_MS && u->page != SV_PG_MENU) {
            sv_goto(u, SV_PG_MENU, -1);
            u->home_down = false;
        }
    }
}
