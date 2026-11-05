/* SPDX-License-Identifier: GPL-3.0-only */
/* SARDINE layer engine: live club mixing as a stack of layers, each one a line of "code" the player builds on the
 * device. A layer is a sound and a pattern, bent by up to four modifiers (FAST, SLOW, REV, EVERY, THIN, EUCLID,
 * SHIFT, LEN, OCT, PLY) and, on a synth layer, an arpeggiator. Edits go into a copy of the code (the screen shows it
 * in the second colour) and nothing changes in the music until REC drops the copy in: on the next bar line while
 * playing, at once while stopped. The sound of a layer (CUTOFF RESO DRIVE ENV, a drum layer's LEVEL) and the mutes
 * act at once.
 *
 * Time runs in ticks, 96 to the bar (24 to the beat): a sixteenth is 6, a sixteenth triplet 4, an eighth triplet 8.
 * On every bar line the engine works out the whole bar of every layer (the modifiers act on spans of time, not on
 * single steps) into one list of notes, then plays it tick by tick; FEEL swings the off sixteenths.
 *
 * Parts: up to three synth layers, each on a part (a track) of its own (0..2), so each has its own sound and filter;
 * every drum layer plays on part 3, the kit (the FM-1's engine runs four parts and eight voices).
 *
 * The key and the chords (the KEY page) are code too: a key change drops with the rest. Every synth layer follows the
 * chord of the bar; drums ignore it.
 *
 * No libm, no doubles: floats on the FPU (single precision). */
#ifndef SA_LAYER_H
#define SA_LAYER_H

#include <stdbool.h>
#include <stdint.h>

#define SA_LAYERS 12
#define SA_SYNTHS 3                                 /* synth parts 0..2 */
#define SA_PART_DRUMS 3
#define SA_PARTS 4
#define SA_MODS 4
#define SA_TPB 96                                   /* ticks per bar */
#define SA_TPS 6                                    /* ticks per sixteenth */
#define SA_BAR_EVENTS 192                           /* notes a bar can hold, all layers */

enum { SA_T_NONE, SA_T_DRUM, SA_T_SYNTH };
enum { SA_M_OFF, SA_M_FAST, SA_M_SLOW, SA_M_REV, SA_M_EVERY, SA_M_THIN, SA_M_EUCLID, SA_M_SHIFT, SA_M_LEN, SA_M_OCT,
       SA_M_PLY, SA_M_TYPES };
enum { SA_AR_OFF, SA_AR_UP, SA_AR_DOWN, SA_AR_UPDOWN, SA_AR_DOWNUP, SA_AR_CONVERGE, SA_AR_DIVERGE, SA_AR_RANDOM,
       SA_AR_THUMB, SA_AR_PINKY, SA_AR_STRUM, SA_AR_MODES };
enum { SA_RATE_4, SA_RATE_8, SA_RATE_8T, SA_RATE_16, SA_RATE_16T, SA_RATE_32, SA_RATES };
enum { SA_SC_MINOR, SA_SC_DORIAN, SA_SC_PHRYGIAN, SA_SC_MAJOR, SA_SCALES };
enum { SA_KIT_TECH, SA_KIT_HOUSE, SA_KIT_DEEP, SA_KIT_HARD, SA_KIT_BOOM, SA_KIT_TIGHT, SA_KIT_AFRO, SA_KIT_DUSTY,
       SA_KIT_CLASSIC, SA_KIT_ROOM, SA_KIT_VINTAGE, SA_KIT_MINIMAL, SA_KIT_VOLT, SA_KIT_GARAGE, SA_KIT_DISCO,
       SA_KIT_BREAKS, SA_KIT_DUB, SA_KIT_LOFI, SA_KIT_IRON, SA_KIT_ACID, SA_KITS };   /* (0.7: ROOM .. ACID appended) */
