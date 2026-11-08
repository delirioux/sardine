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
#define K SA_L_DRUM0                         /* the kick of the first groove */
#define BS SA_L_BASS
#define ST SA_L_CHORD
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
    only(&g, K);                                    /* stopped: drops at once */
    CHECK(sa_pending(&g) == 0, "dropped while stopped");
    sa_play(&g, true);
    clear_log();
    run_bars(&g, 4);
    CHECK(LG.on[3][36] == 16, "4 FLOOR: 16 kicks in 4 bars, got %d", LG.on[3][36]);

    /* FAST 2: twice as many */
    g.ed[K].mod[0].type = SA_M_FAST;
    g.ed[K].mod[0].val = 2;
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
    memset(g.ed[K].mod, 0, sizeof g.ed[K].mod);
    g.ed[K].mod[0].type = SA_M_LEN;
    g.ed[K].mod[0].val = 3;                         /* the first 3 sixteenths (a kick on 0) looped: a kick every 3 */
    sa_drop(&g);
    run_bars(&g, 1);                                /* (the drop's bar) */
    clear_log();
    run_bars(&g, 3);                                /* 48 sixteenths: a kick every 3 -> 16 */
    CHECK(LG.on[3][36] == 16, "LEN 3: a kick every 3 sixteenths, 16 in 3 bars, got %d", LG.on[3][36]);

    /* EUCLID 5/16 */
    g.ed[K].mod[0].type = SA_M_EUCLID;
    g.ed[K].mod[0].val = 5;
    sa_drop(&g);
    run_bars(&g, 1);
    clear_log();
    run_bars(&g, 2);
    CHECK(LG.on[3][36] == 10, "EUCLID 5: 5 a bar, got %d", LG.on[3][36]);

    /* THIN 50 % leaves some, not all */
    g.ed[K].mod[0].type = SA_M_FAST;
    g.ed[K].mod[0].val = 4;
    g.ed[K].mod[1].type = SA_M_THIN;
    g.ed[K].mod[1].val = 5;
    sa_drop(&g);
    run_bars(&g, 1);
    clear_log();
    run_bars(&g, 4);
    CHECK(LG.on[3][36] > 10 && LG.on[3][36] < 54, "FAST 4 THIN 50: between, got %d", LG.on[3][36]);

    /* the mute: at once */
    sa_mute(&g, K, true);
    clear_log();
    run_bars(&g, 1);
    CHECK(LG.on[3][36] == 0, "muted: silent, got %d", LG.on[3][36]);
    sa_mute(&g, K, false);

    /* CHANCE: 0 % none, 50 % some; ACCENT: the accents louder than the rest */
    g.s.live[K].cut = 0;
    clear_log();
    run_bars(&g, 2);
    CHECK(LG.on[3][36] == 0, "CHANCE 0: no kick, got %d", LG.on[3][36]);
    g.s.live[K].cut = 50;
    clear_log();
    run_bars(&g, 8);
    CHECK(LG.on[3][36] > 4 && LG.on[3][36] < 60, "CHANCE 50: some of 64, got %d", LG.on[3][36]);
    g.s.live[K].cut = 100;

    /* a synth layer follows the chord; the arpeggiator */
    sa_play(&g, false);
    sa_init(&g, SR);
    sa_set_sound(&g, snd, 0);
    only(&g, BS);                                    /* the ROLLER bass alone */
    sa_play(&g, true);
    clear_log();
    run_bars(&g, 2);
    CHECK(LG.per_part[2] == 16, "ROLLER: 8 notes a bar on part 1, got %d", LG.per_part[2]);
    /* A minor: the bass notes are A (57 - 12 = 45 at octave 2) or its octave/fifth */
    CHECK(LG.on[2][45] > 0, "the bass plays A2 in A minor");
    g.ed[BS].amode = SA_AR_UP;
    g.ed[BS].arate = SA_RATE_32;
    g.ed[BS].pat = 6;                                /* SUB HOLD: one note the bar long */
    sa_drop(&g);
    run_bars(&g, 1);
    clear_log();
    run_bars(&g, 1);
    CHECK(LG.per_part[2] >= 30, "ARP 1/32 over a held note: 32 a bar, got %d", LG.per_part[2]);

    /* own patterns: a drum step added and one taken away (in the copy, dropped with REC) */
    sa_play(&g, false);
    sa_init(&g, SR);
    sa_set_sound(&g, snd, 0);
    only(&g, K);                                    /* the kick: 4 FLOOR */
    sa_step_drum(&g, K, 4);                         /* the second kick off */
    sa_step_drum(&g, K, 14);                        /* a pickup on */
    CHECK(g.ed[K].own && g.ed[K].hits == ((1u << 0) | (1u << 8) | (1u << 12) | (1u << 14)), "drum steps: %04x",
          g.ed[K].hits);
    CHECK(g.s.code[K].own == 0, "the edit waits for REC");
    sa_drop(&g);
    sa_play(&g, true);
    clear_log();
    run_bars(&g, 2);
    CHECK(LG.on[3][36] == 8, "own kick: 4 a bar, got %d", LG.on[3][36]);
    CHECK(sa_preview(&g, &g.s.code[K], K, 0) == g.s.code[K].hits, "the preview shows the own steps");
    /* a synth layer's own notes: a key on a step, the same key again takes it off */
    sa_play(&g, false);
    sa_init(&g, SR);
    sa_set_sound(&g, snd, 0);
    only(&g, BS);                                    /* ROLLER: notes on 2 3 6 7 10 11 14 15 */
    sa_step_note(&g, BS, 0, 48);                     /* C3 on step 1 (A minor: C is in the scale) */
    CHECK(g.ed[BS].sn[0] == 48 - 9, "the note is kept above the key (%d)", g.ed[BS].sn[0]);
    sa_step_note(&g, BS, 2, 45);                     /* step 3: A2, ROLLER's own A2 there: it goes */
    CHECK(g.ed[BS].sn[2] == SA_NO_NOTE, "the same note again: off (%d)", g.ed[BS].sn[2]);
    sa_step_note(&g, BS, 4, 49);                     /* (0.8.37) C#3, SNAP OFF (the default): as pressed */
    CHECK(g.ed[BS].sn[4] == 49 - 9, "SNAP OFF: the note as pressed (%d)", g.ed[BS].sn[4]);
    g.snap = true;                                   /* SNAP ON: C#3 not in A minor, snapped to C3 or D3 */
    sa_step_note(&g, BS, 5, 49);
    CHECK(g.ed[BS].sn[5] == 48 - 9 || g.ed[BS].sn[5] == 50 - 9, "SNAP ON: snapped to the scale (%d)", g.ed[BS].sn[5]);
    g.snap = false;
    sa_step_len(&g, BS, 0, 3);
    CHECK(g.ed[BS].sl[0] == 4, "the length: 4 sixteenths (%d)", g.ed[BS].sl[0]);
    sa_drop(&g);
    sa_play(&g, true);
    clear_log();
    run_bars(&g, 1);
    CHECK(LG.on[2][48] == 1 + (g.s.code[BS].sn[5] == 48 - 9), "the own C3 plays (%d)", LG.on[2][48]);
    CHECK(LG.on[2][49] == 1, "the C#3 as pressed plays (%d)", LG.on[2][49]);
    CHECK(LG.per_part[2] == 10, "ROLLER's 8 notes, one off, three on: 10, got %d", LG.per_part[2]);
    /* a chord layer: a key toggles the chord */
    sa_play(&g, false);
    sa_init(&g, SR);
    sa_set_sound(&g, snd, 0);
    only(&g, ST);                                    /* STAB 2: chords on 4 and 11 */
    CHECK(sa_chordy(&g.ed[ST]), "STAB 2 is a chord pattern");
    sa_step_note(&g, ST, 0, 60);
    CHECK(g.ed[ST].chord == ((1u << 0) | (1u << 3) | (1u << 10)), "a chord added on step 1 (%04x)", g.ed[ST].chord);
    sa_drop(&g);
    sa_play(&g, true);
    clear_log();
    run_bars(&g, 1);
    CHECK(LG.per_part[1] == 9, "three chords of three notes, got %d", LG.per_part[1]);

    /* the gate: a held chord cut to 8ths by its volume, no new notes */
    sa_play(&g, false);
    sa_init(&g, SR);
    sa_set_sound(&g, snd, 0);
    only(&g, ST);
    g.ed[ST].pat = 20;                               /* WASH: the chord held the bar */
    g.ed_gate[ST].pat = 2;                           /* 8THS */
    CHECK(sa_pending(&g) == 1, "the gate is code (one layer pending)");
    sa_drop(&g);
    sa_play(&g, true);
    clear_log();
    {
        int opens = 0, closes = 0, was = -1;
        for (int i = 0; i < bar_frames(&g) * 2 / 128; ++i) {
            sa_process(&g, 128);
            int closed = g.gate_off[1] > 100, open = g.gate_off[1] < 20;
            if (open && was == 0) opens++;
            if (closed && was == 1) closes++;
            if (open) was = 1;
            if (closed) was = 0;
        }
        CHECK(opens >= 14 && closes >= 14, "8THS: open and closed 16 times in 2 bars (%d %d)", opens, closes);
        CHECK(LG.per_part[1] == 6, "the chord played twice, not retriggered by the gate (%d notes)", LG.per_part[1]);
    }
    sa_gate_step(&g, ST, 1);                         /* an own gate step */
    CHECK(g.ed_gate[ST].own && g.ed_gate[ST].mask == (0x5555 | 2), "a gate step on the keys (%04x)", g.ed_gate[ST].mask);

    /* the fixed layers: LEAD CHORD BASS on parts 0 1 2, the drums after them; a moved synth layer goes back */
    sa_init(&g, SR);
    CHECK(sa_synth_count(&g) == 3 && g.s.code[SA_L_LEAD].pat != SA_PAT_OFF, "first groove: three synth layers, LEAD on (in after a pattern)");
    {
        sa_settings t = g.s;
        sa_code bass = t.code[SA_L_BASS];
        t.code[SA_L_BASS] = t.code[7];
        t.code[7] = bass;
        sa_settings_fix(&t);
        CHECK(t.code[SA_L_BASS].type == SA_T_SYNTH && t.code[SA_L_BASS].part == SA_L_BASS &&
              t.code[SA_L_BASS].sound == bass.sound, "a synth layer out of place: back on its part's slot");
    }

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

    /* a fresh groove: a kick, hats, a clap or snare; BASS on its part; CHORD stabs or a pad; LEAD mostly OFF */
    {
        static const uint8_t PB[] = {10, 11, 12}, PK[] = {20, 21}, PP[] = {30, 31, 32}, PL[] = {40, 41};
        sa_pool pool = {PB, PK, PP, PL, 3, 2, 3, 2};
        int differ = 0, gated = 0, leads = 0;
        sa_settings a, b;
        for (uint32_t seed = 1; seed <= 200; ++seed) {
            sa_random(&a, seed, &pool);
            int drums = 0, kick = 0;
            for (int l = SA_L_DRUM0; l < SA_LAYERS; ++l) {
                drums += a.code[l].type == SA_T_DRUM;
                kick += a.code[l].type == SA_T_DRUM && a.code[l].sound == SA_DV_KICK;
            }
            const sa_code *bs = &a.code[SA_L_BASS], *ch = &a.code[SA_L_CHORD], *ld = &a.code[SA_L_LEAD];
            CHECK(bs->type == SA_T_SYNTH && bs->part == SA_L_BASS && bs->sound >= 10 && bs->sound <= 12 &&
                  bs->pat != SA_PAT_OFF, "seed %u: BASS from the bass list, playing", seed);
            bool pad = ch->sound >= 30;
            const char *pn = sa_pat_name(SA_T_SYNTH, ch->pat);
            CHECK(ch->type == SA_T_SYNTH && ch->part == SA_L_CHORD && (pad ? ch->sound <= 32 : (ch->sound == 20 ||
                  ch->sound == 21)), "seed %u: CHORD a stab or a pad", seed);
            CHECK(pad ? !strcmp(pn, "WASH") || !strcmp(pn, "TWO HALVES") || !strcmp(pn, "SWELL") : !pad,
                  "seed %u: a pad's pattern (%s)", seed, pn);
            CHECK(ch->amode == SA_AR_OFF, "seed %u: ARP off", seed);
            CHECK(ld->type == SA_T_SYNTH && ld->part == SA_L_LEAD && (ld->sound == 40 || ld->sound == 41),
                  "seed %u: LEAD from the lead list", seed);
            leads += ld->pat != SA_PAT_OFF;
            gated += sa_gate_on(&a.gate[SA_L_CHORD]);
            CHECK(drums >= 3 && kick == 1, "seed %u: three drums or more, one kick (%d, %d)", seed, drums, kick);
            CHECK(a.bpm >= 120 && a.bpm <= 130, "seed %u: a club tempo, %d", seed, a.bpm);
            sa_random(&b, seed, &pool);
            CHECK(!memcmp(&a, &b, sizeof a), "seed %u: the same seed, the same groove", seed);
            sa_random(&b, seed + 1, &pool);
            differ += memcmp(&a, &b, sizeof a) != 0;
        }
        CHECK(differ == 200, "each seed its own groove (%d of 200)", differ);
        CHECK(gated > 0 && leads == 200, "some gated pads (%d), always a lead (%d)", gated, leads);

        /* a whole groove in: stopped at once, playing on the bar line; the look stays, what waited goes */
        sa_init(&g, SR);
        sa_set_sound(&g, snd, 0);
        g.s.look = 1;
        g.s.palette = 2;
        sa_random(&a, 7, &pool);
        a.look = 0;
        sa_set_arm(&g, &a);
        CHECK(!g.set_next && g.s.bpm == a.bpm && !memcmp(g.s.code, a.code, sizeof a.code), "stopped: in at once");
        CHECK(g.s.look == 1 && g.s.palette == 2, "the look and the colour stay");
        sa_random(&b, 8, &pool);
        sa_play(&g, true);
        run_bars(&g, 1);
        g.ed[0].mod[0] = sa_mod_opt(SA_T_DRUM, 1);
        sa_set_arm(&g, &b);
        CHECK(g.set_next == &b && g.s.bpm == a.bpm, "playing: armed, the old one plays on");
        run_bars(&g, 1);
        CHECK(!g.set_next && !memcmp(g.s.code, b.code, sizeof b.code) && sa_pending(&g) == 0,
              "on the bar line: in, the copy with it");
        sa_play(&g, false);
    }

    /* INTRO: the kick first, the others coming in; ARC: its break, and the drop */
    sa_init(&g, SR);
    sa_set_sound(&g, snd, 0);
    g.s.intro = 2;                                  /* 8 bars */
    g.s.arc = 1;                                    /* 16 bars */
    sa_play(&g, true);
    clear_log();
    run_bars(&g, 1);
    CHECK(LG.on[3][36] == 4 && LG.per_part[2] == 0 && LG.per_part[1] == 0, "INTRO bar 1: the kick alone (%d %d %d)",
          LG.on[3][36], LG.per_part[2], LG.per_part[1]);
    CHECK(g.sweep > 0.5f, "INTRO: the filter closed at first");
    run_bars(&g, 7);
    clear_log();
    run_bars(&g, 1);
    CHECK(LG.per_part[2] > 0 && LG.per_part[1] > 0, "after INTRO: bass and chord in");
    run_bars(&g, 8);
    clear_log();
    run_bars(&g, 2);
    CHECK(LG.on[3][36] == 0 && LG.per_part[2] == 0 && LG.per_part[1] > 0, "ARC: the break, the chord alone");
    run_bars(&g, 6);
    clear_log();
    run_bars(&g, 1);
    CHECK(LG.on[3][36] == 4, "ARC: the drop, the kick back");
    sa_play(&g, false);

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

    {   /* no diminished chord in any scale or progression (dorian's VI, major's VII, minor's ii: the chord a third down) */
        static sa_groove h;
        int dim = 0;
        char where[48] = "";
        for (int sc = 0; sc < SA_SCALES; ++sc)
            for (int pr = 0; pr < SA_PROGS; ++pr) {
                sa_init(&h, SR);
                sa_set_sound(&h, snd, 0);
                h.ed_harm.key = 7, h.ed_harm.scale = (uint8_t)sc, h.ed_harm.prog = (uint8_t)pr, h.ed_harm.bars = 1;
                sa_drop(&h);
                sa_play(&h, true);
                for (int b = 0; b < 4; ++b) {
                    run_bars(&h, 1);
                    if (strstr(h.chord_name, "dim")) { dim++; snprintf(where, sizeof where, "%d/%d %s", sc, pr, h.chord_name); }
                }
            }
        CHECK(dim == 0, "no diminished chords (%d, %s)", dim, where);
    }
    {   /* no ride, no crash: PERC lists its own percussion; a stored ride or crash is the tambourine */
        uint8_t v[16];
        int n = sa_row_voices(SA_DR_PERC, v), cym = 0;
        for (int i = 0; i < n; ++i) cym |= v[i] == SA_DV_RIDE || v[i] == SA_DV_CRASH;
        static sa_settings z;
        sa_defaults(&z);
        z.code[SA_L_DRUM0 + SA_DR_PERC].sound = SA_DV_CRASH;
        sa_settings_fix(&z);
        CHECK(!cym && n == 6 && z.code[SA_L_DRUM0 + SA_DR_PERC].sound == SA_DV_TAMB && sa_dv_gm(SA_DV_TAMB) == 82 &&
                  !strcmp(sa_row_name(SA_DR_PERC), "PERC"),
              "PERC: %d drums, no cymbal; a stored crash is the tambourine", n);
    }

    /* PHRASE (A A A' B: bar 4's last beat quiet on the bass) and PART B (on the next phrase) */
    {
        static sa_groove h;
        sa_init(&h, SR);
        sa_set_sound(&h, snd, 0);
        only(&h, BS);
        h.s.code[BS].pat = 4;                       /* PUMP 16: a note every sixteenth */
        sa_edit_reset(&h);
        int nb[8];
        sa_play(&h, true);
        for (int b = 0; b < 8; ++b) { clear_log(); run_bars(&h, 1); nb[b] = LG.per_part[2]; }
        CHECK(nb[3] < nb[0] && nb[7] < nb[4] && nb[1] == nb[0], "PHRASE: bar 4 the answer, room at its end (%d %d %d %d)",
              nb[0], nb[1], nb[2], nb[3]);
        h.s.phrase = 1;
        for (int b = 0; b < 4; ++b) { clear_log(); run_bars(&h, 1); nb[b] = LG.per_part[2]; }
        CHECK(nb[3] == nb[0], "PHRASE OFF: every bar the same (%d %d)", nb[0], nb[3]);
        h.s.patb[BS] = 1;                           /* B: OFFBEAT (pattern 0) */
        sa_part(&h, true);
        int was = h.part_b;
        for (int b = 0; b < 4 && !h.part_b; ++b) run_bars(&h, 1);
        clear_log();
        run_bars(&h, 1);
        CHECK(!was && h.part_b && LG.per_part[2] == 4, "PART B on the phrase: B's line (%d notes)", LG.per_part[2]);
        h.s.abauto = 1;                             /* A/B every 8 bars by itself */
        int flips = 0, last = h.part_b;
        for (int b = 0; b < 32; ++b) { run_bars(&h, 1); flips += h.part_b != last; last = h.part_b; }
        CHECK(flips == 4, "A/B every 8 bars: %d turns in 32", flips);
        int ok = 0, ones = 0;                       /* (0.8.31) the bars left before PART changes, as the header shows */
        for (int b = 0; b < 16; ++b) {
            bool tb = false;
            int left = sa_part_next(&h, &tb), was_b = h.part_b;
            run_bars(&h, 1);
            ones += left == 1;
            ok += left == 1 ? h.part_b == tb && h.part_b != was_b : h.part_b == was_b && left >= 1 && left <= 8;
        }
        CHECK(ok == 16 && ones == 2, "A/B: the bars left before it turns, right every bar (%d of 16, %d last bars)", ok, ones);
        h.s.abauto = 0;
        sa_part(&h, !h.part_b);
        int cue = sa_part_next(&h, 0);
        CHECK(cue >= 1 && cue <= 4, "PART cued: on the phrase, %d bars left", cue);
        sa_part(&h, h.part_b);
        CHECK(sa_part_next(&h, 0) == 0, "nothing cued, A/B OFF: no change coming");
        {   /* (0.8.32) EVOLVE: the part that goes to rest comes back other; NEW: all of the resting part */
            static sa_settings rz;
            sa_random(&rz, 11, &(sa_pool){0});
            h.s = rz;
            h.part_b = h.part_cue = false;
            sa_edit_reset(&h);
            sa_settings before = h.s;
            h.s.evolve = 1;
            h.s.abauto = 1;
            bool was_b = h.part_b;
            for (int b = 0; b < 9 && h.part_b == was_b; ++b) run_bars(&h, 1);
            int diff = 0;
            for (int l = 0; l < SA_LAYERS; ++l)
                diff += was_b ? h.s.patb[l] != before.patb[l] : h.s.code[l].pat != before.code[l].pat;
            CHECK(h.part_b != was_b && diff == 1 && !memcmp(h.ed, h.s.code, sizeof h.ed),
                  "EVOLVE LIGHT: the part gone to rest, one layer new (%d), nothing left waiting", diff);
            h.s.evolve = 0;
            h.s.abauto = 0;
            sa_part(&h, h.part_b);
            before = h.s;
            int n = sa_part_new(&h), d2 = 0;
            for (int l = 0; l < SA_LAYERS; ++l)
                d2 += h.part_b ? h.s.code[l].pat != before.code[l].pat : h.s.patb[l] != before.patb[l];
            bool prog = h.part_b ? h.s.harm.prog != before.harm.prog : h.s.progb != before.progb;
            CHECK(n >= 3 && d2 == n && prog && !memcmp(&h.s.code[SA_L_DRUM0], &before.code[SA_L_DRUM0], sizeof h.s.code[0]),
                  "NEW: the resting part, %d layers and its chords, the kick left", n);
        }
        {   /* (0.8.40) steps edited by hand play in PART B too: the row's own B line goes */
            int l = SA_L_DRUM0 + SA_DR_HATS;
            h.s.abauto = h.s.evolve = 0;
            h.s.patb[l] = h.ed_patb[l] = (uint8_t)((h.s.code[l].pat + 3) % 20 + 1);   /* (another line in B) */
            h.part_b = h.part_cue = true;
            sa_step_drum(&h, l, 1);
            sa_step_drum(&h, l, 9);
            uint16_t want = h.ed[l].hits;
            sa_drop(&h);
            run_bars(&h, 2);
            CHECK(h.part_b && h.s.patb[l] == 0 && h.s.code[l].own && h.hits_last[l] == want,
                  "PART B, the hats edited by hand: B plays the edit (%04x, wanted %04x)", h.hits_last[l], want);
            h.part_b = h.part_cue = false;
        }
        sa_settings z;
        sa_random(&z, 7, &(sa_pool){0});
        CHECK(z.progb != 0 && z.patb[BS] != 0 && z.patb[BS] != z.code[BS].pat + 1, "a fresh groove has a B");
    }

    if (fails) { printf("sa_layer_test: %d failed\n", fails); return 1; }
    printf("sa_layer_test: all passed\n");
    return 0;
}
