/* SPDX-License-Identifier: GPL-3.0-only */
/* SARDINE's screen and controls on the host: firmware/src/sa_ui.c drawn by Felucca's gfx.c into a 240 x 240 frame,
 * the layers running (their notes counted, no sound), the controls driven as a player would, every page in every
 * palette written out, and the layout lint (from Tuna's and Flyfish's UI tests): every text inside its canvas, not
 * cut, not ellipsised unless it is a name, no two texts of one canvas overlapping.
 *   tests/sa_ui_test.sh [OUTDIR]     (exit 0: all passed; PNGs and a GIF of MAIN under OUTDIR) */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails, checks;
#define CHECK(cond, ...)                                                   \
    do {                                                                   \
        checks++;                                                          \
        if (!(cond)) {                                                     \
            fails++;                                                       \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                    \
            printf(__VA_ARGS__);                                           \
            printf("\n");                                                  \
        }                                                                  \
    } while (0)

/* ---- the layout lint: the ink boxes of one canvas, checked when it is blitted -------------------------- */
typedef struct { int x0, y0, x1, y1, flags; char s[24]; } box;
static box boxes[256];
static int nbox;
static const char *lint_where = "";
static int lint_findings;
static void hook_text(int x0, int y0, int x1, int y1, const char *s, int flags)
{
    if (nbox < 256) {
        box *b = &boxes[nbox++];
        b->x0 = x0; b->y0 = y0; b->x1 = x1; b->y1 = y1; b->flags = flags;
        strncpy(b->s, s, sizeof b->s - 1);
        b->s[sizeof b->s - 1] = 0;
    }
}
#define GFX_HOOK_TEXT(x0, y0, x1, y1, s, flags) hook_text((x0), (y0), (x1), (y1), (s), (flags))
#define GFX_HOOK_BLIT(x, y, r0) ((void)0)
#define GFX_HOOK_BEGIN() (nbox = 0)
#define GFX_HOOK_PIXELS(n) ((void)0)

static uint16_t fb[240 * 240];
static void lint_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    for (int i = 0; i < nbox; ++i) {
        box *b = &boxes[i];
        if (b->x1 <= b->x0)
            continue;
        bool name = (b->flags & 8) != 0;            /* free text (a name, a message) may be ellipsised */
        if (b->x0 < 0 || b->y0 < 0 || b->x1 > (int)w || b->y1 > (int)h) {
            printf("LINT %s: \"%s\" leaves its canvas (%d,%d..%d,%d of %ux%u at %u,%u)\n", lint_where, b->s, b->x0,
                   b->y0, b->x1, b->y1, w, h, x, y);
            lint_findings++;
        }
        if ((int)x + b->x1 > 240 || (int)y + b->y1 > 240) {
            printf("LINT %s: \"%s\" off the screen\n", lint_where, b->s);
            lint_findings++;
        }
        if ((b->flags & 1) && !name) {
            printf("LINT %s: \"%s\" ellipsised\n", lint_where, b->s);
            lint_findings++;
        }
        for (int j = i + 1; j < nbox; ++j) {
            box *c = &boxes[j];
            if (c->x1 <= c->x0)
                continue;
            if (b->x0 < c->x1 && c->x0 < b->x1 && b->y0 < c->y1 && c->y0 < b->y1) {
                printf("LINT %s: \"%s\" overlaps \"%s\" (%d,%d..%d,%d / %d,%d..%d,%d)\n", lint_where, b->s, c->s, b->x0,
                       b->y0, b->x1, b->y1, c->x0, c->y0, c->x1, c->y1);
                lint_findings++;
            }
        }
    }
    nbox = 0;
}
static void lcd_sync(void) {}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{
    lint_blit(x, y, w, h);
    for (uint32_t j = 0; j < h && y + j < 240; j++)
        for (uint32_t i = 0; i < w && x + i < 240; i++)
            fb[(y + j) * 240 + x + i] = p[j * w + i];
}
#define main hostsim_main
#include "hostsim.c"                               /* (core.h, the matrix: sa_sounds.h's track settings) */
#undef main
#include "gfx.c"
#include "sa_layer.h"
#include "sa_sounds.h"
#include "sa_ui.c"