/* the drum voices a drum layer can be */
enum { SA_DV_KICK, SA_DV_SNARE, SA_DV_CLAP, SA_DV_HAT, SA_DV_OPEN, SA_DV_RIDE, SA_DV_CRASH, SA_DV_RIM, SA_DV_TOM,
       SA_DV_LOTOM, SA_DV_CONGA, SA_DV_COWBELL, SA_DV_CLAVE, SA_DV_SHAKER, SA_DVS };
#define SA_PROGS 10                                 /* the chord progressions (sa_layer.c) */
#define SA_CH 9                                     /* a pattern step's tone: the whole chord */

/* a modifier's value: one entry of the flat list ALGORITHM turns through (sa_mod_opt) */
typedef struct { uint8_t type, val; } sa_mod;

/* a layer's code: what REC drops in */
typedef struct {
    uint8_t type;                                   /* SA_T_* */
    uint8_t sound;                                  /* SA_DV_* or a synth sound (sa_sounds.h) */
    uint8_t pat;                                    /* the pattern, in its type's list */
    uint8_t part;                                   /* a synth layer's part, 0..2 (drums: 3) */
    sa_mod mod[SA_MODS];                            /* applied in order; OFF ones are skipped */
    uint8_t amode, arate, arange, agate;            /* the arpeggiator (synth): SA_AR_*, SA_RATE_*, 1..4 octaves, 10..100 % */
    /* the layer's own pattern (own 1): a ready pattern becomes one the first time a step is edited; picking a
     * pattern again drops it. A drum layer: a hit on each sixteenth of `hits`, accented on `acc`. A synth layer: a
     * note on each step with sn != SA_NO_NOTE (semitones above the key's C, MIDI), sl sixteenths long; a step of
     * `chord` plays the bar's chord instead (rooted in octave ooct): it follows the progression */
    uint8_t own, ooct;
    uint16_t hits, acc, chord;
    int8_t sn[16];
    uint8_t sl[16];
} sa_code;
#define SA_NO_NOTE (-128)
#define SA_PAT_OFF 255                              /* a layer's pattern OFF: it plays nothing (a ready pattern's place) */
/* the fixed layers (0.7): LEAD, CHORD and BASS on the synth parts of the same number; the drums in the slots after */
enum { SA_L_LEAD, SA_L_CHORD, SA_L_BASS, SA_L_DRUM0 };
/* the drum rows (0.7): seven, always there, each on a drum channel of its own (none choke another); a row's INSTR one
 * of its channel's drums */
enum { SA_DR_KICK, SA_DR_SNARE, SA_DR_CLAP, SA_DR_HATS, SA_DR_TOMS, SA_DR_RIM, SA_DR_METAL, SA_DROWS };
int sa_dv_row(int dv);                              /* the row a drum voice plays on */
int sa_row_voices(int row, uint8_t *out);           /* a row's drum voices (its INSTR list) */
const char *sa_row_name(int row);
/* a drum row's variation (its code's amode, 0.7): the kit's own drum (0), TIGHT ROOM HIGH LOW (it, reshaped), or another
 * drum machine's (80 10 DEEP); the glue sets the drum engine's lane by it */
#define SA_DVARS 8
const char *sa_dvar_name(int row, int v);           /* "", "TIGHT" .. (CLAP: its own eight, all short) */

/* a layer's sound: at once */
typedef struct {
    uint8_t mute;
    uint8_t cut, reso, drive, env;                  /* synth: 0..100 (CUT 100: the sound as it is) */
    uint8_t level;                                  /* 0..100 (80: as it is; MIX's LEVEL, a drum's LEVEL) */
    uint8_t nopump;                                 /* synth: 1 = this layer does not duck under the kick (0.4) */
    uint8_t pad;
} sa_live;

/* a drum layer's ACCENT and CHANCE (0.7) live in its sa_live's env (0..100, 50: the pattern's own accents) and cut
 * (0..100 %: how likely each hit plays) */

/* a synth layer's gate (0.5): its volume cut to a rhythm on the sixteenths, the note held (no new attack). Code: it
 * drops with REC. pat 0: off; own: the steps of `mask` (edited on the keys), else the ready gate pat's */
