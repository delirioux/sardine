/* SPDX-License-Identifier: GPL-3.0-only */
/* SARDINE's sounds on Felucca's engines, its kits on the DRUM engine, and how a layer sets its part's track: shared by
 * the firmware (sa_fw.c) and the host tests. Needs core.h (track_t, P_*, preset_t, the song) first.
 *
 * A synth layer is one of the sounds below: an engine and its factory preset, a sound of Sardine's own on that engine
 * (Sill's, the same author's), or an FM6 patch (sa_fm6.inc). The list runs BASS, KEYS (stabs and chords), PADS, LEADS,
 * FX. The PIANO is the SAMPLE engine's set 0 (tools/si_piano.py's: six notes of a Kawai grand, VCSL, CC0).
 *
 * A layer's CUTOFF takes the sound's brightness (the engine parameter `bright`) from what the preset has (100) down to
 * nothing (0); MAIN's FILTER does the same to every part at once. RESO is the engine's resonance where it has one
 * (ANALOG, TRIO, NOISE), DRIVE the track's DIST over the sound's own, ENV the filter envelope's amount (50: the
 * preset's).
 *
 * The AMBIENT sounds at the end of the list (pads, dub stabs, a rumbling bass) carry their own stereo spread, reverb
 * and delay; the others take their role's. The room is a big hall.
 *
 * The kits: the DRUM engine's model kits (all synthesised: no samples), each with its own tuning, tone, decay, snap
 * and drive; a drum layer's TUNE, TONE and DECAY move the whole kit around its own values. */
#ifndef SA_SOUNDS_H
#define SA_SOUNDS_H

#include "sa_layer.h"

enum { SAS_ANALOG = 0, SAS_PHASE = 2, SAS_LOFI = 3, SAS_SAMPLE = 4, SAS_VOICE = 5, SAS_TRIO = 6, SAS_WHEEL = 7,
       SAS_GRAIN = 8, SAS_PHYS = 9, SAS_DRUM = 10, SAS_NOISE = 11, SAS_FM6 = 12 };
enum { SAS_KEEP = -1 };
enum { SAS_POLY = 0, SAS_MONO = 1, SAS_LEGATO = 2, SAS_UNISON = 3 };
enum { SAS_R_BASS, SAS_R_KEYS, SAS_R_PAD, SAS_R_LEAD, SAS_R_FX, SAS_ROLES };

typedef struct {
    const char *name;
    uint8_t role, engine, preset;
    int8_t dist, glide, voice;
    const preset_t *own;
    const uint8_t *fm6;
    int8_t bright, reso;                  /* the E parameters CUTOFF and RESO move (-1: none) */
    int8_t trim;                          /* added to the level (half-dB steps): each role's sounds at one loudness,
                                           * measured in the browser build with a pattern of their role */
    uint8_t wide, rev, dly;               /* the stereo spread of its voices, its reverb and delay sends (0: its role's) */
} sas_sound;

/* ---- the own sounds (Felucca's preset layout: {name, E0..E7, ATK DEC SUS REL, ENV->FLT, mono, FX(dist, chorus,
 * delay, reverb)}; ANALOG's E: WAVE DTN MIX NOIS CUT RES DRV KTR). Sill's, as they are ---- */
static const preset_t SAS_SUPER = {"SUPER", {0, 42, 64, 0, 110, 22, 12, 64}, {0, 64, 84, 26}, 12, 0, FX(0, 60, 25, 25)};
static const preset_t SAS_SUPERLEAD = {"SUPER LEAD", {0, 30, 64, 0, 112, 18, 16, 64}, {1, 60, 104, 30}, 8, 1,
                                       FX(0, 50, 35, 30)};
static const preset_t SAS_OCTAVE = {"OCTAVE", {6, 0, 40, 0, 62, 35, 20, 64}, {0, 52, 60, 14}, 30, 1, FX(5, 0, 5, 10)};
static const preset_t SAS_REESE = {"REESE", {0, 22, 64, 0, 58, 20, 15, 64}, {0, 80, 110, 18}, 6, 1, FX(0, 0, 0, 8)};
static const preset_t SAS_HOOVER = {"HOOVER", {4, 38, 64, 0, 96, 30, 20, 64}, {0, 70, 100, 30}, 16, 1, FX(6, 40, 20, 25)};
static const preset_t SAS_DUBCHORD = {"DUB CHORD", {1, 20, 64, 0, 86, 32, 10, 64}, {0, 58, 46, 20}, 22, 0,
                                      FX(0, 40, 30, 30)};
