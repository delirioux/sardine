/* SPDX-License-Identifier: GPL-3.0-only */
/* SARDINE screen and controls: Sardine's screens on Felucca's drawing (gfx.c: canvas, fonts, palettes, icons), in the
 * family's frame (the header, four knob cards, the panel, the footer). Compiled after gfx.c in one unit (the firmware's
 * felucca.c through sa_fw.c, or a host test).
 *
 * The screens: MAIN (the whole stack), DRUMS (every drum layer, a row each, their steps on the keys), one screen per
 * synth layer, NEW (a synth layer, past the last one); on a synth layer its ARP and STEPS pages; the KEY page; SETS;
 * MENU.
 *
 * The controls
 *   SELECT     through the screens: MAIN, DRUMS, each synth layer, NEW
 *   MASTER     volume (Felucca's)
 *   KNOB 1..4  MAIN: TEMPO FILTER FEEL KIT; SPACE (HOME on MAIN): PUMP REVERB DELAY WIDTH. A synth layer: CUTOFF RESO DRIVE ENV; a drum layer: LEVEL, and the kit's
 *              TUNE TONE DECAY. At once
 *   PRESETS    a layer: picks a chip of its code (SOUND, PATTERN, the modifiers, +, ARP); DRUMS: picks a row
 *   ALGORITHM  a layer: changes the chip picked; DRUMS: the row's pattern (+ ADD A DRUM: a new drum). In the copy:
 *              REC drops it
 *   EDIT       DRUMS: the row picked as a layer screen (its modifiers), again: back
 *   SEQ        a synth layer: STEPS (KNOB 1 the step, KNOB 2 its length, a key its note), again: back
 *   LFO        a synth layer: PAN (MODE: FIXED SWEEP PING-PONG RANDOM, PAN, WIDTH, RATE; at once), again: back
 *   ENV        a synth layer: GATE (KNOB 1 a ready gate, DEPTH WIDTH SMOOTH; the lowest 16 keys its steps), again: back
 *   REC        drop the copy in: playing, on the next bar line; stopped, at once
 *   PLAY       start / stop
 *   FX         a layer, a DRUMS row: mute it (at once)
 *   ARP        a synth layer: its ARP page (MODE RATE RANGE GATE), again: back
 *   SCL        the KEY page (KEY SCALE CHORDS BARS), again: back
 *   SAVE       SETS: eight grooves kept in the flash (KNOB 1 or PRESETS the set, KNOB 2 LOAD STORE NEW CLEAR, REC does
 *              it; a load or a new groove goes in on the one), again: back
 *   HOME       MAIN, on MAIN: SPACE and back; held: MENU (colour, look)
 *   keys       the open layer, live (snapped to the scale); DRUMS: the lowest 16 are the row's steps (a hit on, off),
 *              the others play it; STEPS: the step's note. Music and steps only
 *   OCT- OCT+  the keys' octave */
#ifndef SA_UI_H
#define SA_UI_H

#include <stdbool.h>
#include <stdint.h>
#include "sa_layer.h"

enum { SV_K_MASTER, SV_K_SELECT, SV_K_PRESETS, SV_K_ALGO, SV_K1, SV_K2, SV_K3, SV_K4, SV_KNOBS };
enum {
    SV_B_FX, SV_B_SCL, SV_B_ENV, SV_B_LFO, SV_B_EDIT, SV_B_GLO,
    SV_B_HOME, SV_B_SAVE, SV_B_ARP, SV_B_SEQ, SV_B_PLAY, SV_B_REC,
    SV_B_OCTDN, SV_B_OCTUP, SV_BUTTONS
};
enum { SV_PG_MAIN, SV_PG_LAYER, SV_PG_ARP, SV_PG_KEY, SV_PG_MENU, SV_PG_DRUMS, SV_PG_STEPS, SV_PG_GATE, SV_PG_PAN,
       SV_PG_SETS, SV_PAGES };
#define SV_KEY_LO 53                      /* F3: the FM-1's lowest key */
#define SV_KEY_HI 79                      /* G5 */

/* SETS: eight grooves in the flash (the glue keeps them; the screen shows what each holds and asks for the action) */
#define SV_SETS 8
enum { SV_SA_LOAD, SV_SA_STORE, SV_SA_NEW, SV_SA_CLEAR, SV_SA_ACTS };
typedef struct {
    uint8_t used, bpm, key, scale, kit, nsyn;
    uint8_t snd[SA_SYNTHS];                           /* its synth layers' sounds */
    uint8_t wait;                                     /* stored while playing: written once stopped */
} sv_set_info;

typedef struct {
    sa_groove *g;
    int page;
    int layer;                            /* the layer open (SV_PG_LAYER, SV_PG_ARP); -1 on MAIN */
    int back;                             /* the page KEY goes back to */
    int drow;                             /* DRUMS: the row picked (the last one: + ADD A DRUM) */
    bool space;                           /* MAIN's second page: SPACE (PUMP REVERB DELAY WIDTH) */
    bool from_drums;                      /* the layer open came from DRUMS (EDIT goes back there) */
    int scur;                             /* STEPS: the step the keys write */
    int chip;                             /* the chip PRESETS picked */
    int hot, hot_ms;                      /* the knob just turned (its card lit); 9: the chip */
    char msg[32];
    int msg_ms;
    int home_ms;
    bool home_down;
    int oct;
    int key_sent[27];
    uint32_t frame;
    int batt;
    bool changed;                         /* a setting changed: the glue applies it (and keeps the look) */
    sv_set_info sets[SV_SETS];            /* what each set holds (the glue fills it) */
    int set_slot, set_act;                /* SETS: the set picked, the action (SV_SA_*) */
    int req_act, req_slot;                /* REC on SETS: what the glue is to do (req_act -1: nothing) */
    int sback, sback_layer;               /* the page SAVE goes back to */
    uint32_t sig_head, sig_card[4], sig_foot;
    bool force;
    /* the previews (the code's bar, a bit a sixteenth), worked out again when the code changes */
    sa_code pv_code[2];
    uint16_t pv_bits[2];
    int pv_layer, pv_bar;
    uint16_t main_bits[SA_LAYERS];
    sa_code main_code[SA_LAYERS];
    int main_ms;
    uint16_t ed_bits[SA_LAYERS];          /* the copy's bars (DRUMS' rows) */
    sa_code ed_code[SA_LAYERS];
} sa_ui;

void sv_init(sa_ui *u, sa_groove *g);
void sv_knob(sa_ui *u, int knob, int delta);
void sv_button(sa_ui *u, int button, bool down);
void sv_key(sa_ui *u, int note, bool down);
void sv_tick(sa_ui *u, int ms);
void sv_draw(sa_ui *u);
void sv_draw_all(sa_ui *u);
int sv_page_button(const sa_ui *u);       /* the button whose LED shows the page (SV_B_*), -1 none */
/* the keys' LEDs while editing (bit k: the key SV_KEY_LO + k): DRUMS lights the 16 step keys dim and the steps set
 * full; STEPS lights the scale's keys dim and the step's note full. False: no editing (the glue's own) */
bool sv_key_leds(const sa_ui *u, uint32_t *full, uint32_t *dim);

#endif
