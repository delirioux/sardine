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
static void to_view(int v)                        /* PRESETS to a layer (from HOME) */
{
    press(SV_B_HOME);
    sv_knob(&ui, SV_K_PRESETS, v);
}
static void fx_to(int pg)                         /* FX until the page */
{
    for (int k = 0; k < 8 && sv_page_now(&ui) != pg; ++k) press(SV_B_FX);
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
    shot("01_home_stopped");
    press(SV_B_PLAY);
    CHECK(g.playing, "PLAY plays");
    bars(2);
    CHECK(part_on[SA_PART_DRUMS] && part_on[SA_L_CHORD] && part_on[SA_L_BASS] && !part_on[SA_L_LEAD],
          "the first groove: drums, chord, bass; LEAD OFF (%d %d %d %d)", part_on[0], part_on[1], part_on[2], part_on[3]);
    shot("02_home");
    all_palettes("home");
    for (int f = 0; f < 40; ++f) {                 /* HOME as it plays, a frame every 48 ms */
        char nm[32];
        run_ms(48);
        snprintf(nm, sizeof nm, "anim_%03d", f);
        lint_where = nm;
        write_ppm(nm);
    }
    /* SELECT is the tempo everywhere; HOME's knobs act at once */
    sv_knob(&ui, SV_K_SELECT, 2);
    CHECK(g.s.bpm == 128, "SELECT: the tempo +2 (%d)", g.s.bpm);
    sv_knob(&ui, SV_K4, -5);
    CHECK(g.s.reverb == 60 && g.s.delay == 60, "SPACE: reverb and delay (%d %d)", g.s.reverb, g.s.delay);
    sv_knob(&ui, SV_K3, 5);
    CHECK(g.s.pump == 45, "PUMP (%d)", g.s.pump);
    /* PRESETS at the ends: nothing to redraw */
    sv_draw(&ui);
    sv_knob(&ui, SV_K_PRESETS, -1);
    CHECK(ui.page == SV_PG_MAIN && !ui.force, "PRESETS down on HOME: no redraw");
    press(SV_B_HOME);
    CHECK(ui.page == SV_PG_MAIN && !ui.force, "HOME on HOME: no redraw");
    /* PRESETS: HOME LEAD CHORD BASS DRUMS, each on its home page */
    sv_knob(&ui, SV_K_PRESETS, 1);
    CHECK(ui.page == SV_PG_LAYER && ui.view == SV_V_LEAD && ui.layer == SA_L_LEAD && ui.fx == 0, "PRESETS: LEAD");
    shot("03_lead_off");
    sv_knob(&ui, SV_K_PRESETS, 1);
    CHECK(ui.view == SV_V_CHORD && ui.layer == SA_L_CHORD, "PRESETS: CHORD");
    shot("04_chord");
    all_palettes("chord");
    sv_knob(&ui, SV_K_PRESETS, 1);
    CHECK(ui.view == SV_V_BASS && ui.layer == SA_L_BASS, "PRESETS: BASS");
    sv_key(&ui, 57, true);
    CHECK(ui.msg_ms > 0 && !strncmp(ui.msg, "KEYS: BASS, ", 12), "a key: what it plays (%s)", ui.msg);
    sv_key(&ui, 57, false);
    {
        uint32_t seen = 0;
        for (int k = 0; k < 60; ++k) { run_ms(16); seen |= sv_play_keys(&ui); }
        CHECK(seen != 0, "BASS's notes light on the keys (%07x)", seen);
    }
    int cut = g.s.live[SA_L_BASS].cut;
    sv_knob(&ui, SV_K1, -10);
    CHECK(g.s.live[SA_L_BASS].cut == cut - 20, "BASS's CUTOFF at once");
    /* ALGORITHM: the pattern, in the copy; SEQ drops it on the bar line */
    int pat = g.s.code[SA_L_BASS].pat;
    sv_knob(&ui, SV_K_ALGO, 2);
    CHECK(g.ed[SA_L_BASS].pat != pat && g.s.code[SA_L_BASS].pat == pat && sa_pending(&g) == 1, "ALGORITHM: waits");
    shot("05_bass_pending");
    press(SV_B_REC);
    CHECK(ui.rec && sa_pending(&g) == 1, "REC is step recording, not a drop");
    press(SV_B_REC);
    press(SV_B_SEQ);
    CHECK(g.drop_armed, "SEQ: armed");
    bars(1);
    CHECK(g.s.code[SA_L_BASS].pat == g.ed[SA_L_BASS].pat && !sa_pending(&g), "dropped on the bar line");
    /* FX through BASS's pages and back to its home; another layer opens on its home */
    press(SV_B_FX);
    CHECK(sv_page_now(&ui) == SVP_MIX, "FX: the first page, VOLUME TYPE INSTR MUTE");
    int snd = g.s.code[SA_L_BASS].sound;
    sv_knob(&ui, SV_K3, 1);
    CHECK(g.s.code[SA_L_BASS].sound != snd && g.ed[SA_L_BASS].sound == g.s.code[SA_L_BASS].sound && !sa_pending(&g),
          "INSTR: at once, nothing waits");
    sv_knob(&ui, SV_K4, 1);
    CHECK(g.s.live[SA_L_BASS].mute, "MUTE at once");
    shot("06_bass_mix");
    sv_knob(&ui, SV_K4, -1);
    sv_knob(&ui, SV_K1, -10);
    CHECK(g.s.live[SA_L_BASS].level == 60, "VOLUME (%d)", g.s.live[SA_L_BASS].level);
    press(SV_B_FX);
    CHECK(sv_page_now(&ui) == SVP_ARP && g.ed[SA_L_BASS].amode == SA_AR_OFF, "FX: ARP on BASS too, OFF");
    press(SV_B_FX);
    CHECK(sv_page_now(&ui) == SVP_PAN, "FX: PAN");
    shot("07_bass_pan");
    press(SV_B_FX);
    CHECK(sv_page_now(&ui) == SVP_3D, "FX: 3D");
    sv_knob(&ui, SV_K2, 27);
    sv_knob(&ui, SV_K3, 4);
    sv_knob(&ui, SV_K4, 10);
    CHECK(g.s.d3[SA_L_BASS].on == SA_3D_FIXED && g.s.d3[SA_L_BASS].az == 27 && g.s.d3[SA_L_BASS].el == 20 &&
          g.s.d3[SA_L_BASS].dist == 50, "3D: ANGLE HEIGHT DISTANCE turn it on, FIXED");
    run_ms(50);
    CHECK(g.pan_now[SA_L_BASS] > 30, "3D: behind on the right pans right (%d)", g.pan_now[SA_L_BASS]);
    shot("07b_bass_3d");
    all_palettes("bass_3d");
    sv_knob(&ui, SV_K1, 1);
    CHECK(g.s.d3[SA_L_BASS].on == SA_3D_ORBIT, "MODE: ORBIT");
    sv_knob(&ui, SV_K2, -3);
    CHECK(g.s.d3rate[SA_L_BASS] == SA_PRATE_QUARTER, "ORBIT: RATE a quarter bar (%d)", g.s.d3rate[SA_L_BASS]);
    {
        int lo = 99, hi = -99;
        for (int k = 0; k < 80; ++k) {
            run_ms(16);
            if (g.pan_now[SA_L_BASS] < lo) lo = g.pan_now[SA_L_BASS];
            if (g.pan_now[SA_L_BASS] > hi) hi = g.pan_now[SA_L_BASS];
        }
        CHECK(lo < -40 && hi > 40, "ORBIT: round you, left and right (%d %d)", lo, hi);
    }
    shot("07d_bass_orbit");
    sv_knob(&ui, SV_K1, -5);
    CHECK(!g.s.d3[SA_L_BASS].on, "3D OFF");
    press(SV_B_FX);
    CHECK(sv_page_now(&ui) == SVP_PUMP && g.s.pl[SA_L_BASS].depth == 50, "FX: PUMP, BASS at 50 %% (%d)",
          g.s.pl[SA_L_BASS].depth);
    sv_knob(&ui, SV_K3, 1);
    sv_knob(&ui, SV_K2, 6);
    CHECK(g.s.pl[SA_L_BASS].src == SA_PS_BEAT && g.s.pl[SA_L_BASS].rel == 80, "SOURCE BEAT, RELEASE 80");
    bars(1);
    shot("07c_bass_pump");
    {
        int mb = 0, mc = 0;
        g.s.pl[SA_L_CHORD].on = 0;
        for (int k = 0; k < 120; ++k) {
            run_ms(16);
            if (g.duck[SA_L_BASS] > mb) mb = g.duck[SA_L_BASS];
            if (g.duck[SA_L_CHORD] > mc) mc = g.duck[SA_L_CHORD];
        }
        CHECK(mb > 5 && mc == 0, "BASS pumps on the beat, CHORD with its pump off stays up (%d %d)", mb, mc);
        g.s.pl[SA_L_CHORD].on = 1;
    }
    press(SV_B_FX);
    CHECK(sv_page_now(&ui) == SVP_GATE, "FX: GATE");
    press(SV_B_FX);
    CHECK(sv_page_now(&ui) == SVP_MOD, "FX: MOD");
    sv_knob(&ui, SV_K1, 1);
    CHECK(g.ed[SA_L_BASS].mod[0].type != SA_M_OFF && sa_pending(&g) == 1, "MOD 1: a modifier, waiting");
    shot("08_bass_mod");
    press(SV_B_SEQ);
    bars(1);
    press(SV_B_FX);
    CHECK(ui.fx == 0, "FX past the last page: home");
    press(SV_B_FX);
    sv_knob(&ui, SV_K_PRESETS, -1);
    CHECK(ui.view == SV_V_CHORD && ui.fx == 0, "another layer: its home page, not the page FX was on");
    /* CHORD's ARP, off at first */
    fx_to(SVP_ARP);
    CHECK(sv_page_now(&ui) == SVP_ARP && g.ed[SA_L_CHORD].amode == SA_AR_OFF, "CHORD: ARP, OFF");
    sv_knob(&ui, SV_K1, 1);
    shot("10_chord_arp");
    all_palettes("chord_arp");
    press(SV_B_SEQ);
    bars(1);
    CHECK(g.s.code[SA_L_CHORD].amode == SA_AR_UP, "ARP UP dropped");
    fx_to(SVP_GATE);
    press(SV_B_REC);
    sv_key(&ui, SV_KEY_LO + 2, true);
    sv_key(&ui, SV_KEY_LO + 2, false);
    CHECK(g.ed_gate[SA_L_CHORD].own, "REC on GATE: a key, a gate step");
    shot("11_chord_gate_rec");
    press(SV_B_SEQ);
    press(SV_B_FX);
    CHECK(sv_page_now(&ui) == SVP_MOD && ui.rec, "REC stays while FX turns");
    sv_knob(&ui, SV_K_PRESETS, -1);
    CHECK(!ui.rec, "another layer: REC off");
    /* LEAD: OFF; REC writes its steps from the keys */
    press(SV_B_REC);
    CHECK(ui.rec && sv_steps_on(&ui), "LEAD, REC: STEPS");
    sv_key(&ui, 69, true);
    sv_key(&ui, 69, false);
    sv_key(&ui, 72, true);
    sv_key(&ui, 72, false);
    CHECK(g.ed[SA_L_LEAD].own && g.ed[SA_L_LEAD].sn[0] != SA_NO_NOTE && g.ed[SA_L_LEAD].sn[1] != SA_NO_NOTE &&
          ui.scur == 2, "two notes on two steps");
    shot("12_lead_steps");
    press(SV_B_SEQ);
    bars(2);
    CHECK(part_on[SA_L_LEAD] > 0, "LEAD plays its steps (%d)", part_on[SA_L_LEAD]);
    press(SV_B_REC);
    /* DRUMS: seven rows; KNOB 1 the row, KNOB 2 its INSTR (at once), the keys its steps under REC */
    sv_knob(&ui, SV_K_PRESETS, 4);
    CHECK(ui.view == SV_V_DRUMS && ui.layer == SA_L_DRUM0 + SA_DR_KICK, "DRUMS: the kick row");
    CHECK(sv_play_keys(&ui) == 1u << (60 - SV_KEY_LO), "DRUMS: the kick's own note (C, folded up) lit (%x)",
          sv_play_keys(&ui));
    {
        int before = part_on[SA_PART_DRUMS];
        sv_key(&ui, SV_KEY_LO + 7, true);
        CHECK(part_on[SA_PART_DRUMS] == before + 1 && !strcmp(ui.msg, "KEYS: DRUMS, KICK"), "any key plays the drum picked (%s)",
              ui.msg);
        sv_key(&ui, SV_KEY_LO + 7, false);
    }
    shot("13_drums");
    all_palettes("drums");
    sv_knob(&ui, SV_K1, 3);
    CHECK(ui.layer == SA_L_DRUM0 + SA_DR_HATS && g.s.code[ui.layer].sound == SA_DV_HAT, "DRUM: the HATS row, a HAT");
    sv_knob(&ui, SV_K2, 1);
    CHECK(g.s.code[ui.layer].sound == SA_DV_OPEN && g.ed[ui.layer].sound == SA_DV_OPEN && !sa_pending(&g),
          "INSTR: the open hat, at once");
    sv_knob(&ui, SV_K2, 5);
    CHECK(g.s.code[ui.layer].sound == SA_DV_SHAKER, "INSTR stops at the row's last (%d)", g.s.code[ui.layer].sound);
    sv_knob(&ui, SV_K2, -5);
    sv_knob(&ui, SV_K4, 10);
    CHECK(g.s.live[ui.layer].env == 70, "ACCENT");
    press(SV_B_REC);
    uint32_t fl, dl;
    CHECK(sv_key_leds(&ui, &fl, &dl) && dl == 0xFFFFu, "REC: the step keys lit");
    sv_key(&ui, SV_KEY_LO + 5, true);
    sv_key(&ui, SV_KEY_LO + 5, false);
    CHECK(g.ed[ui.layer].own, "a drum step");
    shot("14_drums_rec");
    press(SV_B_SEQ);
    press(SV_B_REC);
    /* a row OFF: ALGORITHM gives it a pattern */
    sv_knob(&ui, SV_K1, 1);
    CHECK(ui.layer == SA_L_DRUM0 + SA_DR_TOMS && g.s.code[ui.layer].pat == SA_PAT_OFF, "TOMS: OFF");
    sv_knob(&ui, SV_K2, 2);
    sv_knob(&ui, SV_K_ALGO, 11);
    CHECK(g.ed[ui.layer].pat != SA_PAT_OFF && g.s.code[ui.layer].sound == SA_DV_CONGA, "a conga, a pattern waiting");
    shot("15_drums_on");
    press(SV_B_SEQ);
    bars(1);
    fx_to(SVP_MOD);
    sv_knob(&ui, SV_K4, -10);
    CHECK(g.s.live[ui.layer].cut == 50, "CHANCE: on MOD, beside the modifiers (%d)", g.s.live[ui.layer].cut);
    shot("15b_drums_mod");
    sv_knob(&ui, SV_K4, 10);
    fx_to(SVP_KIT);
    sv_knob(&ui, SV_K1, 3);
    CHECK(g.s.kit == SA_KIT_HARD, "KIT +3: HARD (%d)", g.s.kit);
    for (int k = 0; k < SA_KITS; ++k) {            /* every kit's name on its card */
        char nm[32];
        g.s.kit = (uint8_t)k;
        snprintf(nm, sizeof nm, "kit_%s", sa_kit_name(k));
        lint_where = nm;
        sv_draw_all(&ui);
    }
    CHECK(SA_KITS == 20, "twenty kits (%d)", SA_KITS);
    g.s.kit = SA_KIT_HARD;
    shot("16_drums_kit");
    /* KEY on SCL, and back */
    press(SV_B_SCL);
    CHECK(ui.page == SV_PG_KEY, "SCL: KEY");
    sv_knob(&ui, SV_K1, 2);
    shot("17_key");
    press(SV_B_SEQ);
    bars(1);
    press(SV_B_SCL);
    CHECK(ui.page == SV_PG_LAYER && ui.view == SV_V_DRUMS, "SCL again: back to DRUMS");
    /* HOME: every layer */
    press(SV_B_HOME);
    bars(1);
    shot("18_home_full");
    all_palettes("home_full");
    /* EDIT: a layer's pattern editor; KNOB 1 the step, REC: a key writes there */
    to_view(SV_V_BASS);
    press(SV_B_EDIT);
    CHECK(ui.page == SV_PG_EDIT && sv_page_button(&ui) == SV_B_EDIT, "EDIT: BASS's editor");
    {
        uint32_t seen = 0;
        for (int k = 0; k < 40; ++k) { run_ms(16); seen |= sv_play_keys(&ui); }
        CHECK(seen != 0, "EDIT: the keys light as BASS plays");
    }
    sv_knob(&ui, SV_K1, 5);
    press(SV_B_REC);
    sv_key(&ui, 60, true);
    sv_key(&ui, 60, false);
    CHECK(ui.rec && g.ed[SA_L_BASS].own && g.ed[SA_L_BASS].sn[5] != SA_NO_NOTE && ui.scur == 6, "EDIT, REC: a note on step 6");
    sv_knob(&ui, SV_K2, 2);
    CHECK(!strcmp(sv_fill_name(&ui, ui.fill), "ARP UP") && g.ed[SA_L_BASS].sn[0] != SA_NO_NOTE &&
          g.ed[SA_L_BASS].sn[2] > g.ed[SA_L_BASS].sn[0], "FILL: ARP UP, a line up the scale");
    shot("18f_edit_fill");
    shot("18d_edit_bass");
    press(SV_B_SEQ);
    press(SV_B_EDIT);
    CHECK(ui.page == SV_PG_LAYER && !ui.rec, "EDIT again: back, REC off");
    to_view(SV_V_DRUMS);
    press(SV_B_EDIT);
    sv_knob(&ui, SV_K1, 2);
    sv_knob(&ui, SV_K3, 1);
    CHECK((sv_ed_bits(&ui, ui.layer) >> 2) & 1u, "EDIT, a drum: KNOB 3 a hit on step 3");
    sv_knob(&ui, SV_K2, 1);
    CHECK((sv_ed_bits(&ui, ui.layer) & 0xF000u) == 0xF000u && !strcmp(sv_fill_name(&ui, ui.fill), "ROLL"),
          "FILL: ROLL, the last beat in sixteenths (%04x)", sv_ed_bits(&ui, ui.layer));
    sv_knob(&ui, SV_K2, -1);
    CHECK(ui.fill == 0 && !memcmp(&g.ed[ui.layer], &ui.fill_orig, sizeof ui.fill_orig), "FILL back to CURR: as it was");
    sv_knob(&ui, SV_K3, 1);
    press(SV_B_REC);
    {
        uint32_t fl, dl;
        CHECK(sv_key_leds(&ui, &fl, &dl) && dl == 0xFFFFu, "EDIT, REC, a drum: the 16 step keys lit");
    }
    sv_key(&ui, SV_KEY_LO + 2, true);
    sv_key(&ui, SV_KEY_LO + 2, false);
    CHECK(((sv_ed_bits(&ui, ui.layer) >> 2) & 1u) == 0, "EDIT, REC: the third key, step 3 off");
    shot("18e_edit_drum");
    press(SV_B_SEQ);
    press(SV_B_EDIT);
    press(SV_B_HOME);
    bars(1);
    /* the MIXER on GLO: the four volumes, FX: their mutes */
    press(SV_B_GLO);
    CHECK(ui.page == SV_PG_MIXER && sv_page_button(&ui) == SV_B_GLO, "GLO: MIXER");
    sv_knob(&ui, SV_K4, -10);
    CHECK(g.s.dvol == 60, "MIXER: DRUMS volume (%d)", g.s.dvol);
    bars(1);
    shot("18b_mixer");
    all_palettes("mixer");
    press(SV_B_FX);
    sv_knob(&ui, SV_K2, 1);
    CHECK(ui.mixmute && g.s.live[SA_L_CHORD].mute, "MIXER, FX: CHORD muted");
    shot("18c_mixer_mute");
    sv_knob(&ui, SV_K2, -1);
    sv_knob(&ui, SV_K4, -1);
    press(SV_B_GLO);
    CHECK(ui.page == SV_PG_MAIN, "GLO again: back HOME");
    g.s.dvol = 80;
    /* SETS */
    ui.sets[0] = (sv_set_info){1, 126, 9, SA_SC_MINOR, SA_KIT_TECH, 3, {0, 1, (uint8_t)(SAS_NSOUNDS - 1)}, 0};
    ui.sets[1] = (sv_set_info){0, 0, 0, 0, 0, 0, {0, 0, 0}, 1};
    ui.sets[SV_LAST] = (sv_set_info){1, 128, 5, SA_SC_MINOR, SA_KIT_HOUSE, 2, {2, 4, 0}, 0};
    press(SV_B_SAVE);
    CHECK(ui.page == SV_PG_SETS && sv_page_button(&ui) == SV_B_SAVE, "SAVE: SETS");
    shot("19_sets");
    all_palettes("sets");
    sv_knob(&ui, SV_K1, 4);
    press(SV_B_REC);
    CHECK(ui.req_act == -1 && ui.msg_ms > 0, "LOAD an empty set: nothing asked");
    sv_knob(&ui, SV_K_PRESETS, -4);
    press(SV_B_REC);
    CHECK(ui.req_act == SV_SA_LOAD && ui.req_slot == 0, "REC: LOAD set 1 asked");
    ui.req_act = -1;
    sv_knob(&ui, SV_K1, 9);
    sv_knob(&ui, SV_K2, 1);
    press(SV_B_REC);
    CHECK(ui.req_act == -1, "STORE into LAST: refused");
    press(SV_B_SAVE);
    CHECK(ui.page == SV_PG_MAIN, "SAVE again: back HOME");
    /* MENU */
    sv_button(&ui, SV_B_HOME, true);
    run_ms(700);
    sv_button(&ui, SV_B_HOME, false);
    CHECK(ui.page == SV_PG_MENU, "HOME held: MENU");
    shot("20_menu");
    press(SV_B_HOME);
    press(SV_B_PLAY);
    CHECK(!g.playing, "PLAY stops");
    CHECK(g.nheld == 0, "nothing left sounding");
    CHECK(lint_findings == 0, "the layout lint: %d findings", lint_findings);
    printf("sa_ui: %d checks, %d failed, %d frames, %d notes on (parts %d %d %d, drums %d)\n", checks, fails,
           frames_written, notes_on, part_on[0], part_on[1], part_on[2], part_on[3]);
    return fails ? 1 : 0;
}