static const preset_t SAS_TRANCESTAB = {"TRANCE", {0, 38, 64, 0, 110, 24, 12, 64}, {0, 58, 30, 18}, 36, 0, FX(0, 50, 40, 30)};
static const preset_t SAS_TECHBASS = {"TECH BASS", {0, 0, 0, 0, 70, 40, 12, 64}, {0, 46, 30, 10}, 34, 1, FX(0, 0, 0, 6)};
static const preset_t SAS_SINEBASS = {"SINE BASS", {3, 0, 0, 0, 127, 0, 25, 0}, {0, 70, 100, 12}, 0, 1, FX(0, 0, 0, 4)};
static const preset_t SAS_WHISTLE = {"WHISTLE", {3, 0, 0, 6, 127, 0, 0, 64}, {6, 70, 110, 25}, 0, 1, FX(0, 20, 40, 45)};
static const preset_t SAS_ROBOT = {"ROBOT", {0, 64, 40, 0, 127, 0, 110, 0}, {2, 70, 110, 30}, 0, 1, FX(0, 15, 30, 25)};

/* ---- the ambient sounds (0.3): wide, slow pads, dub stabs with long tails, a rumbling bass. ANALOG's WAVE: 0 SAW
 * 1 SQR 2 TRI 3 SIN 4 PWM 5 SYNC 6 SUB ---- */
static const preset_t SAS_WIDEPAD = {"WIDE PAD", {0, 38, 64, 0, 72, 12, 0, 32}, {70, 90, 115, 100}, 8, 0, FX(0, 100, 20, 90)};
static const preset_t SAS_DARKPAD = {"DARK PAD", {1, 22, 64, 0, 48, 18, 0, 32}, {90, 90, 115, 105}, 6, 0, FX(0, 80, 10, 90)};
static const preset_t SAS_GLASS = {"GLASS", {2, 14, 64, 0, 100, 8, 0, 48}, {60, 90, 110, 100}, 4, 0, FX(0, 90, 35, 95)};
static const preset_t SAS_VAPOR = {"VAPOR", {0, 30, 64, 40, 64, 20, 0, 32}, {100, 90, 110, 110}, 10, 0, FX(0, 90, 20, 100)};
static const preset_t SAS_SWELL = {"SWELL", {4, 16, 50, 0, 80, 14, 0, 40}, {118, 100, 120, 110}, 20, 0, FX(0, 80, 25, 95)};
static const preset_t SAS_NIGHT = {"NIGHT", {0, 10, 40, 0, 56, 28, 6, 32}, {85, 100, 112, 100}, 24, 0, FX(0, 70, 30, 90)};
static const preset_t SAS_DRONEPAD = {"DRONE PAD", {6, 26, 64, 0, 60, 16, 8, 32}, {90, 100, 120, 115}, 4, 0, FX(0, 90, 10, 100)};
static const preset_t SAS_SKY = {"SKY", {3, 30, 64, 8, 110, 0, 0, 64}, {90, 100, 115, 110}, 0, 0, FX(0, 110, 30, 110)};
static const preset_t SAS_TAPEPAD = {"TAPE PAD", {0, 18, 64, 12, 58, 10, 18, 32}, {70, 90, 110, 95}, 6, 0, FX(0, 60, 10, 80)};
static const preset_t SAS_CHOIRAIR = {"CHOIR AIR", {0, 0, 40, 0, 40, 26, 64, 6}, {60, 90, 112, 90}, 0, 0, FX(0, 90, 15, 100)};
static const preset_t SAS_DUBSTAB = {"DUB STAB", {1, 20, 64, 0, 78, 34, 10, 64}, {0, 58, 40, 24}, 26, 0, FX(0, 50, 90, 70)};
static const preset_t SAS_SPACESTAB = {"SPACE STAB", {0, 30, 64, 0, 96, 22, 8, 64}, {0, 52, 30, 40}, 30, 0, FX(0, 70, 60, 100)};
static const preset_t SAS_ECHOCHORD = {"ECHO CHORD", {4, 14, 64, 0, 84, 30, 0, 64}, {0, 50, 20, 30}, 28, 0, FX(0, 40, 100, 60)};
static const preset_t SAS_RUMBLE = {"RUMBLE", {0, 12, 64, 0, 46, 10, 40, 32}, {0, 80, 100, 40}, 10, 1, FX(0, 0, 0, 70)};
static const preset_t SAS_DEEPSUB = {"DEEP SUB", {3, 0, 0, 0, 127, 0, 0, 0}, {2, 80, 120, 30}, 0, 1, FX(0, 0, 0, 4)};
static const preset_t SAS_AIRLEAD = {"AIR LEAD", {3, 6, 64, 6, 110, 0, 0, 64}, {10, 80, 110, 60}, 0, 1, FX(0, 40, 70, 80)};

