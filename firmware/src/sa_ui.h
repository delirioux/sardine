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
 *   KNOB 1..4  HOME: FILTER FEEL PUMP SPACE (FX: RESO WIDTH). A synth layer's pages: its home CUTOFF RESO DRIVE ENV (at once);
 *              SOUND (TYPE, SOUND, PUMP: at once); ARP (MODE RATE RANGE GATE); PAN (at once); 3D (MODE: OFF FIXED
 *              ORBIT RANDOM FLY, ANGLE or RATE, HEIGHT, DISTANCE: at once, headphones); PUMP (DEPTH RELEASE SOURCE ON, at once); GATE; MOD (the
 *              four modifiers); MIX (MUTE, LEVEL: at once). DRUMS: DRUM LEVEL ACCENT CHANCE; KIT (KIT TUNE TONE DECAY);
 *              MOD; MIX
 *   FX         a layer: its next page (the last: back to its home)
 *   SEQ        drop what waits (pattern, steps, modifiers, ARP, GATE, the key): playing, on the next bar line
 *   REC        step recording on a layer, on and off: a synth layer's steps (KNOB 1 the step, KNOB 2 its length, a
 *              key its note), a drum's 16 steps on the lowest keys, on GATE the gate's steps
 *   PLAY       start / stop
 *   SCL        the KEY page (KEY SCALE CHORDS BARS), again: back
 *   SAVE       SETS (eight sets and LAST), again: back
 *   EDIT       a layer's pattern editor: KNOB 1 the step, KNOB 2 FILL (CURR, the pattern as it was, or a ready fill:
 *              a line, a drum fill), KNOB 3 its LENGTH (a drum: its HIT); REC: a key writes the step (a drum: the 16
 *              lowest keys its steps, lit); again: back
 *   ENV        held: a drum fill from the next beat, in place of the drums, a different one each time (ROLL SNARE
 *              BUILD TOMS STUTTER); let go: the groove again
 *   ARP        held: BREAK, the drums and the bass out at once; let go: back in on the next 1
 *   LFO        held: SWEEP, the whole mix's filter closing over two bars; let go: open at once
 *   GLO        GLOBAL, each press its next page: the MIXER (LEAD CHORD BASS DRUMS: their VOLUME), their MUTE, their
 *              SOLO, STYLE INTRO ARC (SEQ: a fresh groove in the STYLE, on the 1); past the last: back
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
enum { SV_PG_MAIN, SV_PG_LAYER, SV_PG_KEY, SV_PG_MENU, SV_PG_SETS, SV_PG_MIXER, SV_PG_EDIT, SV_PAGES };
enum { SV_V_HOME, SV_V_LEAD, SV_V_CHORD, SV_V_BASS, SV_V_DRUMS, SV_VIEWS };       /* what PRESETS turns through */
enum { SVP_HOME, SVP_SOUND, SVP_ARP, SVP_PAN, SVP_GATE, SVP_MOD, SVP_MIX, SVP_KIT, SVP_3D, SVP_PUMP };  /* a layer's pages (FX) */
#define SV_KEY_LO 53                      /* F3: the FM-1's lowest key */
#define SV_KEY_HI 79                      /* G5 */

/* SETS: eight grooves in the flash (the glue keeps them; the screen shows what each holds and asks for the action) */
#define SV_SETS 8
#define SV_LAST SV_SETS                       /* the ninth slot: the last groove, kept by itself (LOAD only) */
#define SV_SLOTS (SV_SETS + 1)
enum { SV_SA_LOAD, SV_SA_STORE, SV_SA_NEW, SV_SA_CLEAR, SV_SA_ACTS, SV_SA_STYLE = 9, SV_SA_LIVE = 10, SV_SA_MIX = 11,
       SV_SA_PART = 12 };   /* (STYLE, LIVE, STYLE MIX:
                                                                                                  * GLO's pages) */
typedef struct {
    uint8_t used, bpm, key, scale, kit, nsyn;
    uint8_t snd[SA_SYNTHS];                           /* its synth layers' sounds */
    uint8_t wait;                                     /* stored while playing: written once stopped */
} sv_set_info;

/* MOVES (0.8.5): tap OCT- OCT+ EDIT REC as ever; held past SV_MOVE_MS, a live move while held (the glue plays them):
 * OCT- WASH (everything into the reverb; back dry on the next 1), OCT+ THROW (into the delay, the echoes ring out),
 * EDIT STOP (a tape stop), REC WOBBLE (the synth layers' filter on the eighths) */
enum { SV_MV_WASH, SV_MV_THROW, SV_MV_STOP, SV_MV_WOBBLE, SV_MOVES };
#define SV_MOVE_MS 300
/* STYLE MIX's KEEP (the layers playing on through it) and its bars out and in */
#define SV_MIX_KEEPS 6
#define SV_MIX_OUTS 3
#define SV_MIX_INS 4
const char *sv_mix_keep_name(int k);
uint16_t sv_mix_keep_mask(int k);
int sv_mix_out_bars(int i);
int sv_mix_in_bars(int i);
#define SV_NEW_MS 1000

/* SONG (GLO's 5th page): up to eight steps, each a set for a number of bars, played in turn round and round
 * while SONG is ON (the glue loads each on its bar line); kept with the preferences */
