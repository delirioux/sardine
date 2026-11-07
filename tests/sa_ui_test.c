/* SPDX-License-Identifier: GPL-3.0-only */
/* SARDINE's screen and controls on the host: firmware/src/sa_ui.c drawn by Felucca's gfx.c into a 240 x 240 frame,
 * the layers running (their notes counted, no sound), the controls driven as a player would, every page in every
 * palette written out, and the layout lint (from Tuna's and Flyfish's UI tests): every text inside its canvas, not
 * cut, not ellipsised unless it is a name, no two texts of one canvas overlapping.
 *   tests/sa_ui_test.sh [OUTDIR]     (exit 0: all passed; PNGs and a GIF of MAIN under OUTDIR) */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>
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
          "the first groove: drums, chord, bass; LEAD not in yet (%d %d %d %d)", part_on[0], part_on[1], part_on[2], part_on[3]);
    shot("02_home");
    bars(7);
    CHECK(part_on[SA_L_LEAD] > 0, "the LEAD in after a pattern (%d notes by bar 9)", part_on[SA_L_LEAD]);
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
    /* HOME, FX: RESO and WIDTH of the whole mix */
    press(SV_B_FX);
    sv_knob(&ui, SV_K1, 20);
    CHECK(ui.home2 && g.s.mreso == 40, "HOME, FX: RESO (%d)", g.s.mreso);
    shot("02c_home_reso");
    press(SV_B_FX);
    CHECK(!ui.home2, "FX again: FILTER FEEL PUMP SPACE");
    sv_knob(&ui, SV_K1, 0);
    g.s.mreso = 0;
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
    for (int i = 0; i < 96; ++i)                   /* (the glue's wave: here a sine and its octave) */
        ui.wave[i] = (int8_t)(80.0 * sin(i * 6.2832 / 32.0) + 30.0 * sin(i * 6.2832 / 16.0));
    ui.wave_n = 96;
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
    CHECK(g.s.live[SA_L_BASS].level == 50, "VOLUME (70 less 20) (%d)", g.s.live[SA_L_BASS].level);
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
    {
        const sa_code *c = &g.s.code[SA_L_CHORD];
        CHECK(c->amode == SA_AR_PINKY && c->arate == SA_RATE_8 && c->arange == 1 && c->agate == 70,
              "ARP on: PINKY, 1/8, one octave, gate 70, dropped");
    }
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
    CHECK(g.s.code[ui.layer].sound == SA_DV_HAT && g.s.code[ui.layer].amode == 1 && !strcmp(ui.msg, "HAT TIGHT"),
          "INSTR: HAT TIGHT, a variation (%s)", ui.msg);
    shot("13b_drums_variation");
    sv_knob(&ui, SV_K2, SA_DVARS - 1);
    CHECK(g.s.code[ui.layer].sound == SA_DV_OPEN && g.s.code[ui.layer].amode == 0 && !sa_pending(&g),
          "INSTR: the open hat, at once");
    sv_knob(&ui, SV_K2, 40);
    CHECK(g.s.code[ui.layer].sound == SA_DV_SHAKER && g.s.code[ui.layer].amode == SA_DVARS - 1,
          "INSTR stops at the row's last: SHAKER DEEP (%d %d)", g.s.code[ui.layer].sound, g.s.code[ui.layer].amode);
    sv_knob(&ui, SV_K2, -40);
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
    sv_knob(&ui, SV_K2, 2 * SA_DVARS);
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
    CHECK(SA_KITS == 24 && !strcmp(sa_kit_name(SA_KIT_RUMBLE), "RUMBLE") && SA_KIT_INKEY(SA_KIT_HYPNO) && !SA_KIT_INKEY(SA_KIT_ACID), "24 kits, the last four with the kick in key (%d)", SA_KITS);
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
    /* ENV held: a drum fill from the next beat, the drums' own notes out; let go: back */
    {
        int before = part_on[SA_PART_DRUMS];
        sv_button(&ui, SV_B_ENV, true);
        bars(1);
        CHECK(g.fill_on && part_on[SA_PART_DRUMS] - before > 16, "ENV held: a fill plays (%d drum hits in a bar)",
              part_on[SA_PART_DRUMS] - before);
        float sw = 0.0f;
        for (int i = 0; i < 40; ++i) { run_ms(10); if (g.sweep > sw) sw = g.sweep; }
        CHECK(sw > 0.1f, "a fill dips the filter towards the 1 (%.2f)", sw);
        int k0 = g.fill_kind;
        sv_button(&ui, SV_B_ENV, false);
        CHECK(!g.fill_on && g.fill_kind == (k0 + 1) % 8, "let go: the groove, the next fill next time");
    }
    /* DRUMS: a row with no pattern, ALGORITHM: the pattern waits, its name in the header; PART B: its B row */
    {
        press(SV_B_HOME);
        while (ui.view != SV_V_DRUMS) sv_knob(&ui, SV_K_PRESETS, 1);
        int r = -1;
        for (int k = 0; k < SA_DROWS && r < 0; ++k)
            if (g.ed[SA_L_DRUM0 + k].pat == SA_PAT_OFF) r = k;
        CHECK(r >= 0, "a drum row without a pattern (%d)", r);
        if (r >= 0) {
            int l = SA_L_DRUM0 + r;
            sv_knob(&ui, SV_K1, r - ui.drow);
            sv_knob(&ui, SV_K_ALGO, 1);
            CHECK(ui.layer == l && g.ed[l].pat != SA_PAT_OFF && sa_layer_pending(&g, l) && ui.msg[0] && strcmp(ui.msg, "OFF"),
                  "its pattern waits, named in the header (%s)", ui.msg);
            shot("13c_drum_new");
            sa_part(&g, true);
            sv_knob(&ui, SV_K_ALGO, 1);
            CHECK(g.ed_patb[l] != 0 && !strncmp(ui.msg, "B: ", 3), "PART B: the row's B pattern (%s)", ui.msg);
            sa_part(&g, false);
            sa_edit_reset(&g);
            g.s.live[l].mute = 1;                   /* muted: a new pattern waits lit, SEQ brings it in with it */
            sv_knob(&ui, SV_K_ALGO, 1);
            press(SV_B_SEQ);
            bars(2);
            CHECK(!g.s.live[l].mute && g.s.code[l].pat != SA_PAT_OFF, "a muted row's new pattern: in with it");
        }
        press(SV_B_HOME);
    }
    /* MOVES: a tap is the button's job; held past 300 ms a move, the button's job not done */
    {
        int o0 = ui.oct;
        press(SV_B_OCTUP);
        CHECK(ui.oct == o0 + 1 && !ui.moves, "OCT+ tapped: the octave");
        sv_button(&ui, SV_B_OCTUP, true);
        run_ms(400);
        CHECK(ui.moves == 1u << SV_MV_THROW && ui.oct == o0 + 1, "OCT+ held: THROW (moves %u)", ui.moves);
        sv_button(&ui, SV_B_OCTUP, false);
        CHECK(!ui.moves && ui.oct == o0 + 1, "let go: THROW over, the octave as it was");
        press(SV_B_OCTDN);
        sv_button(&ui, SV_B_REC, true);
        sv_button(&ui, SV_B_EDIT, true);
        sv_button(&ui, SV_B_OCTDN, true);
        run_ms(400);
        CHECK(ui.moves == ((1u << SV_MV_WOBBLE) | (1u << SV_MV_STOP) | (1u << SV_MV_WASH)) && !ui.rec,
              "REC EDIT OCT- held together: WOBBLE TAPE STOP WASH (moves %u), REC not toggled", ui.moves);
        g.wobble = true;
        float sw = 0.0f;
        for (int i = 0; i < 30; ++i) { run_ms(10); if (g.sweep > sw) sw = g.sweep; }
        g.wobble = false;
        CHECK(sw > 0.4f, "WOBBLE: the filter moves (%.2f)", sw);
        sv_button(&ui, SV_B_REC, false);
        sv_button(&ui, SV_B_EDIT, false);
        sv_button(&ui, SV_B_OCTDN, false);
        CHECK(!ui.moves && ui.page != SV_PG_EDIT, "let go: the moves over, no EDIT opened");
    }
    /* ARP held: BREAK, the drums and the bass out at once; let go: in on the next 1 */
    {
        int d0 = part_on[SA_PART_DRUMS], b0 = part_on[SA_L_BASS], c0 = part_on[SA_L_CHORD];
        sv_button(&ui, SV_B_ARP, true);
        bars(2);                                    /* (two: a slow chord pattern plays at least once) */
        CHECK(g.brk_on && part_on[SA_PART_DRUMS] == d0 && part_on[SA_L_BASS] == b0 && part_on[SA_L_CHORD] > c0,
              "BREAK: no drums, no bass, the chord on");
        sv_button(&ui, SV_B_ARP, false);
        CHECK(g.brk_on, "let go: still out until the 1");
        bars(1);
        CHECK(!g.brk_on && part_on[SA_PART_DRUMS] > d0, "on the 1: the drop");
        CHECK(!g.back_on, "a short BREAK: everything back at once");
    }
    /* BREAK held over 4 bars: let go, the drums back one by one over two bars (the kick first) */
    {
        bars(1);
        int n0 = part_on[SA_PART_DRUMS];
        bars(1);
        int full = part_on[SA_PART_DRUMS] - n0;
        sv_button(&ui, SV_B_ARP, true);
        bars(5);
        sv_button(&ui, SV_B_ARP, false);
        int a = part_on[SA_PART_DRUMS];
        bars(1);
        int b1 = part_on[SA_PART_DRUMS] - a;
        CHECK(!g.brk_on && g.back_on, "a long BREAK let go: back in (%d)", g.brk_bars);
        a = part_on[SA_PART_DRUMS];
        bars(1);
        int b2 = part_on[SA_PART_DRUMS] - a;
        a = part_on[SA_PART_DRUMS];
        bars(1);
        int b3 = part_on[SA_PART_DRUMS] - a;
        CHECK(b1 > 0 && b1 < b2 && b2 < full && b3 > b2 && !g.back_on,
              "one by one: %d then %d drum hits, then the whole beat (%d of %d)", b1, b2, b3, full);
    }
    /* LFO held: SWEEP, the filter closing; let go: open */
    sv_button(&ui, SV_B_LFO, true);
    bars(1);
    CHECK(g.sweep > 0.3f && g.sweep < 0.8f, "SWEEP: half closed after a bar (%d %%)", (int)(g.sweep * 100.0f));
    sv_button(&ui, SV_B_LFO, false);
    CHECK(g.sweep_man == 0.0f, "let go: open");
    /* the STYLEs: every name they use there */
    for (int k = 0; k < SAS_NSTYLES; ++k) {
        const sas_style *y = &SAS_STYLE[k];
        const char *snd[6] = {y->bass[0], y->bass[1], y->chord[0], y->chord[1], y->lead[0], y->lead[1]};
        const char *pat[6] = {y->bpat[0], y->bpat[1], y->cpat[0], y->cpat[1], y->lpat[0], y->lpat[1]};
        for (int i = 0; i < 6; ++i) {
            CHECK(!snd[i] || sas_find(snd[i]) >= 0, "style %s: the sound %s", y->name, snd[i] ? snd[i] : "");
            CHECK(!pat[i] || sas_pat(SA_T_SYNTH, pat[i]) >= 0, "style %s: the pattern %s", y->name, pat[i] ? pat[i] : "");
        }
        CHECK(sas_pat(SA_T_DRUM, y->kick) >= 0 && sas_pat(SA_T_DRUM, y->hats) >= 0 && (!y->xpat || sas_pat(SA_T_DRUM, y->xpat) >= 0),
              "style %s: its drum patterns", y->name);
        CHECK(y->bpm_lo <= y->bpm_hi && y->bpm_hi <= 200, "style %s: its tempo", y->name);
    }
    CHECK(SAS_NSTYLES == 20, "twenty styles (%d)", SAS_NSTYLES);
    {
        sa_pool pool;
        sa_settings t;
        sas_pool_make(&pool);
        sas_style_apply(&t, 8, 3, &pool);          /* ACID */
        CHECK(!strcmp(sas_of(t.code[SA_L_BASS].sound)->name, "ACID") && t.bpm >= 128 && t.bpm <= 134 && t.style == 8,
              "ACID: the ACID bass, its tempo (%s %d)", sas_of(t.code[SA_L_BASS].sound)->name, t.bpm);
    }
    /* the MIXER on GLO: the four volumes, FX: their mutes */
    press(SV_B_GLO);
    CHECK(ui.page == SV_PG_MIXER && sv_page_button(&ui) == SV_B_GLO, "GLO: MIXER");
    sv_knob(&ui, SV_K4, -10);
    CHECK(g.s.dvol == 50, "MIXER: DRUMS volume (70 less 20) (%d)", g.s.dvol);
    bars(1);
    shot("18b_mixer");
    all_palettes("mixer");
    press(SV_B_GLO);
    sv_knob(&ui, SV_K2, 1);
    CHECK(ui.mixmute && g.s.live[SA_L_CHORD].mute, "GLO again: MUTE, CHORD muted");
    shot("18c_mixer_mute");
    sv_knob(&ui, SV_K2, -1);
    sv_knob(&ui, SV_K4, -1);
    press(SV_B_GLO);
    sv_knob(&ui, SV_K3, 1);
    {
        int b0 = part_on[SA_L_BASS], c0 = part_on[SA_L_CHORD], d0 = part_on[SA_PART_DRUMS];
        bars(1);
        CHECK(ui.mixmute == 2 && g.solo == 4 && part_on[SA_L_BASS] > b0 && part_on[SA_L_CHORD] == c0 &&
              part_on[SA_PART_DRUMS] == d0, "GLO a third time: SOLO, BASS alone");
    }
    shot("18g_mixer_solo");
    sv_knob(&ui, SV_K3, -1);
    CHECK(g.solo == 0, "SOLO off: all");
    press(SV_B_GLO);
    CHECK(ui.mixmute == 3, "GLO a fourth time: STYLE");
    sv_knob(&ui, SV_K1, 5);
    sv_knob(&ui, SV_K2, 2);
    sv_knob(&ui, SV_K3, 1);
    CHECK(g.s.intro == 2 && g.s.arc == 1, "INTRO 8 BARS, ARC 16 BARS");
    shot("18h_style");
    all_palettes("style");
    press(SV_B_SEQ);
    CHECK(ui.req_act == SV_SA_STYLE && ui.req_slot == ui.style_pick, "SEQ: a groove in the style asked");
    ui.req_act = -1;
    g.s.intro = g.s.arc = 0;
    press(SV_B_GLO);
    CHECK(ui.page == SV_PG_MIXER && ui.mixmute == 4 && ui.live_start == SA_START_KICK, "GLO: LIVE, START KICK");
    shot("18h2_live_setup");
    press(SV_B_SEQ);
    CHECK(ui.req_act == SV_SA_LIVE, "LIVE, SEQ: the mix set up, asked");
    ui.req_act = -1;
    {   /* (the glue: the groove in, its START cued) */
        static sa_settings ls;
        ls = g.s;
        g.build_cue = SA_START_KICK;
        sa_set_arm(&g, &ls);
        bars(1);
        int s0 = part_on[0] + part_on[1] + part_on[2], d0 = part_on[SA_PART_DRUMS];
        bars(1);
        int syn = part_on[0] + part_on[1] + part_on[2] - s0, dr = part_on[SA_PART_DRUMS] - d0;
        CHECK(g.build_n > 3 && sa_build_peek(&g) == SA_L_DRUM0 + SA_DR_HATS && syn == 0 && dr > 0 && dr <= 8,
              "LIVE KICK: the kick alone (%d drum hits, %d synth notes), the hats next", dr, syn);
        int dup = 0;
        for (int i = 0; i < g.build_n; ++i)
            for (int j = 0; j < i; ++j) dup += g.build_order[i] == g.build_order[j];
        CHECK(dup == 0 && g.build_n <= 10, "the build: each layer once (%d)", g.build_n);
        press(SV_B_SEQ);
        CHECK(g.build_req && ui.req_act < 0 && sa_build_peek(&g) == SA_L_DRUM0 + SA_DR_HATS,
              "SEQ (on LIVE too, playing): the hats in on the 1");
        bars(1);
        CHECK(!g.build_req && sa_build_peek(&g) == SA_L_BASS, "the hats in, the bass next");
        shot("18h3_live");
        ui.live_build = 0;
        sv_knob(&ui, SV_K3, 1);
        CHECK(g.build_auto == 8, "BUILD 8 BARS");
        for (int k = 0; k < 4; ++k) bars(25);
        CHECK(sa_build_peek(&g) < 0 && g.build_out == 0, "by itself, every 8 bars: all in");
        sv_knob(&ui, SV_K3, -1);
    }
    press(SV_B_GLO);
    CHECK(ui.page == SV_PG_MIXER && ui.mixmute == 5, "GLO: SONG");
    CHECK(ui.song.set[0] == 1 && ui.song.set[3] == 4 && ui.song.set[4] == 0 && sv_song_bars(ui.song.bars[0]) == 16,
          "SONG: sets 1..4, 16 bars each");
    sv_knob(&ui, SV_K1, 4);
    sv_knob(&ui, SV_K2, 2);
    sv_knob(&ui, SV_K3, 1);
    CHECK(ui.song_pick == 4 && ui.song.set[4] == 2 && sv_song_bars(ui.song.bars[4]) == 8, "SONG: step 5, set 2, 8 bars");
    sv_knob(&ui, SV_K4, 1);
    CHECK(ui.song.on, "SONG ON");
    shot("18i_song");
    sv_knob(&ui, SV_K4, -1);
    press(SV_B_GLO);
    CHECK(ui.page == SV_PG_MAIN, "GLO past SONG: back HOME");
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
    ui.cpu = 42;
    ui.cpu_peak = 67;
    shot("20_menu");
    press(SV_B_HOME);
    CHECK(ui.page == SV_PG_MENU && ui.menu2 && ui.midi_ch[0] == 1 && ui.midi_ch[3] == 10, "MENU, HOME: MIDI OUT, LEAD on 1, DRUMS on 10");
    sv_knob(&ui, SV_K2, -2);
    sv_knob(&ui, SV_K3, -3);
    CHECK(ui.midi_ch[1] == 0 && ui.midi_ch[2] == 0, "CHORD and BASS OFF");
    sv_knob(&ui, SV_K2, 5);
    CHECK(ui.midi_ch[1] == 5, "CHORD on 5");
    {   /* (the glue's log, as it writes it) */
        static const uint8_t L[6] = {3, 0, 3, 1, 2, 3}, C[6] = {10, 1, 10, 5, 3, 10}, N[6] = {36, 72, 42, 64, 36, 54};
        for (int k = 0; k < 6; ++k) {
            ui.ml_layer[k] = L[k], ui.ml_ch[k] = C[k], ui.ml_note[k] = N[k], ui.ml_vel[k] = (uint8_t)(90 + k * 5);
        }
        ui.ml_n = 6;
    }
    shot("20b_midi");
    press(SV_B_HOME);
    CHECK(ui.page == SV_PG_MAIN, "HOME again: back HOME");
    press(SV_B_PLAY);
    CHECK(!g.playing, "PLAY stops");
    CHECK(g.nheld == 0, "nothing left sounding");
    CHECK(lint_findings == 0, "the layout lint: %d findings", lint_findings);
    printf("sa_ui: %d checks, %d failed, %d frames, %d notes on (parts %d %d %d, drums %d)\n", checks, fails,
           frames_written, notes_on, part_on[0], part_on[1], part_on[2], part_on[3]);
    return fails ? 1 : 0;
}