typedef struct {
    uint8_t pat, own;
    uint8_t depth, width, smooth;                   /* 0..100: how far it closes; 10..100 % of a step open; the fade */
    uint8_t pad;
    uint16_t mask;
} sa_gate;
#define SA_GATES 13                                 /* OFF and twelve ready gates (sa_layer.c) */

/* a synth layer's panning (0.5): at once, as its sound. FIXED at PAN; SWEEP from side to side over RATE; PING-PONG
 * each note on the other side; RANDOM each note somewhere in WIDTH around PAN */
enum { SA_PAN_FIXED, SA_PAN_SWEEP, SA_PAN_PINGPONG, SA_PAN_RANDOM, SA_PAN_MODES };
enum { SA_PRATE_QUARTER, SA_PRATE_HALF, SA_PRATE_BAR, SA_PRATE_2BARS, SA_PRATE_4BARS, SA_PRATES };
typedef struct {
    uint8_t mode;
    int8_t pos;                                     /* -50 (left) .. 50 (right) */
    uint8_t width, rate;                            /* 0..100 how far it moves; SA_PRATE_* (SWEEP) */
} sa_pan;

/* a synth layer's place around you (0.7, headphones): ANGLE (az, 5-degree steps: 0 in front, 18 right, 36 behind, 54
 * left), HEIGHT (-50 below .. 50 above), DISTANCE (0 close .. 100 far). At once. On, the angle pans the layer (the PAN
 * page rests); behind and below darker, above brighter, far quieter, darker and wetter. MODE (on): FIXED where ANGLE
 * says; ORBIT round you over RATE (d3rate, SA_PRATE_*), from ANGLE; RANDOM each note somewhere round you; FLY an orbit
 * rising and falling about HEIGHT */
enum { SA_3D_OFF, SA_3D_FIXED, SA_3D_ORBIT, SA_3D_RANDOM, SA_3D_FLY, SA_3D_MODES };
typedef struct { uint8_t on, az; int8_t el; uint8_t dist; } sa_3d;   /* on: the MODE (SA_3D_*; 0.7's first 1: FIXED) */
#define SA_AZ_STEPS 72
int sa_sin5(int k);                                 /* sin(k * 5 degrees) * 127 */

/* a synth layer's pump (0.7): DEPTH (0..100 % of HOME's PUMP), RELEASE (0 tight .. 100 breathing; 50: back over 0.7 of
 * a beat), SOURCE (the kick layer's hits, or every beat), ON. At once. on 2: not set yet, the glue sets DEPTH by the
 * sound's type (sas_fix) */
enum { SA_PS_KICK, SA_PS_BEAT, SA_PSRCS };
typedef struct { uint8_t depth, rel, src, on; } sa_pumpl;

/* the key and the chords: code too */
typedef struct {
    uint8_t key, scale, prog, bars;                 /* 0..11 (C..B), SA_SC_*, the progression, 1 2 4 bars a chord */
} sa_harm;