static const char *outdir;
static int frames_written;
static void write_ppm(const char *name)
{
    if (!outdir) return;
    char path[512];
    snprintf(path, sizeof path, "%s/ppm/%s.ppm", outdir, name);
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6 240 240 255\n");
    for (int i = 0; i < 240 * 240; i++) {
        uint16_t c = (uint16_t)((fb[i] >> 8) | (fb[i] << 8));
        uint8_t px[3] = {(uint8_t)((c >> 11) << 3 | (c >> 13)), (uint8_t)(((c >> 5) & 63) << 2 | ((c >> 9) & 3)),
                         (uint8_t)((c & 31) << 3 | ((c >> 2) & 7))};
        fwrite(px, 1, 3, f);
    }
    fclose(f);
    frames_written++;
}

static int notes_on, notes_off;
static int part_on[SA_PARTS];
static void sound(void *ctx, const sa_event *e)
{
    (void)ctx;
    if (e->on) { notes_on++; part_on[e->part % SA_PARTS]++; }
    else notes_off++;
}

static sa_groove g;
static sa_ui ui;
static void run_ms(int ms)
{
    for (int t = 0; t < ms; t += 16) {
        sa_process(&g, 44100 * 16 / 1000);
        sv_tick(&ui, 16);
        sv_draw(&ui);
    }
}
static void press(int b) { sv_button(&ui, b, true); sv_button(&ui, b, false); }
static void shot(const char *name)
{
    lint_where = name;
    sv_draw_all(&ui);
    write_ppm(name);
}
static void all_palettes(const char *page)
{
    int keep = g.s.palette, keep_st = g.s.look;
    for (int p = 0; p < (int)NPALETTES; ++p)
        for (int st = 0; st < 2; ++st) {
            char name[64];
            g.s.palette = (uint8_t)p;
            g.s.look = (uint8_t)st;
            sv_apply_style(&ui);
            snprintf(name, sizeof name, "%s%s_%s", st ? "LINE_" : "", UI_PALETTES[p].name, page);
            shot(name);
        }
    g.s.palette = (uint8_t)keep;
    g.s.look = (uint8_t)keep_st;
    sv_apply_style(&ui);
}
static void bars(int n)
{
    int b = g.bar + n;
    for (int k = 0; k < 4000 && g.bar < b; ++k)
        run_ms(16);
}
static void go_layer(int l)                       /* SELECT until layer l is open */
{
    sv_button(&ui, SV_B_HOME, true);
    sv_button(&ui, SV_B_HOME, false);
    for (int k = 0; k < 16 && !(ui.page == SV_PG_LAYER && ui.layer == l); ++k)
        sv_knob(&ui, SV_K_SELECT, 1);
}

