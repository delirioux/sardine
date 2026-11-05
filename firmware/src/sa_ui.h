/* SPDX-License-Identifier: GPL-3.0-only */
/* SARDINE screen and controls: Sardine's screens on Felucca's drawing (gfx.c: canvas, fonts, palettes, icons), in the
 * family's frame (the header, four knob cards, the panel, the footer). Compiled after gfx.c in one unit (the firmware's
 * felucca.c through sa_fw.c, or a host test).
 *
 * The layers (0.7), always there, each can be OFF: LEAD, CHORD, BASS (a synth part each) and DRUMS (every drum on the
 * kit). HOME shows them all playing. On a layer the FX button turns through its pages, from its home page; another
 * layer always opens on its home page.
 *
 * The controls
 *   SELECT     the tempo, everywhere
 *   PRESETS    HOME, LEAD, CHORD, BASS, DRUMS
 *   ALGORITHM  the pattern of the layer (OFF first; a drum: OFF, then REMOVE; + ADD: which drum). Waits for SEQ
 *   KNOB 1..4  HOME: FILTER FEEL PUMP SPACE. A synth layer's pages: its home CUTOFF RESO DRIVE ENV (at once);
 *              SOUND (TYPE, SOUND, PUMP: at once); ARP on CHORD (MODE RATE RANGE GATE); PAN (at once); GATE; MOD (the
 *              four modifiers); MIX (MUTE, LEVEL: at once). DRUMS: DRUM LEVEL ACCENT CHANCE; KIT (KIT TUNE TONE DECAY);
 *              MOD; MIX
 *   FX         a layer: its next page (the last: back to its home)
 *   SEQ        drop what waits (pattern, steps, modifiers, ARP, GATE, the key): playing, on the next bar line
 *   REC        step recording on a layer, on and off: a synth layer's steps (KNOB 1 the step, KNOB 2 its length, a
 *              key its note), a drum's 16 steps on the lowest keys, on GATE the gate's steps
 *   PLAY       start / stop
 *   SCL        the KEY page (KEY SCALE CHORDS BARS), again: back
 *   SAVE       SETS (eight sets and LAST), again: back
 *   HOME       HOME; held: MENU (colour, look)
 *   keys       the layer open, live (HOME: LEAD), snapped to the scale; REC: its steps. Music and steps only
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
enum { SV_PG_MAIN, SV_PG_LAYER, SV_PG_KEY, SV_PG_MENU, SV_PG_SETS, SV_PAGES };
enum { SV_V_HOME, SV_V_LEAD, SV_V_CHORD, SV_V_BASS, SV_V_DRUMS, SV_VIEWS };       /* what PRESETS turns through */
enum { SVP_HOME, SVP_SOUND, SVP_ARP, SVP_PAN, SVP_GATE, SVP_MOD, SVP_MIX, SVP_KIT };  /* a layer's pages (FX) */
#define SV_KEY_LO 53                      /* F3: the FM-1's lowest key */
#define SV_KEY_HI 79                      /* G5 */

/* SETS: eight grooves in the flash (the glue keeps them; the screen shows what each holds and asks for the action) */
#define SV_SETS 8
#define SV_LAST SV_SETS                       /* the ninth slot: the last groove, kept by itself (LOAD only) */
#define SV_SLOTS (SV_SETS + 1)
enum { SV_SA_LOAD, SV_SA_STORE, SV_SA_NEW, SV_SA_CLEAR, SV_SA_ACTS };
typedef struct {
    uint8_t used, bpm, key, scale, kit, nsyn;
    uint8_t snd[SA_SYNTHS];                           /* its synth layers' sounds */
    uint8_t wait;                                     /* stored while playing: written once stopped */
} sv_set_info;

typedef struct {
    sa_groove *g;
    int page;
    int view;                             /* SV_V_*: the layer open (HOME: none) */
    int fx;                               /* the page of it FX turned to (0: its home) */
    bool rec;                             /* REC: step recording */
    int layer;                            /* the layer's slot (DRUMS: the drum picked; -1: + ADD) */
    int drow;                             /* DRUMS: the row picked (the last one: + ADD) */
    int back;                             /* the page KEY goes back to */
    int scur;                             /* REC on a synth layer: the step the keys write */
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
    sv_set_info sets[SV_SLOTS];           /* what each set holds, and LAST (the glue fills it) */
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