/* what is saved (sa_fw.c): append only */
typedef struct {
    uint8_t bpm, filter, feel, kit;                 /* MAIN: 60..200, 0..100 (100 open), 0..60 % swing, SA_KIT_* */
    uint8_t ktune, ktone, kdecay, kdrive;           /* the kit's (a drum layer's cards) */
    uint8_t palette, look;
    uint8_t pad[2];
    sa_harm harm;
    sa_code code[SA_LAYERS];
    sa_live live[SA_LAYERS];
    /* 0.4, SPACE (MAIN's second page): PUMP 0..100 how deep the synth layers duck under the kick; REVERB, DELAY,
     * WIDTH 0..100, 70 the sounds' own sends and spread */
    uint8_t pump, reverb, delay, width;
    sa_gate gate[SA_LAYERS];                        /* 0.5 */
    sa_pan pan[SA_LAYERS];                          /* 0.5 */
    sa_3d d3[SA_SYNTHS];                            /* 0.7: LEAD CHORD BASS */
    sa_pumpl pl[SA_SYNTHS];                         /* 0.7 */
    uint8_t d3rate[SA_SYNTHS];                      /* 0.7: 3D's RATE (ORBIT, FLY) */
    uint8_t dvol, dmute;                            /* 0.7: the MIXER's DRUMS, all the drums (80: as it is) */
    uint8_t mreso;                                  /* 0.7: HOME's RESO, on the whole mix's filter (each synth layer's) */
    uint8_t style, intro, arc;                      /* 0.7: the STYLE last dropped; INTRO 0 OFF, 1..3 4 8 16 bars; ARC 0 OFF,
                                                     * 1..3 a cycle of 16 32 64 bars */
    /* 0.7, songs: each layer's B pattern (0 as A, SA_PATB_OFF OFF, else the pattern + 1) and B's progression (0 as A,
     * else the progression + 1): PART (HOME's ALGORITHM) A or B, on the next phrase; A/B (HOME, FX) every 8 or 16
     * bars by itself (0 OFF, 1 8, 2 16). PHRASE (0 ON, 1 OFF): the synth layers over four bars, A A A' B (bar 3 a
     * step up in its second half, bar 4 an answer on the root and space at its end) */
    uint8_t patb[SA_LAYERS];
    uint8_t progb, phrase, abauto;
} sa_settings;
#define SA_PATB_OFF 255
#define SA_ABAUTOS 3

typedef struct {
    uint8_t on, part, note, vel;                    /* part 3: note is a General MIDI drum note */
} sa_event;

/* a note of the bar: worked out on the bar line */
typedef struct { uint8_t tick, dur, note, vel, layer, pad; } sa_bar_note;

#define SA_HELD 32
#define SA_PEND 48

typedef struct {
    sa_settings s;                                  /* what plays */
    sa_harm ed_harm;                                /* the edit copy: the screen edits these */
    sa_code ed[SA_LAYERS];
    sa_gate ed_gate[SA_LAYERS];
    bool drop_armed;                                /* REC pressed: the copy goes in on the next bar line */
    const sa_settings *set_next;                    /* a whole groove armed (a set, a fresh one): in on the next bar line */
    float sr;
    bool playing;
    uint32_t clock;                                 /* frames since PLAY */
    float wait;                                     /* frames until the next tick */
    int tick;                                       /* the tick sounding in its bar (0..95; -1 before the first) */
    int bar;                                        /* bars since PLAY */
    sa_bar_note bn[SA_BAR_EVENTS];
    int nbn, bn_at;                                 /* the bar's notes, sorted by tick; the next to play */
    uint32_t arp_i[SA_LAYERS];                      /* each layer's arpeggiator, where it is */
    uint16_t hits[SA_LAYERS], hits_last[SA_LAYERS]; /* what each layer played, a bit per sixteenth (the screen) */
    uint8_t chord_pc[4];                            /* the bar's chord as pitch classes (root third fifth seventh) */
    uint8_t chord_deg;                              /* its degree (0..6) */
    char chord_name[8];
    struct { uint8_t note, part; uint32_t off; } held[SA_HELD];
    int nheld;
    struct { uint32_t at; uint8_t part, note, vel, layer; uint32_t len; } pend[SA_PEND];
    int npend;
    uint32_t keys;                                  /* the FM-1's keys held */
    int key_note[27], key_part[27];
    uint8_t sound_sig;                              /* bumped when the code changed (the glue loads the sounds) */
    uint32_t kick_clock;                            /* the last kick a layer played (the pump) */
    bool kicked;
    uint8_t duck[SA_SYNTHS];                        /* 0..127: how far each synth part is ducked now (each block) */
    float gate_gain[SA_SYNTHS];                     /* each synth part's gate, 0..1, as it fades */
    uint8_t gate_off[SA_SYNTHS];                    /* 0..127: how far each synth part's gate has it down now */
    int8_t pan_now[SA_SYNTHS];                      /* each synth part's pan now, -64..63 (the glue puts it in) */
    bool fill_held;                                 /* ENV held: a drum fill from the next beat, as long as it is held */
    uint8_t fill_on, fill_kind, fill_step;          /* playing it; which (ROLL TOMS BUILD STUTTER FLAM SYNCO CLAPS GHOST,
                                                     * by turns); the step of its bar */
    uint8_t crash_next;                             /* a crash on the next beat (a fill over) */
    bool brk_held, brk_on;                          /* ARP held: BREAK, the drums and the bass out; back on the next 1 */
    uint8_t brk_bars;                               /* the bar lines BREAK has been held over */
    uint8_t back_on, back_bar;                      /* after a long BREAK (4 bars or more): the drums back one by one
                                                     * over two bars (back_bar: which of the two) */
    uint8_t solo;                                   /* the MIXER's SOLO: bit 0 LEAD 1 CHORD 2 BASS 3 DRUMS; none: all */
    bool sweep_held;                                /* LFO held: SWEEP, the mix's filter closing over two bars */
    float sweep;                                    /* 0..1: how far the filter is closed now (SWEEP held, INTRO, ARC) */
    float sweep_man;                                /* SWEEP held: its own (let go: 0) */
    float arr_sw0, arr_sw1;                         /* INTRO, ARC: the sweep over this bar, from .. to */
    uint16_t arr_out;                               /* INTRO: the layers not in yet (a bit per slot) */
    bool arr_brk, arr_fill;                         /* ARC: its break (drums and bass out), its fill (the last half bar) */
    uint8_t az_now[SA_SYNTHS];                      /* 3D: each synth part's angle and height now (moving: ORBIT RANDOM FLY) */
    int8_t el_now[SA_SYNTHS];
    uint8_t pan_side[SA_LAYERS];                    /* PING-PONG: the side the next note goes to */
    uint32_t pan_rnd;
    bool part_b, part_cue;                          /* PART B playing; the PART wanted (in on the next phrase) */
    uint8_t ed_patb[SA_LAYERS], ed_progb;           /* the edit copy of the B patterns and B's progression (SEQ drops) */
    char msg[24];
    uint8_t msg_new;
    void (*sound)(void *ctx, const sa_event *e);
    void *sound_ctx;
} sa_groove;