#include "sa_fm6.inc"

#define S(n, r, e, p, b, rs, v, tr) {(n), (r), (uint8_t)(e), (uint8_t)(p), SAS_KEEP, SAS_KEEP, (int8_t)(v), 0, 0, \
                                      (int8_t)(b), (int8_t)(rs), (int8_t)(tr)}
#define SG(n, r, e, p, gl, v, b, rs, tr) {(n), (r), (uint8_t)(e), (uint8_t)(p), SAS_KEEP, (int8_t)(gl), (int8_t)(v), 0, 0, \
                                          (int8_t)(b), (int8_t)(rs), (int8_t)(tr)}
#define SO(n, r, e, own, gl, v, b, rs, tr) {(n), (r), (uint8_t)(e), 0, SAS_KEEP, (int8_t)(gl), (int8_t)(v), &(own), 0, \
                                            (int8_t)(b), (int8_t)(rs), (int8_t)(tr)}
/* an ambient sound: its spread, reverb and delay sends its own */
#define SA(n, r, e, own, gl, v, b, rs, tr, w, rv, dl) {(n), (r), (uint8_t)(e), 0, SAS_KEEP, (int8_t)(gl), (int8_t)(v), &(own), \
                                                      0, (int8_t)(b), (int8_t)(rs), (int8_t)(tr), (w), (rv), (dl)}
#define SF(n, r, patch, gl, v, tr) {(n), (r), (uint8_t)SAS_FM6, 0, SAS_KEEP, (int8_t)(gl), (int8_t)(v), 0, (patch), 2, -1, \
                                    (int8_t)(tr)}
