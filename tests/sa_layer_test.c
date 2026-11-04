/* SPDX-License-Identifier: GPL-3.0-only */
/* Sardine's layer engine on the host: the patterns, the modifiers, the drop on the bar line, the mutes, the
 * arpeggiator, the chords, the limits.   cc ... tests/sa_layer_test.c firmware/src/sa_layer.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sa_layer.h"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef struct { int on[4][128]; int total_on, total_off; int per_part[4]; } log_t;
static log_t LG;
static void snd(void *ctx, const sa_event *e)
{
    (void)ctx;
    if (e->on) { LG.on[e->part & 3][e->note & 127]++; LG.total_on++; LG.per_part[e->part & 3]++; }
    else LG.total_off++;
}

#define SR 44100.0f
static int bar_frames(const sa_groove *g) { return (int)(SR * 60.0f / (float)g->s.bpm * 4.0f); }
static void run_bars(sa_groove *g, int bars)
{
    int n = bar_frames(g) * bars;
    while (n > 0) { int k = n > 128 ? 128 : n; sa_process(g, k); n -= k; }
}
static void clear_log(void) { memset(&LG, 0, sizeof LG); }

static void only(sa_groove *g, int keep)
{
    for (int l = 0; l < SA_LAYERS; ++l)
        if (l != keep) { g->ed[l].type = SA_T_NONE; }
    sa_drop(g);
}

int main(void)
{
    static sa_groove g;
    sa_init(&g, SR);
    sa_set_sound(&g, snd, 0);
    CHECK(sa_pending(&g) == 0, "fresh: nothing pending");

    /* the first groove plays: four kicks a bar */
    only(&g, 0);                                    /* stopped: drops at once */
    CHECK(sa_pending(&g) == 0, "dropped while stopped");
    sa_play(&g, true);
    clear_log();
    run_bars(&g, 4);
    CHECK(LG.on[3][36] == 16, "4 FLOOR: 16 kicks in 4 bars, got %d", LG.on[3][36]);

    /* FAST 2: twice as many */
    g.ed[0].mod[0].type = SA_M_FAST;
    g.ed[0].mod[0].val = 2;
    CHECK(sa_pending(&g) == 1, "one change pending");
    clear_log();
    run_bars(&g, 1);
    CHECK(LG.on[3][36] == 4, "not dropped yet: still 4 a bar, got %d", LG.on[3][36]);
    /* REC in the middle of a bar: that bar finishes with the old code, the next one has the new */
    clear_log();
    sa_process(&g, bar_frames(&g) / 2);
    sa_drop(&g);
    CHECK(g.drop_armed, "armed while playing");
    sa_process(&g, bar_frames(&g) - bar_frames(&g) / 2);
    CHECK(LG.on[3][36] == 4, "the bar REC was pressed in: the old code, 4, got %d", LG.on[3][36]);
    clear_log();
    run_bars(&g, 1);
    CHECK(LG.on[3][36] == 8, "FAST 2 from the next bar line: 8, got %d", LG.on[3][36]);
    CHECK(!g.drop_armed && sa_pending(&g) == 0, "dropped on the bar line");

    /* REV of 4 FLOOR: still 4 a bar (symmetric), LEN 3: polymeter, 6 sixteenths in 16? (0,3 of each 3-step cycle) */
    memset(g.ed[0].mod, 0, sizeof g.ed[0].mod);
    g.ed[0].mod[0].type = SA_M_LEN;
    g.ed[0].mod[0].val = 3;                         /* the first 3 sixteenths (a kick on 0) looped: a kick every 3 */
    sa_drop(&g);
    run_bars(&g, 1);                                /* (the drop's bar) */
    clear_log();
    run_bars(&g, 3);                                /* 48 sixteenths: a kick every 3 -> 16 */
    CHECK(LG.on[3][36] == 16, "LEN 3: a kick every 3 sixteenths, 16 in 3 bars, got %d", LG.on[3][36]);

    /* EUCLID 5/16 */
    g.ed[0].mod[0].type = SA_M_EUCLID;
    g.ed[0].mod[0].val = 5;
    sa_drop(&g);
    run_bars(&g, 1);
    clear_log();
    run_bars(&g, 2);
    CHECK(LG.on[3][36] == 10, "EUCLID 5: 5 a bar, got %d", LG.on[3][36]);

    /* THIN 50 % leaves some, not all */
    g.ed[0].mod[0].type = SA_M_FAST;
    g.ed[0].mod[0].val = 4;
    g.ed[0].mod[1].type = SA_M_THIN;
    g.ed[0].mod[1].val = 5;
    sa_drop(&g);
    run_bars(&g, 1);
    clear_log();
    run_bars(&g, 4);
    CHECK(LG.on[3][36] > 10 && LG.on[3][36] < 54, "FAST 4 THIN 50: between, got %d", LG.on[3][36]);

    /* the mute: at once */
    sa_mute(&g, 0, true);
    clear_log();
    run_bars(&g, 1);
    CHECK(LG.on[3][36] == 0, "muted: silent, got %d", LG.on[3][36]);
    sa_mute(&g, 0, false);

    /* a synth layer follows the chord; the arpeggiator */
    sa_play(&g, false);
    sa_init(&g, SR);
    sa_set_sound(&g, snd, 0);
    only(&g, 3);                                    /* the ROLLER bass alone */
    sa_play(&g, true);
    clear_log();
    run_bars(&g, 2);
    CHECK(LG.per_part[0] == 16, "ROLLER: 8 notes a bar on part 1, got %d", LG.per_part[0]);
    /* A minor: the bass notes are A (57 - 12 = 45 at octave 2) or its octave/fifth */
    CHECK(LG.on[0][45] > 0, "the bass plays A2 in A minor");
    g.ed[3].amode = SA_AR_UP;
    g.ed[3].arate = SA_RATE_32;
    g.ed[3].pat = 6;                                /* SUB HOLD: one note the bar long */
    sa_drop(&g);
    run_bars(&g, 1);
    clear_log();
    run_bars(&g, 1);
    CHECK(LG.per_part[0] >= 30, "ARP 1/32 over a held note: 32 a bar, got %d", LG.per_part[0]);

    /* own patterns: a drum step added and one taken away (in the copy, dropped with REC) */
    sa_play(&g, false);
    sa_init(&g, SR);
    sa_set_sound(&g, snd, 0);
    only(&g, 0);                                    /* the kick: 4 FLOOR */
    sa_step_drum(&g, 0, 4);                         /* the second kick off */
    sa_step_drum(&g, 0, 14);                        /* a pickup on */
    CHECK(g.ed[0].own && g.ed[0].hits == ((1u << 0) | (1u << 8) | (1u << 12) | (1u << 14)), "drum steps: %04x",
          g.ed[0].hits);
    CHECK(g.s.code[0].own == 0, "the edit waits for REC");
    sa_drop(&g);
    sa_play(&g, true);
    clear_log();
    run_bars(&g, 2);
    CHECK(LG.on[3][36] == 8, "own kick: 4 a bar, got %d", LG.on[3][36]);
    CHECK(sa_preview(&g, &g.s.code[0], 0, 0) == g.s.code[0].hits, "the preview shows the own steps");
    /* a synth layer's own notes: a key on a step, the same key again takes it off */
    sa_play(&g, false);
    sa_init(&g, SR);
    sa_set_sound(&g, snd, 0);
    only(&g, 3);                                    /* ROLLER: notes on 2 3 6 7 10 11 14 15 */
    sa_step_note(&g, 3, 0, 48);                     /* C3 on step 1 (A minor: C is in the scale) */
    CHECK(g.ed[3].sn[0] == 48 - 9, "the note is kept above the key (%d)", g.ed[3].sn[0]);
    sa_step_note(&g, 3, 2, 45);                     /* step 3: A2, ROLLER's own A2 there: it goes */
    CHECK(g.ed[3].sn[2] == SA_NO_NOTE, "the same note again: off (%d)", g.ed[3].sn[2]);
    sa_step_note(&g, 3, 4, 49);                     /* C#3: not in A minor, snapped to C3 or D3 */
    CHECK(g.ed[3].sn[4] == 48 - 9 || g.ed[3].sn[4] == 50 - 9, "snapped to the scale (%d)", g.ed[3].sn[4]);
    sa_step_len(&g, 3, 0, 3);
    CHECK(g.ed[3].sl[0] == 4, "the length: 4 sixteenths (%d)", g.ed[3].sl[0]);
    sa_drop(&g);
    sa_play(&g, true);
    clear_log();
    run_bars(&g, 1);
    CHECK(LG.on[0][48] == 1 + (g.s.code[3].sn[4] == 48 - 9), "the own C3 plays (%d)", LG.on[0][48]);
    CHECK(LG.per_part[0] == 9, "ROLLER's 8 notes, one off, two on: 9, got %d", LG.per_part[0]);
    /* a chord layer: a key toggles the chord */
    sa_play(&g, false);
    sa_init(&g, SR);
    sa_set_sound(&g, snd, 0);
    only(&g, 4);                                    /* STAB 2: chords on 4 and 11 */
    CHECK(sa_chordy(&g.ed[4]), "STAB 2 is a chord pattern");
    sa_step_note(&g, 4, 0, 60);
    CHECK(g.ed[4].chord == ((1u << 0) | (1u << 3) | (1u << 10)), "a chord added on step 1 (%04x)", g.ed[4].chord);
    sa_drop(&g);
    sa_play(&g, true);
    clear_log();
    run_bars(&g, 1);
    CHECK(LG.per_part[1] == 9, "three chords of three notes, got %d", LG.per_part[1]);

    /* three synth layers at most */
    sa_init(&g, SR);
    CHECK(sa_synth_count(&g) == 2, "first groove: two synth layers");
    int p = sa_synth_free_part(&g, -1);
    CHECK(p == 2, "the free part is 3 (index 2), got %d", p);
    g.ed[5].type = SA_T_SYNTH;
    g.ed[5].part = (uint8_t)p;
    CHECK(sa_synth_free_part(&g, -1) < 0, "three: no part left");

    /* the modifier list round-trips */
    for (int i = 0; i < sa_mod_opts(SA_T_SYNTH); ++i) {
        sa_mod m = sa_mod_opt(SA_T_SYNTH, i);
        CHECK(sa_mod_index(SA_T_SYNTH, m) == i || m.type == SA_M_REV || m.type == SA_M_OFF, "mod %d round-trip", i);
    }
    CHECK(sa_mod_opts(SA_T_DRUM) < sa_mod_opts(SA_T_SYNTH), "OCT only on synths");

    /* a broken record comes back in range */
    sa_settings s;
    memset(&s, 0xFF, sizeof s);
    sa_settings_fix(&s);
    int synths = 0;
    for (int l = 0; l < SA_LAYERS; ++l) synths += s.code[l].type == SA_T_SYNTH;
    CHECK(synths <= SA_SYNTHS, "fix: at most three synths, got %d", synths);
    CHECK(s.bpm <= 200 && s.harm.key < 12, "fix: ranges");

    /* every pattern with every modifier, two bars: no crash, notes within the bar */
    sa_init(&g, SR);
    sa_set_sound(&g, snd, 0);
    sa_play(&g, true);
    for (int t = SA_T_DRUM; t <= SA_T_SYNTH; ++t) {
        int np = t == SA_T_DRUM ? sa_drum_pats() : sa_synth_pats();
        for (int pi = 0; pi < np; ++pi)
            for (int mi = 0; mi < sa_mod_opts(t); mi += 3) {
                g.ed[3].type = (uint8_t)t;
                g.ed[3].part = t == SA_T_DRUM ? SA_PART_DRUMS : 0;
                g.ed[3].pat = (uint8_t)pi;
                g.ed[3].mod[0] = sa_mod_opt(t, mi);
                g.ed[3].mod[1] = sa_mod_opt(t, (mi * 7) % sa_mod_opts(t));
                g.ed[3].amode = (uint8_t)(pi % SA_AR_MODES);
                (void)sa_preview(&g, &g.ed[3], 3, 1);
                sa_drop(&g);
                run_bars(&g, 1);
            }
    }
    CHECK(g.nheld <= SA_HELD, "held in range");

    if (fails) { printf("sa_layer_test: %d failed\n", fails); return 1; }
    printf("sa_layer_test: all passed\n");
    return 0;
}