/* ---- the lists ---- */
int sa_drum_pats(void);
int sa_synth_pats(void);
const char *sa_pat_name(int type, int i);
const char *sa_dv_name(int dv);
uint8_t sa_dv_gm(int dv);                           /* the General MIDI note a drum voice plays */
int sa_mod_opts(int type);                          /* the modifier list for a layer type (OCT on synths only) */
sa_mod sa_mod_opt(int type, int i);
int sa_mod_index(int type, sa_mod m);
void sa_mod_name(sa_mod m, char *out, int max);     /* "FAST 2", "EVERY 4: REV", "OFF" */
const char *sa_arp_name(int mode);
const char *sa_rate_name(int rate);
const char *sa_scale_name(int sc);
const char *sa_note_name(int pc);
void sa_prog_name(int prog, char *out, int max);    /* "i-VI" */
const char *sa_kit_name(int kit);

/* ---- the engine ---- */
void sa_defaults(sa_settings *s);                   /* the first groove: kick, hats, clap, bass, stab, in TECH */
void sa_settings_fix(sa_settings *s);               /* every field into its range (a loaded record) */
void sa_init(sa_groove *g, float sample_rate);
void sa_set_sound(sa_groove *g, void (*fn)(void *ctx, const sa_event *e), void *ctx);
void sa_edit_reset(sa_groove *g);                   /* the edit copy = what plays */
int sa_pending(const sa_groove *g);                 /* how many changes wait for REC (layers, plus 1 for the key) */
bool sa_layer_pending(const sa_groove *g, int l);
void sa_drop(sa_groove *g);                         /* SEQ: playing, on the next bar line; stopped, at once */
void sa_sound_now(sa_groove *g, int l, int sound);  /* a synth layer's sound, at once (in the copy too) */
void sa_fill(sa_groove *g, bool down);              /* the drum fill button: held from the next beat, a fill in place of
                                                     * the drums; let go: the groove again */