#define B SAS_R_BASS
#define K SAS_R_KEYS
#define P SAS_R_PAD
#define L SAS_R_LEAD
#define F SAS_R_FX
/* the synth sounds, in the SOUND chip's order (0 and 1: the first groove's bass and stab, sa_layer.c) */
static const sas_sound SAS_SOUND[] = {
    SO("TECH BASS", B, SAS_ANALOG, SAS_TECHBASS, SAS_KEEP, SAS_MONO, 4, 5, 2),
    SO("DUB CHORD", K, SAS_ANALOG, SAS_DUBCHORD, SAS_KEEP, SAS_POLY, 4, 5, 6),
    /* BASS */
    S("SUB", B, SAS_ANALOG, 7, 4, 5, SAS_MONO, -13),
    S("SQUARE", B, SAS_ANALOG, 2, 4, 5, SAS_MONO, -1),
    SG("ACID", B, SAS_ANALOG, 4, 18, SAS_LEGATO, 4, 5, 6),
    S("FM BASS", B, SAS_FM6, 2, 2, -1, SAS_MONO, -4),
    SO("REESE", B, SAS_ANALOG, SAS_REESE, SAS_KEEP, SAS_MONO, 4, 5, -8),
    SO("OCTAVE", B, SAS_ANALOG, SAS_OCTAVE, SAS_KEEP, SAS_MONO, 4, 5, -1),
    SO("SINE BASS", B, SAS_ANALOG, SAS_SINEBASS, SAS_KEEP, SAS_MONO, -1, -1, -14),
    S("FAT BASS", B, SAS_TRIO, 0, 5, 6, SAS_MONO, 2),
    S("WAVE BASS", B, SAS_LOFI, 1, 6, -1, SAS_MONO, -4),
    SF("SLAP", B, SA_FM6_SLAPBASS, SAS_KEEP, SAS_MONO, 0),
    SF("FRETLESS", B, SA_FM6_FRETLESS, 22, SAS_LEGATO, -5),
    SF("GROWL", B, SA_FM6_GROWL, SAS_KEEP, SAS_MONO, -6),
    SG("WOW BASS", B, SAS_VOICE, 2, 24, SAS_LEGATO, -1, -1, -3),
    SG("ORGAN BASS", B, SAS_WHEEL, 1, SAS_KEEP, SAS_MONO, 3, -1, -4),
    /* KEYS */
    S("PIANO", K, SAS_SAMPLE, 0, 4, -1, SAS_POLY, -10),
    S("ORGAN", K, SAS_WHEEL, 1, 3, -1, SAS_POLY, 0),
    S("FULL ORGAN", K, SAS_WHEEL, 0, 3, -1, SAS_POLY, 0),
    SF("TECH STAB", K, SA_FM6_TECHSTAB, SAS_KEEP, SAS_POLY, 12),
    SO("SUPER", K, SAS_ANALOG, SAS_SUPER, SAS_KEEP, SAS_POLY, 4, 5, -6),
    SO("TRANCE", K, SAS_ANALOG, SAS_TRANCESTAB, SAS_KEEP, SAS_POLY, 4, 5, 6),
    S("PLUCK", K, SAS_ANALOG, 8, 4, 5, SAS_POLY, 4),
    S("E-PIANO", K, SAS_FM6, 0, 2, -1, SAS_POLY, -7),
    S("MARIMBA", K, SAS_FM6, 5, 2, -1, SAS_POLY, 11),
    S("KALIMBA", K, SAS_PHYS, 4, 2, -1, SAS_POLY, 10),
    S("HARP", K, SAS_PHYS, 8, 2, -1, SAS_POLY, 8),
    S("HORNS", K, SAS_PHASE, 0, 2, -1, SAS_POLY, -17),
    SF("ORCH HIT", K, SA_FM6_ORCHHIT, SAS_KEEP, SAS_POLY, 5),
    S("RAVE", K, SAS_ANALOG, 6, 4, 5, SAS_POLY, -5),
    S("SINE KEY", K, SAS_ANALOG, 5, 4, 5, SAS_POLY, -4),
    S("FM PLUCK", K, SAS_FM6, 7, 2, -1, SAS_POLY, 6),
    S("GOSPEL", K, SAS_WHEEL, 2, 3, -1, SAS_POLY, -2),
    /* PADS */
    S("SOFT PAD", P, SAS_ANALOG, 1, 4, 5, SAS_POLY, -11),
    S("STRINGS", P, SAS_ANALOG, 11, 4, 5, SAS_POLY, -11),
    S("PWM STR", P, SAS_ANALOG, 3, 4, 5, SAS_POLY, -9),
    S("FM PAD", P, SAS_FM6, 4, 2, -1, SAS_POLY, 7),
    S("CHOIR", P, SAS_VOICE, 0, -1, -1, SAS_POLY, -14),
    S("CLOUD", P, SAS_GRAIN, 0, 7, -1, SAS_POLY, 10),
    S("SHIMMER", P, SAS_GRAIN, 3, 7, -1, SAS_POLY, 6),
    S("FROZEN", P, SAS_GRAIN, 2, 7, -1, SAS_POLY, 12),
    S("VIOLINS", P, SAS_PHASE, 2, 2, -1, SAS_POLY, -14),
    S("DRONE", P, SAS_PHYS, 7, 2, -1, SAS_POLY, 8),
    S("BOWED", P, SAS_PHYS, 3, 2, -1, SAS_POLY, 1),
    /* LEADS */
    S("SAW LEAD", L, SAS_ANALOG, 0, 4, 5, SAS_MONO, -6),
    SO("SUPER LEAD", L, SAS_ANALOG, SAS_SUPERLEAD, 6, SAS_UNISON, 4, 5, -8),
    SO("HOOVER", L, SAS_ANALOG, SAS_HOOVER, 14, SAS_LEGATO, 4, 5, -4),
    S("SYNC LEAD", L, SAS_TRIO, 2, 5, 6, SAS_MONO, -3),
    S("ARP LEAD", L, SAS_TRIO, 1, 5, 6, SAS_MONO, -2),
    S("PULSE", L, SAS_LOFI, 0, 6, -1, SAS_MONO, -7),
    S("CHIP", L, SAS_LOFI, 2, 6, -1, SAS_MONO, -8),
    SG("VOX LEAD", L, SAS_VOICE, 1, 10, SAS_LEGATO, -1, -1, -22),
    SO("ROBOT", L, SAS_VOICE, SAS_ROBOT, SAS_KEEP, SAS_LEGATO, -1, -1, -24),
    S("FLUTE", L, SAS_WHEEL, 3, 3, -1, SAS_MONO, -3),
    SO("WHISTLE", L, SAS_ANALOG, SAS_WHISTLE, 16, SAS_LEGATO, -1, -1, -19),
    S("BRASS", L, SAS_ANALOG, 9, 4, 5, SAS_MONO, 1),
    S("FM BRASS", L, SAS_FM6, 3, 2, -1, SAS_MONO, 2),
    S("WIRE", L, SAS_PHASE, 5, 2, -1, SAS_MONO, -4),
    S("RESO", L, SAS_PHASE, 3, 2, -1, SAS_MONO, 9),
    /* FX */
    S("WIND", F, SAS_NOISE, 0, 2, 3, SAS_POLY, 0),
    S("RAIN", F, SAS_NOISE, 1, 2, 3, SAS_POLY, 0),
    S("ARCADE", F, SAS_NOISE, 2, 2, 3, SAS_POLY, 0),
    S("METAL", F, SAS_NOISE, 3, 2, 3, SAS_POLY, 0),
    S("GLITCH", F, SAS_GRAIN, 1, 7, -1, SAS_POLY, 0),
    S("WHISPER", F, SAS_VOICE, 3, -1, -1, SAS_POLY, -6),
    /* AMBIENT (0.3, after the others: the saved layers keep their sounds) */
    SA("WIDE PAD", P, SAS_ANALOG, SAS_WIDEPAD, SAS_KEEP, SAS_POLY, 4, 5, -16, 127, 90, 20),
    SA("DARK PAD", P, SAS_ANALOG, SAS_DARKPAD, SAS_KEEP, SAS_POLY, 4, 5, -2, 110, 95, 10),
    SA("GLASS", P, SAS_ANALOG, SAS_GLASS, SAS_KEEP, SAS_POLY, 4, 5, -16, 120, 100, 35),
    SA("VAPOR", P, SAS_ANALOG, SAS_VAPOR, SAS_KEEP, SAS_POLY, 4, 5, -16, 127, 110, 20),
    SA("SWELL", P, SAS_ANALOG, SAS_SWELL, SAS_KEEP, SAS_POLY, 4, 5, 0, 110, 95, 25),
    SA("NIGHT", P, SAS_ANALOG, SAS_NIGHT, SAS_KEEP, SAS_POLY, 4, 5, -11, 100, 90, 30),
    SA("DRONE PAD", P, SAS_ANALOG, SAS_DRONEPAD, SAS_KEEP, SAS_POLY, 4, 5, -13, 127, 105, 10),
    SA("SKY", P, SAS_ANALOG, SAS_SKY, SAS_KEEP, SAS_POLY, 4, 5, -28, 127, 115, 30),
    SA("TAPE PAD", P, SAS_ANALOG, SAS_TAPEPAD, SAS_KEEP, SAS_POLY, 4, 5, -10, 90, 85, 10),
    SA("CHOIR AIR", P, SAS_VOICE, SAS_CHOIRAIR, SAS_KEEP, SAS_POLY, -1, -1, -19, 120, 110, 15),
    SA("DUB STAB", K, SAS_ANALOG, SAS_DUBSTAB, SAS_KEEP, SAS_POLY, 4, 5, 2, 90, 80, 90),
    SA("SPACE STAB", K, SAS_ANALOG, SAS_SPACESTAB, SAS_KEEP, SAS_POLY, 4, 5, 1, 110, 110, 60),
    SA("ECHO CHORD", K, SAS_ANALOG, SAS_ECHOCHORD, SAS_KEEP, SAS_POLY, 4, 5, 13, 80, 60, 110),
    SA("RUMBLE", B, SAS_ANALOG, SAS_RUMBLE, SAS_KEEP, SAS_MONO, 4, 5, -14, 1, 70, 1),
    SA("DEEP SUB", B, SAS_ANALOG, SAS_DEEPSUB, SAS_KEEP, SAS_MONO, -1, -1, -21, 1, 4, 1),
    SA("AIR LEAD", L, SAS_ANALOG, SAS_AIRLEAD, 10, SAS_LEGATO, -1, -1, -16, 60, 80, 70),
};
#undef S
#undef SG
#undef SO
#undef SF
#undef SA
#undef B
#undef K
#undef P
#undef L
#undef F
#define SAS_NSOUNDS ((int)(sizeof SAS_SOUND / sizeof SAS_SOUND[0]))
static const char *const SAS_ROLE_NAMES[SAS_ROLES] = {"BASS", "KEYS", "PAD", "LEAD", "FX"};