#define SV_STEPS 8
#define SV_SONG_BARS 5                        /* 4 8 16 32 64 */
typedef struct {
    uint8_t set[SV_STEPS];                    /* 0: END (nothing), 1..8 the set */
    uint8_t bars[SV_STEPS];                   /* 0..4: 4 8 16 32 64 bars */
    uint8_t on, pad[3];
} sv_song;
int sv_song_bars(int i);
void sv_song_default(sv_song *s);

typedef struct {
    sa_groove *g;
    int page;
    int view;                             /* SV_V_*: the layer open (HOME: none) */
    int fx;                               /* the page of it FX turned to (0: its home) */
    bool rec;                             /* REC: step recording */
    int layer;                            /* the layer's slot (DRUMS: the drum picked; -1: + ADD) */
    int drow;                             /* DRUMS: the row picked (the last one: + ADD) */
    int back;                             /* the page KEY goes back to */
    int mback;                            /* the page the MIXER goes back to */
    int home2;                            /* HOME's page (FX turns them): 0 FILTER FEEL PUMP SPACE, 1 SPACE (RESO, WIDTH),
                                           * 2 PARTS (PHRASE, A/B, EVOLVE) */
    bool menu2;                           /* MENU's second page (FX): MIDI */
    uint8_t midi_ch[4];                   /* MIDI: the channel each layer sends on (LEAD CHORD BASS DRUMS): 0 OFF, 1..16
                                           * (the glue keeps them with the preferences) */
    /* MIDI: what went out, the last SV_MLOG (the glue writes them, from the audio) */
#define SV_MLOG 8
    volatile uint8_t ml_layer[SV_MLOG], ml_ch[SV_MLOG], ml_note[SV_MLOG], ml_vel[SV_MLOG];
    volatile uint32_t ml_n;
    int mixmute;                          /* GLO's page: 0 VOLUME, 1 MUTE, 2 SOLO, 3 STYLE, 4 LIVE, 5 SONG (GLO turns them);
                                           * 6 STYLE MIX (GLO held) */
    int mix_pick, mix_keep, mix_out, mix_in;   /* STYLE MIX (0.8.25): TO (a style), KEEP (SV_MIX_KEEPS), OUT, IN (an index
                                                * of SV_MIX_OUT / SV_MIX_IN) */
    bool play_down;                       /* (0.8.38) PLAY down while playing: a tap stops, held FADE */
    int play_ms;
    bool glo_down;                        /* GLO down: a tap turns GLO's pages (on letting go), held STYLE MIX */
    int glo_ms;
    int live_start;                       /* LIVE: the START (SA_START_KICK PAD EMPTY), the BUILD (0 BY HAND, 1 8 BARS,
                                           * 2 16 BARS) */
    int live_build;
    sv_song song;                         /* SONG (the glue keeps it) */
    int song_pick;                        /* SONG: the step KNOB 1 picked */
    int song_step;                        /* SONG: the step playing (-1: none; the glue sets it) */
    bool seq_down, seq_fired;            /* SEQ held (SV_NEW_MS): a whole new groove, as at power-on */
    int seq_ms;
    uint8_t hb_down, moves;               /* MOVES: OCT- OCT+ EDIT REC held (a bit each, SV_MV_*); the moves on */
    int hb_ms[4];                         /* .. how long each has been held */
    int style_pick;                       /* the MIXER's STYLE page: the style SEQ makes a groove in */
    int scur;                             /* REC on a synth layer, EDIT: the step the keys write */
    int fill;                             /* EDIT: KNOB 2's FILL (0 CURR: the pattern as it was) */
    sa_code fill_orig;                    /* EDIT: the pattern as it was when EDIT opened (CURR) */
    int hot, hot_ms;                      /* the knob just turned (its card lit); 9: the chip */
    char msg[32];
    int msg_ms;
    int home_ms;
    bool home_down;
    int oct;
    int key_sent[27];
    uint32_t frame;
    int batt;
    int8_t wave[96];                      /* the open synth layer's sound now, a cycle or two (the glue fills it) */
    int wave_n;                           /* how many (0: silent, a flat line) */
    int cpu, cpu_peak, shed;              /* MENU: the audio's load now (its average) and its worst, in %; voices shed */
    bool changed;                         /* a setting changed: the glue applies it (and keeps the look) */
    sv_set_info sets[SV_SLOTS];           /* what each set holds, and LAST (the glue fills it) */
    int set_slot, set_act;                /* SETS: the set picked, the action (SV_SA_*) */
    int req_act, req_slot;                /* REC on SETS: what the glue is to do (req_act -1: nothing) */
    int sback, sback_layer;               /* the page SAVE goes back to */
    uint32_t sig_head, sig_card[4], sig_foot, sig_panel;
    uint32_t bg_gen;                      /* the look the background was last drawn in (+1; 0: never) */
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
/* DRUMS: the drum picked's key; else the notes the open synth layer sounds now, on the keys (bit k: SV_KEY_LO + k; an octave out of the keys' reach
 * folded into it). 0: not on a synth layer */
uint32_t sv_play_keys(const sa_ui *u);
/* (0.7: nothing; kept for the glue) */
uint32_t sv_play_dim(const sa_ui *u);

#endif