const char *sa_fill_name(int kind);
void sa_break(sa_groove *g, bool down);             /* BREAK: held, the drums and the bass out at once; let go: in on the 1 */
void sa_sweep(sa_groove *g, bool down);
void sa_part(sa_groove *g, bool b);                 /* PART A or B: playing, on the next phrase (4 bars); stopped, at once */
int sa_ab_bars(int i);                              /* A/B by itself: 0 OFF, 8, 16 */
#define SA_INTROS 4                                 /* INTRO: OFF, 4, 8, 16 bars */
#define SA_ARCS 4                                   /* ARC: OFF, 16, 32, 64 bars */
int sa_intro_bars(int i);
int sa_arc_bars(int i);             /* SWEEP: held, the filter closes over two bars; let go: open at once */
/* a whole groove in (a set loaded, a fresh one): playing, on the next bar line; stopped, at once. What waits for REC
 * goes, the colour and the look stay. s is read on the bar line: the caller keeps it until then */
void sa_set_arm(sa_groove *g, const sa_settings *s);
/* a fresh groove from a seed: a kick, hats, a clap or snare and maybe more, a bass, stabs or a pad, now and then a
 * lead, a key, scale, progression, kit, tempo and swing that suit a club. The sounds come from the glue's lists */
typedef struct { const uint8_t *bass, *keys, *pad, *lead; uint8_t nbass, nkeys, npad, nlead; } sa_pool;
void sa_random(sa_settings *s, uint32_t seed, const sa_pool *p);
void sa_make_b(sa_settings *s, uint32_t seed);      /* PART B from A: another line of the same kind on each synth layer,
                                                     * busier hats, another progression */
void sa_play(sa_groove *g, bool on);
void sa_process(sa_groove *g, int frames);
void sa_all_off(sa_groove *g);
void sa_mute(sa_groove *g, int l, bool m);
void sa_key(sa_groove *g, int k, int note, bool down, int layer);   /* a key on the layer (its part), snapped */
void sa_key_drum(sa_groove *g, int k, int dv, bool down);          /* a key playing a drum voice (SA_DV_*) */
int sa_synth_free_part(const sa_groove *g, int except);   /* a part no other synth layer of the copy holds; -1 none */
int sa_synth_count(const sa_groove *g);             /* synth layers in the copy */
bool sa_used(const sa_groove *g, int l);            /* a layer that plays or will (in either copy) */
int sa_snap(const sa_harm *h, int note);
int sa_degree(const sa_harm *h, int deg);          /* a scale degree (0 the root, 7 the octave up) in semitones */
/* a whole bar of one layer's code, for the screen: a bit per sixteenth (bar 0) */
uint16_t sa_preview(const sa_groove *g, const sa_code *c, int layer, int bar);
/* the own pattern (in the copy, the edits wait for REC): made from the ready pattern on the first edit */
void sa_own_make(sa_groove *g, sa_code *c);
bool sa_chordy(const sa_code *c);                    /* a synth layer whose steps are chords (a key toggles a chord) */
void sa_step_drum(sa_groove *g, int layer, int step);           /* a drum hit on or off */
void sa_step_note(sa_groove *g, int layer, int step, int note); /* a note (snapped) on, the same again off; a chord
                                                                 * layer: the chord on or off */
void sa_step_len(sa_groove *g, int layer, int step, int d);
void sa_pat_label(const sa_code *c, char *out, int max);       /* "OFFBEAT", or "OFFBEAT EDIT" for an own one */
const char *sa_pan_mode_name(int m);
const char *sa_pan_rate_name(int r);
/* the gates */
const char *sa_gate_name(int pat);
uint16_t sa_gate_mask(const sa_gate *gt);           /* its steps */
bool sa_gate_on(const sa_gate *gt);
void sa_gate_label(const sa_gate *gt, char *out, int max);     /* "TRANCE A", "TRANCE A EDIT", "MY GATE", "OFF" */
void sa_gate_step(sa_groove *g, int layer, int step);          /* a gate step on or off (the copy) */
void sa_gate_pick(sa_groove *g, int layer, int d);             /* the next ready gate (the copy) */

#endif