static inline const sas_sound *sas_of(int i) { return &SAS_SOUND[(unsigned)i % (unsigned)SAS_NSOUNDS]; }
static inline const char *sas_name(int type, int i)
{
    return type == SA_T_DRUM ? sa_dv_name(i) : sas_of(i)->name;
}

/* ---- the kits: the DRUM engine's {KIT, TUNE, TONE, DECY, SNAP, ACC, KICK, DRV} (eng_drum.c DK_*: STD 0, the model
 * kits 4..8, DEEP 9, TIGHT 10, PERC 11) ---- */
static const uint8_t SAS_KIT[SA_KITS][8] = {
    {5, 60, 72, 48, 72, 100, 0, 26},     /* TECH: the punchy machine kit, a little drive */
    {5, 64, 64, 60, 64, 100, 0, 8},      /* HOUSE */
    {9, 60, 50, 66, 50, 100, 0, 0},      /* DEEP */
    {5, 50, 84, 38, 92, 110, 0, 72},     /* HARD: tuned down, bright, short, driven */
    {4, 58, 60, 92, 60, 100, 1, 10},     /* BOOM: the long round kick */
    {10, 64, 64, 56, 64, 100, 0, 16},    /* TIGHT */
    {11, 64, 64, 64, 64, 100, 0, 0},     /* AFRO: the hand percussion kit */
    {6, 60, 44, 60, 50, 100, 0, 32},     /* DUSTY: the small box, dark and driven */
    {8, 64, 64, 64, 64, 100, 0, 0},      /* CLASSIC */
};
static inline int16_t sas_clamp(int v, int lo, int hi) { return (int16_t)(v < lo ? lo : v > hi ? hi : v); }
/* a knob 0..100 around a kit's own value v (50: v itself) */
static inline int16_t sas_around(int v, int knob) { return sas_clamp(v + (knob - 50) * 127 / 100, 0, 127); }