int main(int argc, char **argv)
{
    outdir = argc > 1 ? argv[1] : 0;
    sa_init(&g, 44100.0f);
    sa_set_sound(&g, sound, 0);
    g.s.palette = (uint8_t)UI_SHOAL_INDEX;
    sv_init(&ui, &g);
    CHECK(!strcmp(UI_PALETTES[UI_SHOAL_INDEX].name, "SHOAL"), "SHOAL is Sardine's palette");
    CHECK(SAS_NSOUNDS >= 60, "a ton of synth sounds (%d)", SAS_NSOUNDS);
    shot("01_main_stopped");
    all_palettes("main_stopped");
    press(SV_B_PLAY);
    CHECK(g.playing, "PLAY plays");
    bars(2);
    CHECK(part_on[SA_PART_DRUMS] && part_on[0] && part_on[1], "the first groove plays (%d %d %d %d)", part_on[0],
          part_on[1], part_on[2], part_on[3]);
    shot("02_main");
    all_palettes("main");
    for (int f = 0; f < 40; ++f) {                 /* MAIN as it plays, a frame every 48 ms */
        char nm[32];
        run_ms(48);
        snprintf(nm, sizeof nm, "anim_%03d", f);
        lint_where = nm;
        write_ppm(nm);
    }
    /* MAIN's knobs act at once */
    sv_knob(&ui, SV_K1, 2);
    CHECK(g.s.bpm == 128, "TEMPO +2 (%d)", g.s.bpm);
    sv_knob(&ui, SV_K4, 3);
    CHECK(g.s.kit == SA_KIT_HARD, "KIT +3: HARD (%d)", g.s.kit);
    CHECK(sa_pending(&g) == 0, "MAIN's knobs are not code");
    /* SPACE: HOME on MAIN; the pump follows the kick */
    press(SV_B_HOME);
    CHECK(ui.page == SV_PG_MAIN && ui.space, "HOME on MAIN: SPACE");
    sv_knob(&ui, SV_K1, 10);
    CHECK(g.s.pump == 55, "PUMP +10 steps of 2 (%d)", g.s.pump);
    {
        int maxd = 0;
        for (int t = 0; t < 2000; t += 16) { run_ms(16); if (g.duck > maxd) maxd = g.duck; }
        CHECK(maxd > 40, "the synth layers duck under the kick (%d)", maxd);
    }
    shot("02b_space");
    all_palettes("space");
    press(SV_B_HOME);
    CHECK(!ui.space, "HOME again: MAIN");
    /* SELECT: to the bass layer (4) */
    go_layer(3);
    CHECK(ui.page == SV_PG_LAYER && ui.layer == 3, "SELECT reaches layer 4");
    shot("03_layer_bass");
    all_palettes("layer_bass");
    sv_knob(&ui, SV_K1, -10);
    CHECK(g.s.live[3].cut == 80, "CUTOFF at once (%d)", g.s.live[3].cut);
    {                                              /* the PUMP chip: off and on at once, not code */
        sv_chip ch[SA_MODS + 5];
        int n = sv_chips(&ui, ch);
        CHECK(ch[n - 1].kind == CH_PUMP, "the last chip of a synth layer: PUMP");
        ui.chip = n - 1;
        sv_knob(&ui, SV_K_ALGO, -1);
        CHECK(g.s.live[3].nopump && sa_pending(&g) == 0, "PUMP OFF at once");
        sv_knob(&ui, SV_K_ALGO, 1);
        CHECK(!g.s.live[3].nopump, "PUMP ON again");
        ui.chip = 0;
    }
    CHECK(sa_pending(&g) == 0, "the tone is not code");
    /* the code: PRESETS to the pattern chip, ALGORITHM to the next pattern; then a modifier on + */
    sv_knob(&ui, SV_K_PRESETS, 1);
    sv_knob(&ui, SV_K_ALGO, 1);
    CHECK(g.ed[3].pat == 2 && g.s.code[3].pat == 1, "the pattern changes in the copy only");
    CHECK(sa_pending(&g) == 1, "one layer pending");
    sv_knob(&ui, SV_K_PRESETS, 1);                 /* the + chip */
    sv_knob(&ui, SV_K_ALGO, 1);
    CHECK(g.ed[3].mod[0].type == SA_M_FAST && g.ed[3].mod[0].val == 2, "+ makes FAST 2");
    sv_knob(&ui, SV_K_ALGO, 1);
    CHECK(g.ed[3].mod[0].type == SA_M_FAST && g.ed[3].mod[0].val == 3, "then FAST 3");
    sv_knob(&ui, SV_K_ALGO, -1);
    shot("04_layer_pending");
    all_palettes("layer_pending");
    press(SV_B_REC);
    CHECK(g.drop_armed, "REC arms the drop");
    shot("05_layer_armed");
    bars(1);
    CHECK(!g.drop_armed && g.s.code[3].pat == 2 && sa_pending(&g) == 0, "dropped on the 1");
    /* the ARP page */
    press(SV_B_ARP);
    CHECK(ui.page == SV_PG_ARP, "ARP: the arp page");
    sv_knob(&ui, SV_K1, 5);
    CHECK(g.ed[3].amode == SA_AR_CONVERGE, "MODE +5: CONVERGE (%d)", g.ed[3].amode);
    sv_knob(&ui, SV_K2, 1);
    sv_knob(&ui, SV_K3, 1);
    shot("06_arp");
    all_palettes("arp");
    press(SV_B_ARP);
    CHECK(ui.page == SV_PG_LAYER, "ARP again: back");
    press(SV_B_REC);
    bars(1);
    /* FX mutes at once */
    press(SV_B_FX);
    CHECK(g.s.live[3].mute, "FX mutes the layer");
    shot("07_layer_muted");
    press(SV_B_FX);
    /* DRUMS: every drum on one screen, the step keys */
    sv_button(&ui, SV_B_HOME, true);
    sv_button(&ui, SV_B_HOME, false);
    sv_knob(&ui, SV_K_SELECT, 1);
    CHECK(ui.page == SV_PG_DRUMS, "SELECT after MAIN: DRUMS (%d)", ui.page);
    shot("08_drums");
    all_palettes("drums");
    sv_knob(&ui, SV_K2, 3);
    CHECK(g.s.ktune == 56, "TUNE moves the kit (%d)", g.s.ktune);
    sv_knob(&ui, SV_K_PRESETS, 1);                 /* the HAT row */
    CHECK(sv_drow_layer(&ui) == 1, "PRESETS picks the hat row (%d)", sv_drow_layer(&ui));
    {
        uint32_t full, dim;
        sv_draw(&ui);
        CHECK(sv_key_leds(&ui, &full, &dim) && dim == 0xFFFFu && full == ((1u << 2) | (1u << 6) | (1u << 10) | (1u << 14)),
              "the step keys dim, the hat's steps full (%x %x)", full, dim);
        sv_key(&ui, SV_KEY_LO + 6, true);          /* step 7 off */
        sv_key(&ui, SV_KEY_LO + 6, false);
        sv_key(&ui, SV_KEY_LO + 15, true);         /* step 16 on */
        sv_key(&ui, SV_KEY_LO + 15, false);
        sv_draw(&ui);
        sv_key_leds(&ui, &full, &dim);
        CHECK(g.ed[1].own && g.ed[1].hits == ((1u << 2) | (1u << 10) | (1u << 14) | (1u << 15)), "keys edit the hat (%04x)",
              g.ed[1].hits);
        CHECK(full == g.ed[1].hits, "the keys show the edit (%x)", full);
    }
    shot("09_drums_edited");
    all_palettes("drums_edited");
    press(SV_B_REC);
    bars(1);
    CHECK(g.s.code[1].own && sa_pending(&g) == 0, "the hat's edit dropped");
    sv_knob(&ui, SV_K_ALGO, 1);                    /* a ready pattern again */
    CHECK(!g.ed[1].own && g.ed[1].pat == 2, "ALGORITHM: the next ready pattern (%d)", g.ed[1].pat);
    sv_knob(&ui, SV_K_ALGO, -1);
    /* + ADD A DRUM */
    sv_knob(&ui, SV_K_PRESETS, 9);
    CHECK(sv_drow_layer(&ui) < 0, "the last row: + ADD A DRUM");
    shot("10_drums_add");
    sv_knob(&ui, SV_K_ALGO, 10);                   /* a new drum: the conga (and another turn: still the new one) */
    int nl = -1;
    for (int l = 0; l < SA_LAYERS; ++l) if (g.ed[l].type == SA_T_DRUM && g.s.code[l].type == SA_T_NONE) nl = l;
    CHECK(nl >= 0, "ADD: a drum layer in the copy");
    sv_knob(&ui, SV_K_PRESETS, -9);
    sv_knob(&ui, SV_K_PRESETS, 3);
    CHECK(sv_drow_layer(&ui) == nl, "the new drum is a row (%d %d)", sv_drow_layer(&ui), nl);
    sv_knob(&ui, SV_K_ALGO, 10);
    CHECK(g.ed[nl].sound == (9 + 10) % SA_DVS, "a drum not yet dropped: ALGORITHM changes the drum (%d)", g.ed[nl].sound);
    shot("11_drums_added");
    press(SV_B_REC);
    bars(1);
    /* EDIT: the row as a layer screen, its modifiers; EDIT again: back */
    press(SV_B_EDIT);
    CHECK(ui.page == SV_PG_LAYER && ui.layer == nl, "EDIT opens the row");
    shot("12_drum_layer");
    press(SV_B_EDIT);
    CHECK(ui.page == SV_PG_DRUMS, "EDIT again: DRUMS");
    /* STEPS on the bass: notes on the keys */
    go_layer(3);
    press(SV_B_SEQ);
    CHECK(ui.page == SV_PG_STEPS, "SEQ: STEPS");
    sv_key(&ui, 57, true);                          /* A3 on step 1, the cursor on */
    sv_key(&ui, 57, false);
    CHECK(g.ed[3].own && g.ed[3].sn[0] == 57 - g.ed_harm.key && ui.scur == 1, "a key writes the step (%d %d)",
          g.ed[3].sn[0], ui.scur);
    sv_knob(&ui, SV_K1, -1);
    sv_knob(&ui, SV_K2, 3);
    CHECK(g.ed[3].sl[0] == 4, "KNOB 2: its length (%d)", g.ed[3].sl[0]);
    {
        uint32_t full, dim;
        CHECK(sv_key_leds(&ui, &full, &dim) && (full >> (57 - SV_KEY_LO)) & 1u, "the step's note lit full");
    }
    shot("13_steps");
    all_palettes("steps");
    press(SV_B_SEQ);
    CHECK(ui.page == SV_PG_LAYER, "SEQ again: back");
    /* GATE: ENV on a synth layer */
    press(SV_B_ENV);
    CHECK(ui.page == SV_PG_GATE, "ENV: GATE");
    sv_knob(&ui, SV_K1, 5);
    CHECK(g.ed_gate[3].pat == 5, "KNOB 1: TRANCE A (%d)", g.ed_gate[3].pat);
    sv_key(&ui, SV_KEY_LO + 1, true);
    sv_key(&ui, SV_KEY_LO + 1, false);
    CHECK(g.ed_gate[3].own && g.ed_gate[3].mask == (0x6D6D ^ 2), "a key edits the gate (%04x)", g.ed_gate[3].mask);
    {
        uint32_t full, dim;
        CHECK(sv_key_leds(&ui, &full, &dim) && dim == 0xFFFFu && full == g.ed_gate[3].mask, "the gate's keys lit (%x)", full);
    }
    sv_knob(&ui, SV_K3, -4);
    shot("13b_gate");
    all_palettes("gate");
    press(SV_B_ENV);
    CHECK(ui.page == SV_PG_LAYER, "ENV again: back");
    /* PAN: LFO on a synth layer, at once */
    press(SV_B_LFO);
    CHECK(ui.page == SV_PG_PAN, "LFO: PAN");
    int pend_before = sa_pending(&g);
    sv_knob(&ui, SV_K2, -10);
    CHECK(g.s.pan[3].pos == -20 && sa_pending(&g) == pend_before, "PAN L20 at once, not code (%d)", g.s.pan[3].pos);
    run_ms(100);
    CHECK(g.pan_now[0] < -20, "the part pans left (%d)", g.pan_now[0]);
    sv_knob(&ui, SV_K1, 1);
    CHECK(g.s.pan[3].mode == SA_PAN_SWEEP, "MODE: SWEEP");
    {
        int lo = 64, hi = -64;
        for (int t = 0; t < 2500; t += 16) { run_ms(16); if (g.pan_now[0] < lo) lo = g.pan_now[0]; if (g.pan_now[0] > hi) hi = g.pan_now[0]; }
        CHECK(hi - lo > 30, "SWEEP moves it (%d .. %d)", lo, hi);
    }
    shot("13c_pan");
    all_palettes("pan");
    sv_knob(&ui, SV_K1, 1);
    {
        int lo = 64, hi = -64;
        for (int t = 0; t < 2500; t += 16) { run_ms(16); if (g.pan_now[0] < lo) lo = g.pan_now[0]; if (g.pan_now[0] > hi) hi = g.pan_now[0]; }
        CHECK(g.s.pan[3].mode == SA_PAN_PINGPONG && hi - lo > 30, "PING-PONG: from side to side (%d .. %d)", lo, hi);
    }
    sv_knob(&ui, SV_K1, -2);
    sv_knob(&ui, SV_K2, 10);
    press(SV_B_LFO);
    CHECK(ui.page == SV_PG_LAYER, "LFO again: back");
    press(SV_B_REC);
    bars(1);
    /* every chip text on the lint: every sound, every pattern, every modifier */
    go_layer(3);
    {
        sa_code keep = g.ed[3];
        for (int i = 0; i < SAS_NSOUNDS; ++i) {
            char nm[48];
            g.ed[3].sound = (uint8_t)i;
            snprintf(nm, sizeof nm, "z_sound_%02d", i);
            lint_where = nm;
            sv_draw_all(&ui);
        }
        g.ed[3] = keep;
        for (int i = 0; i < sa_synth_pats(); ++i) {
            char nm[48];
            g.ed[3].pat = (uint8_t)i;
            snprintf(nm, sizeof nm, "z_pat_%02d", i);
            lint_where = nm;
            sv_draw_all(&ui);
        }
        for (int i = 0; i < sa_mod_opts(SA_T_SYNTH); ++i) {
            char nm[48];
            for (int k = 0; k < SA_MODS; ++k) g.ed[3].mod[k] = sa_mod_opt(SA_T_SYNTH, (i + k * 9) % sa_mod_opts(SA_T_SYNTH));
            if (g.ed[3].mod[0].type == SA_M_OFF) g.ed[3].mod[0] = sa_mod_opt(SA_T_SYNTH, 1);
            snprintf(nm, sizeof nm, "z_mod_%02d", i);
            lint_where = nm;
            sv_draw_all(&ui);
            if (i == 20) write_ppm("19_layer_four_mods");
        }
        g.ed[3] = keep;
    }
    /* the KEY page: code too */
    press(SV_B_SCL);
    CHECK(ui.page == SV_PG_KEY, "SCL: KEY");
    sv_knob(&ui, SV_K1, 2);
    CHECK(g.ed_harm.key == 11 && g.s.harm.key == 9, "KEY in the copy (%d %d)", g.ed_harm.key, g.s.harm.key);
    shot("14_key");
    all_palettes("key");
    press(SV_B_REC);
    bars(1);
    CHECK(g.s.harm.key == 11, "the key dropped");
    press(SV_B_SCL);
    CHECK(ui.page == SV_PG_LAYER && ui.layer == 3, "SCL again: back to the layer");
    /* NEW: past the last layer, a synth layer while a part is free */
    sv_button(&ui, SV_B_HOME, true);
    sv_button(&ui, SV_B_HOME, false);
    for (int k = 0; k < 16 && !sv_is_new(&ui); ++k)
        sv_knob(&ui, SV_K_SELECT, 1);
    CHECK(sv_is_new(&ui) && sv_free_slot(&ui) == ui.layer, "SELECT past the last: NEW (layer %d)", ui.layer);
    shot("15_new");
    all_palettes("new");
    int newl = ui.layer;
    sv_knob(&ui, SV_K1, 4);                        /* the fourth synth sound */
    CHECK(g.ed[newl].type == SA_T_SYNTH && g.ed[newl].part == 2 && g.ed[newl].sound == 3,
          "a new synth layer on the free part (%d %d %d)", g.ed[newl].type, g.ed[newl].part, g.ed[newl].sound);
    shot("16_new_made");
    press(SV_B_REC);
    bars(2);
    CHECK(part_on[2] > 0, "the new layer plays on part 3 (%d)", part_on[2]);
    /* three synth layers: no NEW any more */
    sv_button(&ui, SV_B_HOME, true);
    sv_button(&ui, SV_B_HOME, false);
    for (int k = 0; k < 16 && !sv_is_new(&ui); ++k)
        sv_knob(&ui, SV_K_SELECT, 1);
    CHECK(!sv_is_new(&ui), "three synth layers: no NEW screen");
    /* MAIN with more drums and a mute */
    sv_button(&ui, SV_B_HOME, true);
    sv_button(&ui, SV_B_HOME, false);
    sv_knob(&ui, SV_K_SELECT, 1);
    for (int k = 0; k < 2; ++k) {
        sv_knob(&ui, SV_K_PRESETS, 12);
        sv_knob(&ui, SV_K_ALGO, 5 + k);
    }
    press(SV_B_REC);
    bars(1);
    sa_mute(&g, 1, true);
    press(SV_B_HOME);
    bars(1);
    shot("17_main_full");
    all_palettes("main_full");
    /* the keys play the open layer */
    go_layer(3);
    int before = part_on[0];
    sv_key(&ui, 57, true);
    CHECK(part_on[0] == before + 1, "a key plays the bass layer");
    sv_key(&ui, 57, false);
    /* MENU */
    sv_button(&ui, SV_B_HOME, true);
    run_ms(700);
    sv_button(&ui, SV_B_HOME, false);
    CHECK(ui.page == SV_PG_MENU, "HOME held: MENU");
    shot("18_menu");
    all_palettes("menu");
    press(SV_B_HOME);
    press(SV_B_PLAY);
    CHECK(!g.playing, "PLAY stops");
    CHECK(g.nheld == 0, "nothing left sounding");
    CHECK(lint_findings == 0, "the layout lint: %d findings", lint_findings);
    printf("sa_ui: %d checks, %d failed, %d frames, %d notes on (parts %d %d %d, drums %d)\n", checks, fails,
           frames_written, notes_on, part_on[0], part_on[1], part_on[2], part_on[3]);
    return fails ? 1 : 0;
}
