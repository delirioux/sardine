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
       SA_KIT_CLASSIC, SA_KITS };
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
} sa_code;

/* a layer's sound: at once */
typedef struct {
    uint8_t mute;
    uint8_t cut, reso, drive, env;                  /* synth: 0..100 (CUT 100: the sound as it is) */
    uint8_t level;                                  /* drum: 0..100 */
    uint8_t pad[2];
} sa_live;

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
} sa_settings;

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
    bool drop_armed;                                /* REC pressed: the copy goes in on the next bar line */
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
void sa_drop(sa_groove *g);                         /* REC: playing, on the next bar line; stopped, at once */
void sa_play(sa_groove *g, bool on);
void sa_process(sa_groove *g, int frames);
void sa_all_off(sa_groove *g);
void sa_mute(sa_groove *g, int l, bool m);
void sa_key(sa_groove *g, int k, int note, bool down, int layer);   /* a key on the layer (its part), snapped */
int sa_synth_free_part(const sa_groove *g, int except);   /* a part no other synth layer of the copy holds; -1 none */
int sa_synth_count(const sa_groove *g);             /* synth layers in the copy */
bool sa_used(const sa_groove *g, int l);            /* a layer that plays or will (in either copy) */
int sa_snap(const sa_harm *h, int note);
/* a whole bar of one layer's code, for the screen: a bit per sixteenth (bar 0) */
uint16_t sa_preview(const sa_groove *g, const sa_code *c, int layer, int bar);

#endif