/* the drum part's track from the kit and the cards (main loop, the audio held off) */
static inline void sas_drums(track_t *t, const sa_settings *s)
{
    const uint8_t *k = SAS_KIT[s->kit % SA_KITS];
    t->p[P_E0] = k[0];
    t->p[P_E1] = sas_around(k[1], s->ktune);
    t->p[P_E2] = sas_clamp(sas_around(k[2], s->ktone) * (40 + s->filter * 60 / 100) / 100, 0, 127);
    t->p[P_E3] = sas_around(k[3], s->kdecay);
    t->p[P_E4] = k[4];
    t->p[P_E5] = k[5];
    t->p[P_E6] = k[6];
    t->p[P_E7] = k[7];
    t->p[P_LEVEL] = 110;
    t->p[P_VOICE] = V_POLY;
    t->p[P_DIST] = 0;
    t->p[P_ITYPE] = 0;
    t->p[P_PAN] = 0;
    t->p[P_REV] = sas_clamp(10 * s->reverb / 70, 0, 127);
    t->p[P_DLY] = 0;
    t->p[P_M2SRC] = MS_OFF;                      /* (the drums never duck) */
}

/* what a synth part's preset left (sa_fw.c keeps it on loading the sound): CUTOFF, ENV and DRIVE move from there */
typedef struct { int16_t bright, fenv, dist; } sas_base;

/* a synth layer's part from its sound and its cards (main loop, the audio held off) */
static inline void sas_synth(track_t *t, const sas_sound *snd, const sas_base *b, const sa_live *v, const sa_settings *s)
{
    t->p[P_LEVEL] = sas_clamp(108 + snd->trim, 1, 127);
    if (snd->bright >= 0) {
        int f = v->cut * (20 + s->filter * 80 / 100) / 100;                      /* 0..100 */
        int base = b->bright < 24 ? 24 : b->bright;
        t->p[P_E0 + snd->bright] = sas_clamp(base * f / 100, 0, 127);
    }
    if (snd->reso >= 0)
        t->p[P_E0 + snd->reso] = sas_clamp(v->reso * 120 / 100, 0, 127);
    /* (ENV>FLT runs -64..63: ENV 50 the preset's own, 0 none, 100 twice it; a preset without one gets up to 40) */
    t->p[P_ED_FLT] = sas_clamp(b->fenv > 0 ? b->fenv * v->env / 50 : v->env * 40 / 100, 0, 63);
    t->p[P_DIST] = sas_clamp(b->dist + v->drive * 70 / 100, 0, 127);
    if (snd->glide >= 0)
        t->p[P_GLIDE] = snd->glide;
    if (snd->voice >= 0)
        t->p[P_VOICE] = (int16_t)(snd->voice == SAS_UNISON ? V_UNISON : snd->voice == SAS_LEGATO ? V_LEGATO
                                  : snd->voice == SAS_MONO ? V_MONO : V_POLY);
    static const uint8_t REV[SAS_ROLES] = {6, 30, 55, 35, 45}, DLY[SAS_ROLES] = {0, 20, 15, 30, 25},
                         WIDE[SAS_ROLES] = {0, 40, 80, 20, 60};
    /* SPACE's REVERB DELAY WIDTH: 70 the sound's own, 0 none, 100 about half as much again */
    t->p[P_REV] = sas_clamp((snd->rev ? snd->rev : REV[snd->role % SAS_ROLES]) * s->reverb / 70, 0, 127);
    t->p[P_DLY] = sas_clamp((snd->dly ? snd->dly : DLY[snd->role % SAS_ROLES]) * s->delay / 70, 0, 127);
    t->p[P_SPRD] = sas_clamp((snd->wide ? snd->wide : WIDE[snd->role % SAS_ROLES]) * s->width / 70, 0, 127);
    /* the pump: the matrix's slot 2, EXPR -> AMP (sas_block sets t->ex_off to the duck: the gain is 1 - duck) */
    t->p[P_M2SRC] = MS_EXPR;
    t->p[P_M2DST] = MD_AMP;
    t->p[P_M2AMT] = 64;
    t->p[P_PAN] = 0;
    t->p[P_ITYPE] = 0;
    if (snd->engine == SAS_SAMPLE) {             /* the piano: its velocity (the SAMPLE engine has none of its own) */
        t->p[P_M4SRC] = MS_VEL;
        t->p[P_M4DST] = MD_AMP;
        t->p[P_M4AMT] = 64;
    } else {
        t->p[P_M4SRC] = MS_OFF;
    }
}

/* the block's pump into the synth parts (the audio ISR, after the layers): a layer with PUMP off stays up */
static inline void sas_block(const sa_groove *g)
{
    uint8_t duck[SA_SYNTHS] = {0, 0, 0};
    for (int l = 0; l < SA_LAYERS; ++l) {
        const sa_code *c = &g->s.code[l];
        if (c->type == SA_T_SYNTH && c->part < SA_SYNTHS && !g->s.live[l].nopump) duck[c->part] = g->duck;
    }
    for (int p = 0; p < SA_SYNTHS; ++p)
        trk[p].ex_off = duck[p];
}

/* the song's own: the tempo for the delay and the LFOs, the delay, the room */
static inline void sas_song(const sa_settings *s)
{
    song.g[G_BPM] = s->bpm;
    song.g[G_DTIME] = 10;                         /* 1/8 dotted (params.c N_DTIME) */
    song.g[G_DFDBK] = 45;
    song.g[G_DMIX] = 100;
    song.g[G_RSIZE] = 92;                        /* (0.3: a big hall for the wide sounds; the drums send little) */
    song.g[G_RDAMP] = 42;
    song.g[G_RTYPE] = 2;                          /* HALL (params.c N_RTYPE) */
}

#endif
