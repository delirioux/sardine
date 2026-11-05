/* SPDX-License-Identifier: GPL-3.0-only */
/* SARDINE layer engine (sa_layer.h): the patterns, the modifiers as transforms of time spans, the arpeggiator, the
 * chords, and the bar's notes played in time. */
#include "sa_layer.h"

#if TUNA_FREESTANDING                             /* the FM-1 has no C library: Felucca's libc.c has these */
#include <stddef.h>
void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
int memcmp(const void *a, const void *b, size_t n);
#else
#include <string.h>
#endif

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static int32_t fdiv(int32_t a, int32_t b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }   /* floor, b > 0 */
static uint32_t hash2(uint32_t a, uint32_t b)
{
    uint32_t h = a * 2654435761u ^ (b + 0x9E3779B9u + (a << 6) + (a >> 2));
    h ^= h >> 15;
    h *= 2246822519u;
    h ^= h >> 13;
    return h;
}
static void cpy(char *d, const char *s, int n)
{
    int i = 0;
    for (; s[i] && i + 1 < n; ++i) d[i] = s[i];
    if (n > 0) d[i] = 0;
}
static void cat(char *d, const char *s, int n)
{
    int k = 0;
    while (d[k]) ++k;
    cpy(d + k, s, n - k);
}
static void fmt_int(char *d, int v)
{
    char b[12];
    int k = 0, neg = v < 0;
    unsigned u = neg ? (unsigned)-v : (unsigned)v;
    do { b[k++] = (char)('0' + u % 10); u /= 10; } while (u && k < 11);
    int j = 0;
    if (neg) d[j++] = '-';
    while (k) d[j++] = b[--k];
    d[j] = 0;
}

/* ---- names ---------------------------------------------------------------------------------------------------- */
static const char *const NOTE_NAMES[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
const char *sa_note_name(int pc) { return NOTE_NAMES[((pc % 12) + 12) % 12]; }
static const char *const SCALE_NAMES[SA_SCALES] = {"MINOR", "DORIAN", "PHRYGIAN", "MAJOR"};
const char *sa_scale_name(int sc) { return SCALE_NAMES[clampi(sc, 0, SA_SCALES - 1)]; }
static const uint8_t SCALE[SA_SCALES][7] = {
    {0, 2, 3, 5, 7, 8, 10}, {0, 2, 3, 5, 7, 9, 10}, {0, 1, 3, 5, 7, 8, 10}, {0, 2, 4, 5, 7, 9, 11}};
static const char *const KIT_NAMES[SA_KITS] = {"TECH", "HOUSE", "DEEP", "HARD", "BOOM", "TIGHT", "AFRO", "DUSTY",
                                               "CLASSIC", "ROOM", "VINTAGE", "MINIMAL", "VOLT", "GARAGE", "DISCO",
                                               "BREAKS", "DUB", "LOFI", "IRON", "ACID"};
const char *sa_kit_name(int kit) { return KIT_NAMES[clampi(kit, 0, SA_KITS - 1)]; }
static const char *const DV_NAMES[SA_DVS] = {"KICK", "SNARE", "CLAP", "HAT", "OPEN HAT", "RIDE", "CRASH", "RIM",
                                             "TOM", "LOW TOM", "CONGA", "COWBELL", "CLAVE", "SHAKER"};
static const uint8_t DV_GM[SA_DVS] = {36, 38, 39, 42, 46, 51, 49, 37, 45, 41, 63, 56, 75, 70};
const char *sa_dv_name(int dv) { return DV_NAMES[clampi(dv, 0, SA_DVS - 1)]; }
/* KICK SNARE CLAP HAT OPEN HAT RIDE CRASH RIM TOM LOW TOM CONGA COWBELL CLAVE SHAKER -> their rows (the drum engine's
 * channels: the shaker is a closed hat, the conga a tom, the clave a rim, ride crash and cowbell its bell) */
static const uint8_t DV_ROW[SA_DVS] = {SA_DR_KICK, SA_DR_SNARE, SA_DR_CLAP, SA_DR_HATS, SA_DR_HATS, SA_DR_METAL,
                                       SA_DR_METAL, SA_DR_RIM, SA_DR_TOMS, SA_DR_TOMS, SA_DR_TOMS, SA_DR_METAL,
                                       SA_DR_RIM, SA_DR_HATS};
static const uint8_t ROW_DEF[SA_DROWS] = {SA_DV_KICK, SA_DV_SNARE, SA_DV_CLAP, SA_DV_HAT, SA_DV_TOM, SA_DV_RIM,
                                          SA_DV_RIDE};
static const char *const ROW_NAMES[SA_DROWS] = {"KICK", "SNARE", "CLAP", "HATS", "TOMS", "RIM", "METAL"};
int sa_dv_row(int dv) { return DV_ROW[clampi(dv, 0, SA_DVS - 1)]; }
static const char *const DVAR_NAMES[SA_DVARS] = {"", "TIGHT", "ROOM", "HIGH", "LOW", "80", "10", "DEEP"};
const char *sa_dvar_name(int v) { return DVAR_NAMES[clampi(v, 0, SA_DVARS - 1)]; }
const char *sa_row_name(int row) { return ROW_NAMES[clampi(row, 0, SA_DROWS - 1)]; }
int sa_row_voices(int row, uint8_t *out)
{
    int n = 0;
    for (int v = 0; v < SA_DVS; ++v)
        if (DV_ROW[v] == row) out[n++] = (uint8_t)v;
    return n;
}
uint8_t sa_dv_gm(int dv) { return DV_GM[clampi(dv, 0, SA_DVS - 1)]; }
static const char *const ARP_NAMES[SA_AR_MODES] = {"OFF", "UP", "DOWN", "UP-DOWN", "DOWN-UP", "CONVERGE", "DIVERGE",
                                                   "RANDOM", "THUMB", "PINKY", "STRUM"};
const char *sa_arp_name(int m) { return ARP_NAMES[clampi(m, 0, SA_AR_MODES - 1)]; }
static const char *const RATE_NAMES[SA_RATES] = {"1/4", "1/8", "1/8 T", "1/16", "1/16 T", "1/32"};
static const uint8_t RATE_TICKS[SA_RATES] = {24, 12, 8, 6, 4, 3};
const char *sa_rate_name(int r) { return RATE_NAMES[clampi(r, 0, SA_RATES - 1)]; }

/* ---- the chord progressions: scale degrees (0 = the key's chord), each lasting BARS ---- */
static const struct { uint8_t n, deg[4]; } PROG[SA_PROGS] = {
    {1, {0}},          /* ONE CHORD */
    {2, {0, 5}},       /* i  VI */
    {2, {0, 3}},       /* i  iv */
    {2, {0, 6}},       /* i  VII */
    {4, {0, 6, 5, 6}}, /* i  VII  VI  VII */
    {4, {0, 4, 5, 3}}, /* i  v  VI  iv */
    {4, {0, 2, 6, 5}}, /* i  III  VII  VI */
    {2, {0, 1}},       /* i  ii */
    {4, {5, 6, 0, 0}}, /* VI  VII  i  i */
    {4, {0, 3, 6, 2}}, /* i  iv  VII  III */
};
static const char *const ROMAN[7] = {"I", "II", "III", "IV", "V", "VI", "VII"};
static bool deg_minor(int sc, int d)
{
    int r = SCALE[sc][d % 7], t = SCALE[sc][(d + 2) % 7];
    return ((t - r + 12) % 12) == 3;
}
void sa_prog_name(int p, char *out, int max)
{
    p = clampi(p, 0, SA_PROGS - 1);
    out[0] = 0;
    if (PROG[p].n == 1) {
        cpy(out, "ONE CHORD", max);
        return;
    }
    for (int i = 0; i < PROG[p].n; ++i) {
        int d = PROG[p].deg[i];
        char r[8];
        cpy(r, ROMAN[d], sizeof r);
        if (deg_minor(SA_SC_MINOR, d))             /* minor chords in small letters (named as in the minor key) */
            for (char *c = r; *c; ++c) *c = (char)(*c - 'A' + 'a');
        if (i) cat(out, "-", max);
        cat(out, r, max);
    }
}

/* ---- the patterns ---------------------------------------------------------------------------------------------- */
/* a drum hit: its tick in the bar, its velocity */
typedef struct { uint8_t t, v; } dstep;
typedef struct { const char *name; uint8_t n; dstep s[24]; } dpat;
#define X(i) {(uint8_t)((i) * SA_TPS), 110}
#define A(i) {(uint8_t)((i) * SA_TPS), 127}
#define G(i) {(uint8_t)((i) * SA_TPS), 70}
#define T8(i) {(uint8_t)((i) * 8), 110}
static const dpat DPAT[] = {
    {"4 FLOOR", 4, {A(0), X(4), A(8), X(12)}},
    {"OFFBEAT", 4, {X(2), X(6), X(10), X(14)}},
    {"2 + 4", 2, {A(4), A(12)}},
    {"8THS", 8, {A(0), G(2), X(4), G(2 + 4), A(8), G(10), X(12), G(14)}},
    {"16THS", 16, {A(0), G(1), X(2), G(3), A(4), G(5), X(6), G(7), A(8), G(9), X(10), G(11), A(12), G(13), X(14),
                   G(15)}},
    {"SKIP", 8, {X(2), G(3), X(6), G(7), X(10), G(11), X(14), G(15)}},
    {"GALLOP", 12, {A(0), G(2), X(3), A(4), G(6), X(7), A(8), G(10), X(11), A(12), G(14), X(15)}},
    {"PUSH", 5, {A(0), X(4), A(8), X(12), G(14)}},
    {"2-STEP", 2, {A(0), X(10)}},
    {"BROKEN", 3, {A(0), X(7), X(10)}},
    {"CLAVE", 5, {X(0), X(3), X(6), X(10), X(12)}},
    {"TRESILLO", 6, {A(0), X(3), X(6), A(8), X(11), X(14)}},
    {"ROLL END", 7, {X(0), X(4), X(8), G(12), X(13), A(14), A(15)}},
    {"TRIPLET", 12, {T8(0), T8(1), T8(2), T8(3), T8(4), T8(5), T8(6), T8(7), T8(8), T8(9), T8(10), T8(11)}},
    {"SHUFFLE", 8, {T8(0), T8(2), T8(3), T8(5), T8(6), T8(8), T8(9), T8(11)}},
    {"BELL 12", 7, {T8(0), T8(2), T8(4), T8(5), T8(7), T8(9), T8(11)}},
    {"SPARSE", 2, {X(0), G(11)}},
    {"SYNCO", 4, {X(3), X(6), X(11), X(14)}},
    {"HALF", 2, {A(0), X(8)}},
    {"ONE", 1, {A(0)}},
    {"BREAK", 5, {A(0), X(4), X(10), A(12), G(15)}},
    {"LAST BEAT", 4, {X(12), X(13), A(14), A(15)}},
    {"SHAKE", 16, {G(0), G(1), X(2), G(3), G(4), G(5), X(6), G(7), G(8), G(9), X(10), G(11), G(12), G(13), X(14),
                   G(15)}},
    {"UPBEATS", 6, {X(1), X(5), X(7), X(9), X(13), X(15)}},
};
#undef X
#undef A
#undef G
#undef T8
#define NDPAT ((int)(sizeof DPAT / sizeof DPAT[0]))

/* a synth step: tick, length (ticks), tone (0 root 1 third 2 fifth 3 seventh 4 the root an octave up, SA_CH the
 * chord), octave offset, velocity */
typedef struct { uint8_t t, d, tone; int8_t oct; uint8_t v; } sstep;
typedef struct { const char *name; int8_t oct; uint8_t n; sstep s[16]; } spat;
#define N(i, len, tone, o, v) {(uint8_t)((i) * SA_TPS), (uint8_t)((len) * SA_TPS / 2), (tone), (o), (v)}
/* (len in eighth-sixteenths: 2 = one sixteenth) */
static const spat SPAT[] = {
    /* bass lines (octave 2) */
    {"OFFBEAT", 2, 4, {N(2, 3, 0, 0, 110), N(6, 3, 0, 0, 100), N(10, 3, 0, 0, 110), N(14, 3, 0, 0, 100)}},
    {"ROLLER", 2, 8, {N(2, 1, 0, 0, 110), N(3, 1, 0, 0, 80), N(6, 1, 0, 0, 110), N(7, 1, 4, 0, 80),
                      N(10, 1, 0, 0, 110), N(11, 1, 0, 0, 80), N(14, 1, 0, 0, 110), N(15, 1, 2, 0, 80)}},
    {"ROOT 8THS", 2, 8, {N(0, 2, 0, 0, 115), N(2, 2, 0, 0, 90), N(4, 2, 0, 0, 105), N(6, 2, 0, 0, 90), N(8, 2, 0, 0, 115),
                         N(10, 2, 0, 0, 90), N(12, 2, 0, 0, 105), N(14, 2, 0, 0, 90)}},
    {"OCTAVE", 2, 8, {N(0, 2, 0, 0, 115), N(2, 2, 4, 0, 95), N(4, 2, 0, 0, 105), N(6, 2, 4, 0, 95), N(8, 2, 0, 0, 115),
                      N(10, 2, 4, 0, 95), N(12, 2, 0, 0, 105), N(14, 2, 4, 0, 95)}},
    {"PUMP 16", 2, 16, {N(0, 1, 0, 0, 120), N(1, 1, 0, 0, 70), N(2, 1, 0, 0, 95), N(3, 1, 0, 0, 70), N(4, 1, 0, 0, 115),
                        N(5, 1, 0, 0, 70), N(6, 1, 0, 0, 95), N(7, 1, 0, 0, 70), N(8, 1, 0, 0, 120), N(9, 1, 0, 0, 70),
                        N(10, 1, 0, 0, 95), N(11, 1, 0, 0, 70), N(12, 1, 0, 0, 115), N(13, 1, 0, 0, 70),
                        N(14, 1, 0, 0, 95), N(15, 1, 0, 0, 70)}},
    {"SYNCO", 2, 5, {N(0, 3, 0, 0, 115), N(3, 2, 0, 0, 100), N(6, 2, 2, 0, 100), N(10, 3, 0, 0, 110),
                     N(13, 2, 4, 0, 95)}},
    {"SUB HOLD", 2, 1, {N(0, 32, 0, 0, 110)}},
    {"HALF", 2, 2, {N(0, 15, 0, 0, 115), N(8, 15, 2, 0, 105)}},
    {"ACID", 2, 8, {N(0, 2, 0, 0, 120), N(2, 1, 0, 0, 90), N(3, 2, 4, 0, 110), N(6, 2, 2, 0, 100), N(8, 2, 0, 0, 120),
                    N(10, 1, 3, 0, 95), N(11, 2, 0, 0, 100), N(14, 2, 4, 0, 115)}},
    {"GROWL", 2, 5, {N(0, 4, 0, 0, 120), N(4, 4, 0, 0, 110), N(7, 2, 0, 0, 100), N(10, 3, 2, 0, 110),
                     N(12, 7, 0, 0, 115)}},
    {"2-STEP", 2, 4, {N(0, 3, 0, 0, 120), N(6, 2, 0, 0, 100), N(10, 3, 2, 0, 110), N(11, 2, 0, 0, 90)}},
    {"WALK", 2, 4, {N(0, 7, 0, 0, 115), N(4, 7, 1, 0, 100), N(8, 7, 2, 0, 105), N(12, 7, 4, 0, 100)}},
    /* chords (octave 4) */
    {"STAB 1", 4, 1, {N(3, 2, SA_CH, 0, 110)}},
    {"STAB 2", 4, 2, {N(3, 2, SA_CH, 0, 110), N(10, 2, SA_CH, 0, 100)}},
    {"OFFBEATS", 4, 4, {N(2, 2, SA_CH, 0, 105), N(6, 2, SA_CH, 0, 100), N(10, 2, SA_CH, 0, 105),
                        N(14, 2, SA_CH, 0, 100)}},
    {"HOUSE", 4, 5, {N(2, 2, SA_CH, 0, 105), N(5, 2, SA_CH, 0, 95), N(8, 2, SA_CH, 0, 110), N(10, 2, SA_CH, 0, 95),
                     N(13, 2, SA_CH, 0, 100)}},
    {"CHARLESTON", 4, 2, {N(0, 3, SA_CH, 0, 115), N(6, 3, SA_CH, 0, 105)}},
    {"DOWNBEAT", 4, 1, {N(0, 4, SA_CH, 0, 115)}},
    {"PUSH", 4, 2, {N(0, 3, SA_CH, 0, 110), N(14, 3, SA_CH, 0, 100)}},
    {"DOTTED", 4, 5, {N(0, 2, SA_CH, 0, 110), N(3, 2, SA_CH, 0, 100), N(6, 2, SA_CH, 0, 105), N(9, 2, SA_CH, 0, 100),
                      N(12, 2, SA_CH, 0, 105)}},
    /* pads (octave 4) */
    {"WASH", 4, 1, {N(0, 32, SA_CH, 0, 100)}},
    {"TWO HALVES", 4, 2, {N(0, 15, SA_CH, 0, 100), N(8, 15, SA_CH, 0, 95)}},
    {"SWELL", 4, 1, {N(4, 24, SA_CH, 0, 95)}},
    /* leads and arp sources (octave 5) */
    {"TONES UP", 5, 4, {N(0, 7, 0, 0, 105), N(4, 7, 1, 0, 100), N(8, 7, 2, 0, 105), N(12, 7, 4, 0, 100)}},
    {"RIFF", 5, 5, {N(0, 2, 0, 0, 110), N(3, 2, 2, 0, 100), N(6, 2, 4, 0, 105), N(10, 2, 2, 0, 100), N(14, 2, 1, 0, 95)}},
    {"PLUCKS", 5, 8, {N(0, 1, 0, 0, 110), N(2, 1, 2, 0, 90), N(4, 1, 4, 0, 105), N(6, 1, 2, 0, 90), N(8, 1, 1, 0, 105),
                      N(10, 1, 2, 0, 90), N(12, 1, 4, 0, 105), N(14, 1, 2, 0, 90)}},
    {"CALL", 5, 4, {N(0, 4, 2, 0, 110), N(3, 2, 4, 0, 100), N(6, 4, 2, 0, 105), N(12, 6, 1, 0, 100)}},
    {"HOLD NOTE", 5, 1, {N(0, 32, 0, 0, 105)}},
    {"HOLD CHORD", 5, 1, {N(0, 32, SA_CH, 0, 100)}},
};
#undef N
#define NSPAT ((int)(sizeof SPAT / sizeof SPAT[0]))
int sa_drum_pats(void) { return NDPAT; }
int sa_synth_pats(void) { return NSPAT; }
const char *sa_pat_name(int type, int i)
{
    return type == SA_T_DRUM ? DPAT[clampi(i, 0, NDPAT - 1)].name : SPAT[clampi(i, 0, NSPAT - 1)].name;
}

/* ---- the modifiers: one flat list, grouped by type, for ALGORITHM to turn through ---- */
typedef struct { uint8_t type, lo, hi; } mrange;   /* values lo..hi (the meaning per type: below) */
static const mrange MODS[] = {
    {SA_M_OFF, 0, 0},
    {SA_M_FAST, 2, 4},                             /* x2 x3 x4 */
    {SA_M_SLOW, 2, 4},                             /* /2 /3 /4 */
    {SA_M_REV, 0, 0},
    {SA_M_EVERY, 2, 8},                            /* every n bars: reversed (2..8) */
    {SA_M_THIN, 1, 9},                             /* 10 .. 90 % of the notes left out */
    {SA_M_EUCLID, 1, 15},                          /* k hits spread over 16 */
    {SA_M_SHIFT, 0, 15},                           /* by -8 .. +7 sixteenths (val - 8, 0 left out) */
    {SA_M_LEN, 3, 15},                             /* the first n sixteenths, looped */
    {SA_M_OCT, 0, 4},                              /* -2 .. +2 (val - 2, 0 left out), synth layers */
    {SA_M_PLY, 2, 4},                              /* each note n times */
};
#define NMODR ((int)(sizeof MODS / sizeof MODS[0]))
static bool mod_skip(int type, int mt, int v)
{
    if (mt == SA_M_SHIFT && v == 8) return true;
    if (mt == SA_M_OCT && (v == 2 || type != SA_T_SYNTH)) return true;
    return false;
}
int sa_mod_opts(int type)
{
    int n = 0;
    for (int r = 0; r < NMODR; ++r)
        for (int v = MODS[r].lo; v <= MODS[r].hi; ++v)
            n += !mod_skip(type, MODS[r].type, v);
    return n;
}
sa_mod sa_mod_opt(int type, int i)
{
    int n = 0;
    for (int r = 0; r < NMODR; ++r)
        for (int v = MODS[r].lo; v <= MODS[r].hi; ++v) {
            if (mod_skip(type, MODS[r].type, v)) continue;
            if (n++ == i) {
                sa_mod m = {MODS[r].type, (uint8_t)v};
                return m;
            }
        }
    sa_mod off = {SA_M_OFF, 0};
    return off;
}
int sa_mod_index(int type, sa_mod m)
{
    int n = sa_mod_opts(type);
    for (int i = 0; i < n; ++i) {
        sa_mod o = sa_mod_opt(type, i);
        if (o.type == m.type && (o.val == m.val || m.type == SA_M_OFF || m.type == SA_M_REV)) return i;
    }
    return 0;
}
void sa_mod_name(sa_mod m, char *out, int max)
{
    char a[8];
    out[0] = 0;
    switch (m.type) {
    case SA_M_FAST: cpy(out, "FAST ", max); fmt_int(a, m.val); cat(out, a, max); break;
    case SA_M_SLOW: cpy(out, "SLOW ", max); fmt_int(a, m.val); cat(out, a, max); break;
    case SA_M_REV: cpy(out, "REV", max); break;
    case SA_M_EVERY: cpy(out, "EVERY ", max); fmt_int(a, m.val); cat(out, a, max); cat(out, ": REV", max); break;
    case SA_M_THIN: cpy(out, "THIN ", max); fmt_int(a, m.val * 10); cat(out, a, max); cat(out, "%", max); break;
    case SA_M_EUCLID: cpy(out, "EUCLID ", max); fmt_int(a, m.val); cat(out, a, max); cat(out, "/16", max); break;
    case SA_M_SHIFT:
        cpy(out, "SHIFT ", max);
        if (m.val > 8) cat(out, "+", max);
        fmt_int(a, (int)m.val - 8);
        cat(out, a, max);
        break;
    case SA_M_LEN: cpy(out, "LEN ", max); fmt_int(a, m.val); cat(out, a, max); break;
    case SA_M_OCT:
        cpy(out, "OCT ", max);
        if (m.val > 2) cat(out, "+", max);
        fmt_int(a, (int)m.val - 2);
        cat(out, a, max);
        break;
    case SA_M_PLY: cpy(out, "PLY ", max); fmt_int(a, m.val); cat(out, a, max); break;
    default: cpy(out, "OFF", max); break;
    }
}

/* ---- the query: a layer's notes over a span of ticks, the modifiers applied from the last one in (each one asks
 * the ones before it for the span it needs). One set of buffers, one per depth: the audio's bar line and the
 * screen's preview never run at once (the glue holds the audio off for the preview) ---- */
typedef struct { int32_t t; uint8_t d, tone, v; int8_t oct, n; } qev;   /* n: an own note (tone SA_NOTE) */
#define SA_NOTE 10                                 /* a query note's tone: an own pattern's note, n above the key */
#define QMAX 64
static qev QB[SA_MODS + 1][QMAX];
static int QN[SA_MODS + 1];
static qev QT[QMAX];                               /* a transform's inner result, copied out of the deeper buffer */

static void qadd(int k, int32_t t, int d, int tone, int oct, int v, int n)
{
    if (QN[k] >= QMAX) return;
    qev *e = &QB[k][QN[k]++];
    e->t = t;
    e->d = (uint8_t)clampi(d, 1, 255);
    e->tone = (uint8_t)tone;
    e->oct = (int8_t)clampi(oct, -4, 4);
    e->v = (uint8_t)clampi(v, 1, 127);
    e->n = (int8_t)n;
}

/* the active modifiers of a code, in order */
static int active_mods(const sa_code *c, sa_mod *out)
{
    int n = 0;
    for (int i = 0; i < SA_MODS; ++i)
        if (c->mod[i].type != SA_M_OFF && c->mod[i].type < SA_M_TYPES) out[n++] = c->mod[i];
    return n;
}

static void query(const sa_code *c, const sa_mod *m, int k, int layer, int32_t a, int32_t b);

/* the inner (depth k - 1) result over [a, b), copied to QT; its count */
static int inner(const sa_code *c, const sa_mod *m, int k, int layer, int32_t a, int32_t b)
{
    query(c, m, k - 1, layer, a, b);
    int n = QN[k - 1];
    memcpy(QT, QB[k - 1], (size_t)n * sizeof(qev));
    return n;
}

static void query(const sa_code *c, const sa_mod *m, int k, int layer, int32_t a, int32_t b)
{
    QN[k] = 0;
    if (b <= a) return;
    if (k == 0) {                                  /* the pattern itself, a bar long, over and over */
        for (int32_t cy = fdiv(a, SA_TPB); cy <= fdiv(b - 1, SA_TPB); ++cy) {
            if (c->own) {                          /* the layer's own: its sixteenths */
                for (int i = 0; i < 16; ++i) {
                    int32_t t = cy * SA_TPB + i * SA_TPS;
                    if (t < a || t >= b) continue;
                    int v = (c->acc >> i) & 1u ? 124 : 104;
                    if (c->type == SA_T_DRUM) {
                        if ((c->hits >> i) & 1u) qadd(0, t, 3, 0, 0, v, 0);
                    } else if ((c->chord >> i) & 1u) {
                        qadd(0, t, c->sl[i] * SA_TPS, SA_CH, 0, v, 0);
                    } else if (c->sn[i] != SA_NO_NOTE) {
                        qadd(0, t, c->sl[i] * SA_TPS, SA_NOTE, 0, v, c->sn[i]);
                    }
                }
                continue;
            }
            if (c->type == SA_T_DRUM) {
                const dpat *p = &DPAT[c->pat % NDPAT];
                for (int i = 0; i < p->n; ++i) {
                    int32_t t = cy * SA_TPB + p->s[i].t;
                    if (t >= a && t < b) qadd(0, t, 3, 0, 0, p->s[i].v, 0);
                }
            } else {
                const spat *p = &SPAT[c->pat % NSPAT];
                for (int i = 0; i < p->n; ++i) {
                    int32_t t = cy * SA_TPB + p->s[i].t;
                    if (t >= a && t < b) qadd(0, t, p->s[i].d, p->s[i].tone, p->s[i].oct, p->s[i].v, 0);
                }
            }
        }
        return;
    }
    sa_mod md = m[k - 1];
    int n, x = md.val;
    switch (md.type) {
    case SA_M_FAST:
        n = inner(c, m, k, layer, a * x, b * x);
        for (int i = 0; i < n; ++i) qadd(k, fdiv(QT[i].t, x), QT[i].d / x, QT[i].tone, QT[i].oct, QT[i].v, QT[i].n);
        break;
    case SA_M_SLOW:
        n = inner(c, m, k, layer, fdiv(a, x), fdiv(b + x - 1, x));
        for (int i = 0; i < n; ++i) {
            int32_t t = QT[i].t * x;
            if (t >= a && t < b) qadd(k, t, QT[i].d * x, QT[i].tone, QT[i].oct, QT[i].v, QT[i].n);
        }
        break;
    case SA_M_REV:
    case SA_M_EVERY:
        for (int32_t cy = fdiv(a, SA_TPB); cy <= fdiv(b - 1, SA_TPB); ++cy) {
            int32_t c0 = cy * SA_TPB;
            bool rev = md.type == SA_M_REV || (x > 0 && ((cy % x) + x) % x == x - 1);
            n = inner(c, m, k, layer, c0, c0 + SA_TPB);
            for (int i = 0; i < n; ++i) {
                int32_t t = QT[i].t;
                if (rev) {
                    t = c0 + SA_TPB - (QT[i].t - c0) - QT[i].d;
                    if (t < c0) t = c0;
                }
                if (t >= a && t < b) qadd(k, t, QT[i].d, QT[i].tone, QT[i].oct, QT[i].v, QT[i].n);
            }
        }
        break;
    case SA_M_THIN:
        n = inner(c, m, k, layer, a, b);
        for (int i = 0; i < n; ++i)
            if ((int)(hash2((uint32_t)QT[i].t, (uint32_t)layer * 31u + (uint32_t)k) % 100u) >= x * 10)
                qadd(k, QT[i].t, QT[i].d, QT[i].tone, QT[i].oct, QT[i].v, QT[i].n);
        break;
    case SA_M_EUCLID:
        for (int32_t cy = fdiv(a, SA_TPB); cy <= fdiv(b - 1, SA_TPB); ++cy) {
            int32_t c0 = cy * SA_TPB;
            n = inner(c, m, k, layer, c0, c0 + SA_TPB);
            if (n == 0) continue;
            int j = 0, last = -1;
            for (int s = 0; s < 16; ++s) {
                if (((s * x) % 16) >= x) continue;      /* (Bresenham: x hits spread over 16) */
                int next = 16;
                for (int s2 = s + 1; s2 < 16; ++s2)
                    if (((s2 * x) % 16) < x) { next = s2; break; }
                const qev *e = &QT[j % n];
                int32_t t = c0 + s * SA_TPS;
                int dmax = (next - s) * SA_TPS;
                if (t >= a && t < b) qadd(k, t, e->d < dmax ? e->d : dmax, e->tone, e->oct, e->v, e->n);
                j++;
                last = s;
            }
            (void)last;
        }
        break;
    case SA_M_SHIFT: {
        int32_t sh = (x - 8) * SA_TPS;
        n = inner(c, m, k, layer, a - sh, b - sh);
        for (int i = 0; i < n; ++i) qadd(k, QT[i].t + sh, QT[i].d, QT[i].tone, QT[i].oct, QT[i].v, QT[i].n);
        break;
    }
    case SA_M_LEN: {
        int32_t L = x * SA_TPS;
        for (int32_t j = fdiv(a, L); j <= fdiv(b - 1, L); ++j) {
            n = inner(c, m, k, layer, j * SA_TPB, j * SA_TPB + L);
            for (int i = 0; i < n; ++i) {
                int32_t t = j * L + (QT[i].t - j * SA_TPB);
                int d = QT[i].d;
                if (t + d > (j + 1) * L) d = (int)((j + 1) * L - t);
                if (t >= a && t < b) qadd(k, t, d, QT[i].tone, QT[i].oct, QT[i].v, QT[i].n);
            }
        }
        break;
    }
    case SA_M_OCT:
        n = inner(c, m, k, layer, a, b);
        for (int i = 0; i < n; ++i) qadd(k, QT[i].t, QT[i].d, QT[i].tone, QT[i].oct + (x - 2), QT[i].v, QT[i].n);
        break;
    case SA_M_PLY:
        n = inner(c, m, k, layer, a, b);
        for (int i = 0; i < n; ++i) {
            int d = QT[i].d / x;
            if (d < 1) d = 1;
            for (int r = 0; r < x; ++r) {
                int32_t t = QT[i].t + r * d;
                if (t >= a && t < b) qadd(k, t, d, QT[i].tone, QT[i].oct, QT[i].v - r * 8, QT[i].n);
            }
        }
        break;
    default:
        n = inner(c, m, k, layer, a, b);
        for (int i = 0; i < n; ++i) qadd(k, QT[i].t, QT[i].d, QT[i].tone, QT[i].oct, QT[i].v, QT[i].n);
        break;
    }
}

/* a layer's notes in bar `bar` into QB[depth] (sorted by tick); the count */
static int bar_query(const sa_code *c, int layer, int bar, qev **out)
{
    sa_mod m[SA_MODS];
    int k = active_mods(c, m);
    query(c, m, k, layer, (int32_t)bar * SA_TPB, (int32_t)bar * SA_TPB + SA_TPB);
    qev *e = QB[k];
    int n = QN[k];
    for (int i = 1; i < n; ++i) {                  /* insertion sort by tick */
        qev v = e[i];
        int j = i - 1;
        while (j >= 0 && e[j].t > v.t) { e[j + 1] = e[j]; --j; }
        e[j + 1] = v;
    }
    *out = e;
    return n;
}

uint16_t sa_preview(const sa_groove *g, const sa_code *c, int layer, int bar)
{
    (void)g;
    if (c->type == SA_T_NONE || (c->pat == SA_PAT_OFF && !c->own)) return 0;
    qev *e;
    int n = bar_query(c, layer, bar < 0 ? 0 : bar, &e);
    uint16_t bits = 0;
    for (int i = 0; i < n; ++i) {
        int32_t t = e[i].t - (int32_t)(bar < 0 ? 0 : bar) * SA_TPB;
        if (t >= 0 && t < SA_TPB) bits |= (uint16_t)(1u << (t / SA_TPS));
    }
    return bits;
}

/* ---- the chords ---------------------------------------------------------------------------------------------- */
int sa_degree(const sa_harm *h, int deg)
{
    int o = deg >= 0 ? deg / 7 : -((6 - deg) / 7);
    return 12 * o + SCALE[h->scale % SA_SCALES][deg - 7 * o];
}

int sa_snap(const sa_harm *h, int note)
{
    const uint8_t *sc = SCALE[clampi(h->scale, 0, SA_SCALES - 1)];
    int best = note, bd = 99;
    for (int d = -6; d <= 6; ++d) {
        int pc = ((note + d - h->key) % 12 + 12) % 12;
        for (int i = 0; i < 7; ++i)
            if (sc[i] == pc) {
                int ad = d < 0 ? -d : d;
                if (ad < bd || (ad == bd && d < 0)) { bd = ad; best = note + d; }
            }
    }
    return best;
}

static void chord_of_bar(sa_groove *g, int bar)
{
    const sa_harm *h = &g->s.harm;
    int p = clampi(h->prog, 0, SA_PROGS - 1), bars = h->bars >= 4 ? 4 : h->bars >= 2 ? 2 : 1;
    int i = (bar < 0 ? 0 : bar / bars) % PROG[p].n;
    int d = PROG[p].deg[i], sc = clampi(h->scale, 0, SA_SCALES - 1);
    for (int t = 0; t < 4; ++t)
        g->chord_pc[t] = (uint8_t)((h->key + SCALE[sc][(d + 2 * t) % 7]) % 12);
    g->chord_deg = (uint8_t)d;
    cpy(g->chord_name, sa_note_name(g->chord_pc[0]), sizeof g->chord_name);
    int third = (g->chord_pc[1] - g->chord_pc[0] + 12) % 12, fifth = (g->chord_pc[2] - g->chord_pc[0] + 12) % 12;
    if (fifth == 6) cat(g->chord_name, "dim", sizeof g->chord_name);
    else if (third == 3) cat(g->chord_name, "m", sizeof g->chord_name);
}

/* the MIDI note of a chord tone at an octave (the third, fifth and seventh above the root) */
static int tone_note(const sa_groove *g, int tone, int oct)
{
    int root = g->chord_pc[0], base = 12 * (oct + 1) + root;
    if (tone == 4) return base + 12;
    int t = clampi(tone, 0, 3);
    return base + (g->chord_pc[t] - root + 12) % 12;
}

/* ---- the bar's notes ------------------------------------------------------------------------------------------- */
static void bn_add(sa_groove *g, int tick, int dur, int note, int vel, int layer)
{
    if (g->nbn >= SA_BAR_EVENTS || tick < 0 || tick >= SA_TPB) return;
    int i = g->nbn++;
    while (i > 0 && g->bn[i - 1].tick > tick) { g->bn[i] = g->bn[i - 1]; --i; }   /* kept sorted */
    sa_bar_note *b = &g->bn[i];
    b->tick = (uint8_t)tick;
    b->dur = (uint8_t)clampi(dur, 1, 255);
    b->note = (uint8_t)clampi(note, 0, 127);
    b->vel = (uint8_t)clampi(vel, 1, 127);
    b->layer = (uint8_t)layer;
}

/* the arpeggiator's i-th note of n (sorted low to high) */
static int arp_pick(int mode, uint32_t i, int n, int layer)
{
    if (n <= 1) return 0;
    uint32_t un = (uint32_t)n;
    switch (mode) {
    case SA_AR_DOWN: return n - 1 - (int)(i % un);
    case SA_AR_UPDOWN: { uint32_t p = i % (2 * un - 2); return (int)(p < un ? p : 2 * un - 2 - p); }
    case SA_AR_DOWNUP: { uint32_t p = i % (2 * un - 2); return n - 1 - (int)(p < un ? p : 2 * un - 2 - p); }
    case SA_AR_CONVERGE: { int k = (int)(i % un); return k % 2 ? n - 1 - k / 2 : k / 2; }
    case SA_AR_DIVERGE: { int k = n - 1 - (int)(i % un); return k % 2 ? n - 1 - k / 2 : k / 2; }
    case SA_AR_RANDOM: return (int)(hash2(i, (uint32_t)layer + 77u) % un);
    case SA_AR_THUMB: { uint32_t k = i % (2 * un - 2); return k % 2 ? 1 + (int)(k / 2) : 0; }
    case SA_AR_PINKY: { uint32_t k = i % (2 * un - 2); return k % 2 ? (int)(k / 2) : n - 1; }
    default: return (int)(i % un);
    }
}

static void layer_bar(sa_groove *g, int l, int bar)
{
    const sa_code *c = &g->s.code[l];
    if (c->type == SA_T_NONE || (c->pat == SA_PAT_OFF && !c->own)) return;
    qev *e;
    int n = bar_query(c, l, bar, &e);
    int32_t b0 = (int32_t)bar * SA_TPB;
    if (c->type == SA_T_DRUM) {                    /* LEVEL; ACCENT (env, 50: as written); CHANCE (cut) */
        const sa_live *v = &g->s.live[l];
        int lv = v->level, note = sa_dv_gm(c->sound), ac = v->env, ch = v->cut;
        for (int i = 0; i < n; ++i) {
            if (ch < 100) {
                uint32_t h = ((uint32_t)bar * 2654435761u) ^ ((uint32_t)e[i].t * 40503u) ^ ((uint32_t)l * 2246822519u);
                h ^= h >> 15;
                h *= 2654435761u;
                if ((int)((h >> 16) % 100u) >= ch) continue;
            }
            int vel = e[i].v;
            vel = vel >= 120 ? 90 + (vel - 90) * ac / 50 : vel * (150 - ac) / 100;
            vel = clampi(vel, 20, 127);
            bn_add(g, (int)(e[i].t - b0), e[i].d, note, vel * (20 + lv * 80 / 100) / 100, l);
        }
        return;
    }
    int poct = c->own ? c->ooct : SPAT[c->pat % NSPAT].oct;
    for (int i = 0; i < n; ++i) {
        int tick = (int)(e[i].t - b0), oct = poct + e[i].oct;
        int own_note = e[i].tone == SA_NOTE ? g->s.harm.key + e[i].n + 12 * e[i].oct : -1;
        if (own_note >= 0) oct = own_note / 12 - 1;
        if (c->amode == SA_AR_OFF || c->amode >= SA_AR_MODES) {
            if (own_note >= 0) {
                bn_add(g, tick, e[i].d, own_note, e[i].v, l);
            } else if (e[i].tone == SA_CH) {
                for (int t = 0; t < 3; ++t)
                    bn_add(g, tick, e[i].d, tone_note(g, t, oct), e[i].v - t * 4, l);
            } else {
                bn_add(g, tick, e[i].d, tone_note(g, e[i].tone, oct), e[i].v, l);
            }
            continue;
        }
        /* the arpeggiator: the chord's tones over RANGE octaves, from the note's octave up */
        int set[12], ns = 0, range = clampi(c->arange, 1, 4);
        for (int o = 0; o < range; ++o)
            for (int t = 0; t < 3; ++t)
                set[ns++] = tone_note(g, t, oct + o);
        int rate = RATE_TICKS[clampi(c->arate, 0, SA_RATES - 1)];
        int gate = rate * clampi(c->agate, 10, 100) / 100;
        int end = tick + e[i].d;
        if (end > SA_TPB) end = SA_TPB;
        for (int t = tick; t < end; t += rate) {
            if (c->amode == SA_AR_STRUM) {
                for (int s = 0; s < ns; ++s)
                    bn_add(g, t + s, gate > 1 ? gate : 1, set[s], e[i].v - s * 3, l);
            } else {
                int k = arp_pick(c->amode, g->arp_i[l]++, ns, l);
                bn_add(g, t, gate > 1 ? gate : 1, set[k], e[i].v, l);
            }
        }
    }
}

static void bar_build(sa_groove *g)
{
    chord_of_bar(g, g->bar);
    g->nbn = 0;
    g->bn_at = 0;
    for (int l = 0; l < SA_LAYERS; ++l)
        layer_bar(g, l, g->bar);
}

/* ---- the own patterns ------------------------------------------------------------------------------------------ */
bool sa_chordy(const sa_code *c)
{
    if (c->type != SA_T_SYNTH) return false;
    if (c->part == SA_L_CHORD) return true;        /* (the CHORD layer: its steps are chords) */
    if (c->pat == SA_PAT_OFF) return false;
    const spat *p = &SPAT[c->pat % NSPAT];
    for (int i = 0; i < p->n; ++i)
        if (p->s[i].tone == SA_CH) return true;
    return false;
}

void sa_own_make(sa_groove *g, sa_code *c)
{
    if (c->own || c->type == SA_T_NONE) return;
    c->hits = c->acc = c->chord = 0;
    for (int i = 0; i < 16; ++i) {
        c->sn[i] = SA_NO_NOTE;
        c->sl[i] = 1;
    }
    if (c->pat == SA_PAT_OFF) {                    /* (OFF: an empty one, in the layer's octave) */
        c->ooct = (uint8_t)(c->part == SA_L_BASS ? 2 : c->part == SA_L_LEAD ? 5 : 4);
        c->own = 1;
        return;
    }
    if (c->type == SA_T_DRUM) {                    /* (a triplet hit goes to its nearest sixteenth) */
        const dpat *p = &DPAT[c->pat % NDPAT];
        for (int i = 0; i < p->n; ++i) {
            int st = ((p->s[i].t + SA_TPS / 2) / SA_TPS) % 16;
            c->hits |= (uint16_t)(1u << st);
            if (p->s[i].v >= 120) c->acc |= (uint16_t)(1u << st);
        }
    } else {                                       /* the notes as the chord of the bar playing voices them */
        const spat *p = &SPAT[c->pat % NSPAT];
        c->ooct = (uint8_t)p->oct;
        for (int i = 0; i < p->n; ++i) {
            int st = ((p->s[i].t + SA_TPS / 2) / SA_TPS) % 16;
            int len = (p->s[i].d + SA_TPS / 2) / SA_TPS;
            c->sl[st] = (uint8_t)clampi(len, 1, 16);
            if (p->s[i].v >= 115) c->acc |= (uint16_t)(1u << st);
            if (p->s[i].tone == SA_CH) {
                c->chord |= (uint16_t)(1u << st);
            } else {
                int nn = tone_note(g, p->s[i].tone, p->oct + p->s[i].oct) - g->s.harm.key;
                c->sn[st] = (int8_t)clampi(nn, -127, 127);
            }
        }
    }
    c->own = 1;
}

void sa_step_drum(sa_groove *g, int l, int st)
{
    if (l < 0 || l >= SA_LAYERS || st < 0 || st > 15) return;
    sa_code *c = &g->ed[l];
    if (c->type != SA_T_DRUM) return;
    sa_own_make(g, c);
    c->hits ^= (uint16_t)(1u << st);
    if (!((c->hits >> st) & 1u)) c->acc &= (uint16_t)~(1u << st);
}

void sa_step_note(sa_groove *g, int l, int st, int note)
{
    if (l < 0 || l >= SA_LAYERS || st < 0 || st > 15) return;
    sa_code *c = &g->ed[l];
    if (c->type != SA_T_SYNTH) return;
    bool chordy = sa_chordy(c);
    sa_own_make(g, c);
    if (chordy) {
        c->chord ^= (uint16_t)(1u << st);
        c->sn[st] = SA_NO_NOTE;
    } else {
        int rel = clampi(sa_snap(&g->ed_harm, note) - g->ed_harm.key, -127, 127);
        if (c->sn[st] == rel && !((c->chord >> st) & 1u)) {
            c->sn[st] = SA_NO_NOTE;
        } else {
            c->sn[st] = (int8_t)rel;
            c->chord &= (uint16_t)~(1u << st);
        }
    }
    if (c->sl[st] < 1) c->sl[st] = 1;
}

void sa_step_len(sa_groove *g, int l, int st, int d)
{
    if (l < 0 || l >= SA_LAYERS || st < 0 || st > 15) return;
    sa_code *c = &g->ed[l];
    if (c->type != SA_T_SYNTH) return;
    sa_own_make(g, c);
    c->sl[st] = (uint8_t)clampi(c->sl[st] + d, 1, 16);
}

void sa_pat_label(const sa_code *c, char *out, int max)
{
    if (c->own) { cpy(out, "MY STEPS", max); return; }
    cpy(out, c->pat == SA_PAT_OFF ? "OFF" : sa_pat_name(c->type, c->pat), max);
}

/* ---- the gates ---------------------------------------------------------------------------------------------------- */
static const struct { const char *name; uint16_t mask; } GATE[SA_GATES] = {
    {"OFF", 0},
    {"16THS", 0xFFFF},
    {"8THS", 0x5555},
    {"OFFBEATS", 0x4444},
    {"PULSE 4", 0x1111},
    {"TRANCE A", 0x6D6D},                          /* (bit i: the sixteenth i) 0 2 3 5 6 8 10 11 13 14 */
    {"TRANCE B", 0x5B5B},                          /* 0 1 3 4 6 8 9 11 12 14 */
    {"DOTTED", 0x9249},                            /* 0 3 6 9 12 15 */
    {"SYNCO", 0x9999},                             /* 0 3 4 7 8 11 12 15 */
    {"BREATH", 0x7777},                            /* three on, one off */
    {"STUTTER", 0x0F0F},
    {"HALVES", 0x00FF},
    {"SPARSE", 0x0441},                            /* 0 6 10 */
};
const char *sa_gate_name(int p) { return GATE[clampi(p, 0, SA_GATES - 1)].name; }
uint16_t sa_gate_mask(const sa_gate *gt) { return gt->own ? gt->mask : GATE[gt->pat % SA_GATES].mask; }
bool sa_gate_on(const sa_gate *gt) { return gt->own || (gt->pat > 0 && gt->pat < SA_GATES); }
void sa_gate_label(const sa_gate *gt, char *out, int max)
{
    if (gt->own && gt->pat == 0) { cpy(out, "MY GATE", max); return; }
    cpy(out, sa_gate_name(gt->pat), max);
    if (gt->own) cat(out, " EDIT", max);
}
void sa_gate_step(sa_groove *g, int l, int st)
{
    if (l < 0 || l >= SA_LAYERS || st < 0 || st > 15) return;
    sa_gate *gt = &g->ed_gate[l];
    if (!gt->own) {
        gt->mask = GATE[gt->pat % SA_GATES].mask;
        gt->own = 1;
    }
    gt->mask ^= (uint16_t)(1u << st);
}
void sa_gate_pick(sa_groove *g, int l, int d)
{
    if (l < 0 || l >= SA_LAYERS) return;
    sa_gate *gt = &g->ed_gate[l];
    gt->pat = (uint8_t)(((gt->pat + d) % SA_GATES + SA_GATES) % SA_GATES);
    gt->own = 0;
}
static void gate_default(sa_gate *gt)
{
    memset(gt, 0, sizeof *gt);
    gt->depth = 100;
    gt->width = 50;
    gt->smooth = 25;
}

/* each synth part's gate, a block at a time: open on the steps of its layer's gate for WIDTH of the step (an off
 * sixteenth late by FEEL, as the notes), down by DEPTH between them, the edges faded by SMOOTH */
static void gate_block(sa_groove *g, int frames)
{
    float target[SA_SYNTHS] = {1.0f, 1.0f, 1.0f}, fade[SA_SYNTHS] = {1.0f, 1.0f, 1.0f};
    float tf = g->sr * 60.0f / (float)clampi(g->s.bpm, 60, 200) / 24.0f;
    float pos = (float)(g->tick < 0 ? 0 : g->tick) + 1.0f - (tf > 0.0f ? g->wait / tf : 0.0f);   /* ticks into the bar */
    if (pos < 0.0f) pos = 0.0f;
    int st = (int)(pos / (float)SA_TPS) % 16;
    float f = (pos - (float)(st * SA_TPS)) / (float)SA_TPS;   /* 0..1 through the sixteenth */
    if (st % 2 && g->s.feel) {                     /* (an off sixteenth: late, as the swung notes) */
        float sw = (float)g->s.feel / 100.0f * 0.66f;
        f = f < sw ? -1.0f : (f - sw) / (1.0f - sw);
    }
    for (int l = 0; l < SA_LAYERS; ++l) {
        const sa_code *c = &g->s.code[l];
        const sa_gate *gt = &g->s.gate[l];
        if (c->type != SA_T_SYNTH || c->part >= SA_SYNTHS || !g->playing || !sa_gate_on(gt)) continue;
        bool open = ((sa_gate_mask(gt) >> st) & 1u) && f >= 0.0f && f < (float)clampi(gt->width, 10, 100) / 100.0f;
        target[c->part] = open ? 1.0f : 1.0f - (float)gt->depth / 100.0f;
        float ms = 1.0f + (float)gt->smooth * 0.2f;          /* 1 .. 21 ms */
        fade[c->part] = (float)frames / (g->sr * ms * 0.001f);
    }
    for (int p = 0; p < SA_SYNTHS; ++p) {
        float d = target[p] - g->gate_gain[p], k = fade[p] > 1.0f ? 1.0f : fade[p];
        g->gate_gain[p] += d * k;
        if (g->gate_gain[p] < 0.0f) g->gate_gain[p] = 0.0f;
        if (g->gate_gain[p] > 1.0f) g->gate_gain[p] = 1.0f;
        g->gate_off[p] = (uint8_t)((1.0f - g->gate_gain[p]) * 127.0f + 0.5f);
    }
}

/* ---- the panning ---------------------------------------------------------------------------------------------- */
static const char *const PAN_MODES[SA_PAN_MODES] = {"FIXED", "SWEEP", "PING-PONG", "RANDOM"};
static const char *const PAN_RATES[SA_PRATES] = {"1/4 BAR", "1/2 BAR", "1 BAR", "2 BARS", "4 BARS"};
const char *sa_pan_mode_name(int m) { return PAN_MODES[clampi(m, 0, SA_PAN_MODES - 1)]; }
const char *sa_pan_rate_name(int r) { return PAN_RATES[clampi(r, 0, SA_PRATES - 1)]; }
static int pan_val(int pos, int off) { return clampi((pos + off) * 64 / 50, -64, 63); }   /* -50..50 -> -64..63 */
/* a note of a layer: PING-PONG and RANDOM move its part */
static void pan_note(sa_groove *g, int l)
{
    const sa_code *c = &g->s.code[l];
    const sa_pan *p = &g->s.pan[l];
    if (c->type != SA_T_SYNTH || c->part >= SA_SYNTHS) return;
    if (g->s.d3[c->part].on == SA_3D_RANDOM) {     /* 3D RANDOM: each note somewhere round you */
        g->pan_rnd = g->pan_rnd * 1664525u + 1013904223u;
        g->az_now[c->part] = (uint8_t)((g->pan_rnd >> 16) % SA_AZ_STEPS);
        return;
    }
    int w = p->width / 2;                          /* (WIDTH 100: 50 each way) */
    if (p->mode == SA_PAN_PINGPONG) {
        g->pan_side[l] ^= 1u;
        g->pan_now[c->part] = (int8_t)pan_val(p->pos, g->pan_side[l] ? w : -w);
    } else if (p->mode == SA_PAN_RANDOM) {
        g->pan_rnd = g->pan_rnd * 1664525u + 1013904223u;
        int r = w ? (int)((g->pan_rnd >> 16) % (uint32_t)(2 * w + 1)) - w : 0;
        g->pan_now[c->part] = (int8_t)pan_val(p->pos, r);
    }
}
/* every block: FIXED and SWEEP (a triangle from one side to the other and back over RATE) */
int sa_sin5(int k)
{
    static const uint8_t Q[19] = {0, 11, 22, 33, 43, 54, 64, 73, 82, 90, 97, 104, 110, 115, 119, 123, 125, 127, 127};
    k %= SA_AZ_STEPS;
    if (k < 0) k += SA_AZ_STEPS;
    return k <= 18 ? Q[k] : k <= 36 ? Q[36 - k] : k <= 54 ? -Q[k - 36] : -Q[SA_AZ_STEPS - k];
}

static void pan_block(sa_groove *g)
{
    static const uint8_t RATE_Q[SA_PRATES] = {1, 2, 4, 8, 16};     /* in quarter bars */
    for (int l = 0; l < SA_LAYERS; ++l) {
        const sa_code *c = &g->s.code[l];
        const sa_pan *p = &g->s.pan[l];
        if (c->type != SA_T_SYNTH || c->part >= SA_SYNTHS) continue;
        const sa_3d *d = &g->s.d3[c->part];
        if (d->on) {                               /* 3D: where it is now; the angle pans it */
            float len = (float)(SA_TPB / 4 * RATE_Q[g->s.d3rate[c->part] % SA_PRATES]);
            float at = (float)((g->bar < 0 ? 0 : g->bar) * SA_TPB + (g->tick < 0 ? 0 : g->tick));
            float ph = at / len;
            ph -= (float)(int)ph;
            int k = (int)(ph * (float)SA_AZ_STEPS);
            g->el_now[c->part] = d->el;
            if (d->on == SA_3D_FIXED) g->az_now[c->part] = d->az;
            else if (d->on == SA_3D_ORBIT || d->on == SA_3D_FLY) g->az_now[c->part] = (uint8_t)((d->az + k) % SA_AZ_STEPS);
            if (d->on == SA_3D_FLY) g->el_now[c->part] = (int8_t)clampi(d->el + sa_sin5(2 * k) * 30 / 127, -50, 50);
            g->pan_now[c->part] = (int8_t)(sa_sin5(g->az_now[c->part]) * 63 / 127);
        } else if (p->mode == SA_PAN_FIXED) {
            g->pan_now[c->part] = (int8_t)pan_val(p->pos, 0);
        } else if (p->mode == SA_PAN_SWEEP) {
            float len = (float)(SA_TPB / 4 * RATE_Q[p->rate % SA_PRATES]);
            float at = (float)((g->bar < 0 ? 0 : g->bar) * SA_TPB + (g->tick < 0 ? 0 : g->tick));
            float ph = at / len;
            ph -= (float)(int)ph;
            float tri = ph < 0.5f ? ph * 4.0f - 1.0f : 3.0f - ph * 4.0f;   /* -1 .. 1 .. -1 */
            g->pan_now[c->part] = (int8_t)pan_val(p->pos, (int)(tri * (float)(p->width / 2)));
        }
    }
}

/* ---- defaults and checks --------------------------------------------------------------------------------------- */
static void code_drum(sa_code *c, int dv, int pat)
{
    memset(c, 0, sizeof *c);
    c->type = SA_T_DRUM;
    c->sound = (uint8_t)dv;
    c->pat = (uint8_t)pat;
    c->part = SA_PART_DRUMS;
    c->arate = SA_RATE_16;
    c->arange = 1;
    c->agate = 60;
}
static void code_synth(sa_code *c, int sound, int pat, int part)
{
    memset(c, 0, sizeof *c);
    c->type = SA_T_SYNTH;
    c->sound = (uint8_t)sound;
    c->pat = (uint8_t)pat;
    c->part = (uint8_t)part;
    c->arate = SA_RATE_16;
    c->arange = 2;
    c->agate = 60;
}
static void live_default(sa_live *v)
{
    memset(v, 0, sizeof *v);
    v->cut = 100;
    v->reso = 30;
    v->env = 50;
    v->level = 80;
}

/* the synth sounds the first groove uses (sa_sounds.h indices: kept in step with it) */
#define SA_DEF_BASS 0
#define SA_DEF_STAB 1
void sa_defaults(sa_settings *s)
{
    memset(s, 0, sizeof *s);
    s->bpm = 126;
    s->filter = 100;
    s->feel = 18;
    s->kit = SA_KIT_TECH;
    s->ktune = 50;
    s->ktone = 55;
    s->kdecay = 50;
    s->kdrive = 15;
    s->pump = 35;
    s->reverb = 70;
    s->delay = 70;
    s->width = 70;
    s->harm.key = 9;                               /* A minor */
    s->harm.scale = SA_SC_MINOR;
    s->harm.prog = 1;
    s->harm.bars = 2;
    for (int l = 0; l < SA_LAYERS; ++l) {
        live_default(&s->live[l]);
        gate_default(&s->gate[l]);
        s->pan[l].width = 60;
        s->pan[l].rate = SA_PRATE_BAR;
    }
    for (int p = 0; p < SA_SYNTHS; ++p) s->d3rate[p] = SA_PRATE_2BARS;
    s->dvol = 80;
    for (int p = 0; p < SA_SYNTHS; ++p) {          /* LEAD 10 %, CHORD 80 %, BASS 50 % until the glue sets it by type */
        static const uint8_t DEP[SA_SYNTHS] = {10, 80, 50};
        s->pl[p].depth = DEP[p];
        s->pl[p].rel = 50;
        s->pl[p].src = SA_PS_KICK;
        s->pl[p].on = 2;
    }
    code_synth(&s->code[SA_L_LEAD], SA_DEF_BASS, SA_PAT_OFF, SA_L_LEAD);
    code_synth(&s->code[SA_L_CHORD], SA_DEF_STAB, 13, SA_L_CHORD);   /* STAB 2 */
    code_synth(&s->code[SA_L_BASS], SA_DEF_BASS, 1, SA_L_BASS);      /* ROLLER */
    for (int r = 0; r < SA_DROWS; ++r)
        code_drum(&s->code[SA_L_DRUM0 + r], ROW_DEF[r], SA_PAT_OFF);
    s->code[SA_L_DRUM0 + SA_DR_KICK].pat = 0;                         /* 4 FLOOR */
    s->code[SA_L_DRUM0 + SA_DR_HATS].pat = 1;                         /* OFFBEAT */
    s->code[SA_L_DRUM0 + SA_DR_CLAP].pat = 2;                         /* 2 + 4 */
}

/* the fixed layers: LEAD CHORD BASS in slots 0..2 on parts 0..2 (a synth layer elsewhere, or a drum there, moves:
 * its code, sound, gate and pan with it); a slot of them empty: that synth, OFF */
static void canon(sa_settings *s)
{
    sa_code c[SA_LAYERS];
    sa_live v[SA_LAYERS];
    sa_gate gt[SA_LAYERS];
    sa_pan pn[SA_LAYERS];
    bool have[3] = {false, false, false}, row[SA_DROWS] = {false};
    memset(c, 0, sizeof c);
    for (int l = 0; l < SA_LAYERS; ++l) {
        live_default(&v[l]);
        gate_default(&gt[l]);
        memset(&pn[l], 0, sizeof pn[l]);
        pn[l].width = 60;
        pn[l].rate = SA_PRATE_BAR;
    }
    for (int l = 0; l < SA_LAYERS; ++l) {           /* the synths to their parts' slots (the first of a part wins) */
        const sa_code *o = &s->code[l];
        int to = -1;
        if (o->type == SA_T_SYNTH && o->part < 3 && !have[o->part]) to = o->part;
        else if (o->type == SA_T_DRUM && !row[DV_ROW[o->sound % SA_DVS]]) to = SA_L_DRUM0 + DV_ROW[o->sound % SA_DVS];
        if (to < 0) continue;                      /* (a second drum on a row's channel: it goes) */
        if (o->type == SA_T_SYNTH) have[to] = true;
        else row[to - SA_L_DRUM0] = true;
        c[to] = *o;
        v[to] = s->live[l];
        gt[to] = s->gate[l];
        pn[to] = s->pan[l];
    }
    for (int p = 0; p < 3; ++p)
        if (!have[p]) code_synth(&c[p], 0, SA_PAT_OFF, p);
    for (int r = 0; r < SA_DROWS; ++r)              /* every row there: OFF if the groove has none */
        if (!row[r]) code_drum(&c[SA_L_DRUM0 + r], ROW_DEF[r], SA_PAT_OFF);
    memcpy(s->code, c, sizeof c);
    memcpy(s->live, v, sizeof v);
    memcpy(s->gate, gt, sizeof gt);
    memcpy(s->pan, pn, sizeof pn);
}

void sa_settings_fix(sa_settings *s)
{
    canon(s);                                      /* (first: the clamps below then hold for every slot) */
    s->bpm = (uint8_t)clampi(s->bpm, 60, 200);
    s->filter = (uint8_t)clampi(s->filter, 0, 100);
    s->feel = (uint8_t)clampi(s->feel, 0, 60);
    s->kit = (uint8_t)clampi(s->kit, 0, SA_KITS - 1);
    s->ktune = (uint8_t)clampi(s->ktune, 0, 100);
    s->ktone = (uint8_t)clampi(s->ktone, 0, 100);
    s->kdecay = (uint8_t)clampi(s->kdecay, 0, 100);
    s->kdrive = (uint8_t)clampi(s->kdrive, 0, 100);
    s->look = (uint8_t)(s->look ? 1 : 0);
    s->pump = (uint8_t)clampi(s->pump, 0, 100);
    s->reverb = (uint8_t)clampi(s->reverb, 0, 100);
    s->delay = (uint8_t)clampi(s->delay, 0, 100);
    s->width = (uint8_t)clampi(s->width, 0, 100);
    s->harm.key %= 12;
    s->harm.scale = (uint8_t)clampi(s->harm.scale, 0, SA_SCALES - 1);
    s->harm.prog = (uint8_t)clampi(s->harm.prog, 0, SA_PROGS - 1);
    s->harm.bars = (uint8_t)(s->harm.bars >= 4 ? 4 : s->harm.bars >= 2 ? 2 : 1);
    int synths = 0;
    uint8_t parts = 0;
    for (int l = 0; l < SA_LAYERS; ++l) {
        sa_code *c = &s->code[l];
        sa_live *v = &s->live[l];
        if (c->type > SA_T_SYNTH) c->type = SA_T_NONE;
        if (c->type == SA_T_SYNTH) {             /* three at most, each on a part of its own */
            if (synths >= SA_SYNTHS || c->part >= SA_SYNTHS || (parts >> c->part) & 1u) {
                int p = 0;
                while (p < SA_SYNTHS && ((parts >> p) & 1u)) ++p;
                if (synths >= SA_SYNTHS || p >= SA_SYNTHS) c->type = SA_T_NONE;
                else c->part = (uint8_t)p;
            }
            if (c->type == SA_T_SYNTH) {
                parts |= (uint8_t)(1u << c->part);
                synths++;
                if (c->pat != SA_PAT_OFF) c->pat %= (uint8_t)NSPAT;
            }
        }
        if (c->type == SA_T_DRUM) {
            c->part = SA_PART_DRUMS;
            c->sound %= SA_DVS;
            if (c->pat != SA_PAT_OFF) c->pat %= (uint8_t)NDPAT;
        }
        for (int i = 0; i < SA_MODS; ++i) {
            sa_mod *m = &c->mod[i];
            if (m->type >= SA_M_TYPES) m->type = SA_M_OFF;
            for (int r = 0; r < NMODR; ++r)
                if (MODS[r].type == m->type) m->val = (uint8_t)clampi(m->val, MODS[r].lo, MODS[r].hi);
            if (mod_skip(c->type, m->type, m->val)) m->type = SA_M_OFF;
        }
        c->own = (uint8_t)(c->own ? 1 : 0);
        c->ooct = (uint8_t)clampi(c->ooct, 1, 6);
        for (int i = 0; i < 16; ++i)
            c->sl[i] = (uint8_t)clampi(c->sl[i], 1, 16);
        c->amode = (uint8_t)(c->amode < (c->type == SA_T_DRUM ? SA_DVARS : SA_AR_MODES) ? c->amode : 0);   /* (a drum: its
                                                                                                   * variation) */
        c->arate = (uint8_t)clampi(c->arate, 0, SA_RATES - 1);
        c->arange = (uint8_t)clampi(c->arange, 1, 4);
        c->agate = (uint8_t)clampi(c->agate, 10, 100);
        v->mute = (uint8_t)(v->mute ? 1 : 0);
        v->cut = (uint8_t)clampi(v->cut, 0, 100);
        v->reso = (uint8_t)clampi(v->reso, 0, 100);
        v->drive = (uint8_t)clampi(v->drive, 0, 100);
        v->env = (uint8_t)clampi(v->env, 0, 100);
        v->level = (uint8_t)clampi(v->level, 0, 100);
        v->nopump = (uint8_t)(v->nopump ? 1 : 0);
        sa_gate *gt = &s->gate[l];
        gt->pat %= SA_GATES;
        gt->own = (uint8_t)(gt->own ? 1 : 0);
        gt->depth = (uint8_t)clampi(gt->depth, 0, 100);
        gt->width = (uint8_t)clampi(gt->width, 10, 100);
        gt->smooth = (uint8_t)clampi(gt->smooth, 0, 100);
        sa_pan *pn = &s->pan[l];
        pn->mode = (uint8_t)(pn->mode % SA_PAN_MODES);
        pn->pos = (int8_t)clampi(pn->pos, -50, 50);
        pn->width = (uint8_t)clampi(pn->width, 0, 100);
        pn->rate = (uint8_t)(pn->rate % SA_PRATES);
    }
    for (int p = 0; p < SA_SYNTHS; ++p) {
        sa_pumpl *q = &s->pl[p];
        q->depth = (uint8_t)clampi(q->depth, 0, 100);
        q->rel = (uint8_t)clampi(q->rel, 0, 100);
        q->src = (uint8_t)(q->src % SA_PSRCS);
        if (q->on > 2) q->on = 1;
    }
    for (int p = 0; p < SA_SYNTHS; ++p) {
        sa_3d *d = &s->d3[p];
        d->on = (uint8_t)(d->on % SA_3D_MODES);
        s->d3rate[p] = (uint8_t)(s->d3rate[p] % SA_PRATES);
        s->dvol = (uint8_t)clampi(s->dvol, 0, 100);
        s->mreso = (uint8_t)clampi(s->mreso, 0, 100);
        s->dmute = (uint8_t)(s->dmute ? 1 : 0);
        d->az %= SA_AZ_STEPS;
        d->el = (int8_t)clampi(d->el, -50, 50);
        d->dist = (uint8_t)clampi(d->dist, 0, 100);
    }
}

/* ---- a fresh groove ---------------------------------------------------------------------------------------------- */
static uint32_t rnd_next(uint32_t *r)
{
    uint32_t x = *r;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *r = x;
}
static int rnd(uint32_t *r, int n) { return n > 0 ? (int)((rnd_next(r) >> 8) % (uint32_t)n) : 0; }
static bool streq(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}
static int pat_find(int type, const char *name)
{
    int n = type == SA_T_DRUM ? NDPAT : NSPAT;
    for (int i = 0; i < n; ++i)
        if (streq(type == SA_T_DRUM ? DPAT[i].name : SPAT[i].name, name)) return i;
    return 0;
}
static int pick_pat(uint32_t *r, int type, const char *const *names, int n) { return pat_find(type, names[rnd(r, n)]); }
static int pick_sound(uint32_t *r, const uint8_t *list, int n) { return n > 0 ? list[rnd(r, n)] : 0; }

void sa_random(sa_settings *s, uint32_t seed, const sa_pool *p)
{
    static const char *const BASS[] = {"OFFBEAT", "ROLLER", "ROOT 8THS", "OCTAVE", "PUMP 16", "SYNCO", "ACID",
                                       "GROWL", "2-STEP", "HALF"};
    static const char *const STABS[] = {"STAB 1", "STAB 2", "OFFBEATS", "HOUSE", "CHARLESTON", "DOWNBEAT", "PUSH",
                                        "DOTTED"};
    static const char *const PADS[] = {"WASH", "TWO HALVES", "SWELL"};
    static const char *const HATS[] = {"OFFBEAT", "16THS", "SKIP", "OFFBEAT"};
    static const uint8_t BPM[] = {122, 124, 125, 126, 126, 128, 128, 130};
    static const uint8_t FEEL[] = {0, 8, 14, 18, 18, 24};
    static const uint8_t SCL[] = {SA_SC_MINOR, SA_SC_MINOR, SA_SC_MINOR, SA_SC_MINOR, SA_SC_DORIAN, SA_SC_DORIAN,
                                    SA_SC_PHRYGIAN, SA_SC_MAJOR};
    static const uint8_t BARS[] = {1, 2, 2, 4};
    static const uint8_t PUMP[] = {20, 30, 35, 45};
    uint32_t r = seed * 2654435761u ^ 0x9E3779B9u;
    if (!r) r = 1;
    for (int i = 0; i < 4; ++i) rnd_next(&r);
    sa_defaults(s);
    memset(s->code, 0, sizeof s->code);
    s->bpm = BPM[rnd(&r, sizeof BPM)];
    s->feel = FEEL[rnd(&r, sizeof FEEL)];
    s->kit = (uint8_t)rnd(&r, SA_KITS);
    s->pump = PUMP[rnd(&r, sizeof PUMP)];
    s->harm.key = (uint8_t)rnd(&r, 12);
    s->harm.scale = SCL[rnd(&r, sizeof SCL)];
    s->harm.prog = (uint8_t)rnd(&r, SA_PROGS);
    s->harm.bars = BARS[rnd(&r, sizeof BARS)];
    static const char *const LEADS[] = {"TONES UP", "RIFF", "PLUCKS", "CALL"};
    int l = SA_L_DRUM0;
    /* the drums: a kick, hats, a clap or a snare on 2 and 4; sometimes an open hat, a shaker, a rim, a conga, a ride */
    code_drum(&s->code[l++], SA_DV_KICK, pat_find(SA_T_DRUM, rnd(&r, 8) ? "4 FLOOR" : rnd(&r, 2) ? "2-STEP" : "BROKEN"));
    int hat = pick_pat(&r, SA_T_DRUM, HATS, (int)(sizeof HATS / sizeof HATS[0]));
    code_drum(&s->code[l++], rnd(&r, 5) ? SA_DV_HAT : SA_DV_SHAKER, hat);
    code_drum(&s->code[l++], rnd(&r, 3) ? SA_DV_CLAP : SA_DV_SNARE, pat_find(SA_T_DRUM, "2 + 4"));
    (void)hat;
    switch (rnd(&r, 6)) {                          /* (one more, on a row of its own) */
    case 0: code_drum(&s->code[l++], SA_DV_CLAVE, pat_find(SA_T_DRUM, "CLAVE")); break;
    case 1: code_drum(&s->code[l++], SA_DV_RIM, pat_find(SA_T_DRUM, "SYNCO")); break;
    case 2: code_drum(&s->code[l++], SA_DV_CONGA, pat_find(SA_T_DRUM, "TRESILLO")); break;
    case 3: code_drum(&s->code[l++], SA_DV_RIDE, pat_find(SA_T_DRUM, "8THS")); break;
    default: break;
    }
    /* BASS always; CHORD stabs or a pad (sometimes gated, sweeping); LEAD now and then, mostly OFF */
    code_synth(&s->code[SA_L_BASS], pick_sound(&r, p->bass, p->nbass),
               pick_pat(&r, SA_T_SYNTH, BASS, (int)(sizeof BASS / sizeof BASS[0])), SA_L_BASS);
    if (rnd(&r, 100) < 55) {
        code_synth(&s->code[SA_L_CHORD], pick_sound(&r, p->keys, p->nkeys),
                   pick_pat(&r, SA_T_SYNTH, STABS, (int)(sizeof STABS / sizeof STABS[0])), SA_L_CHORD);
    } else {
        code_synth(&s->code[SA_L_CHORD], pick_sound(&r, p->pad, p->npad),
                   pick_pat(&r, SA_T_SYNTH, PADS, (int)(sizeof PADS / sizeof PADS[0])), SA_L_CHORD);
        if (rnd(&r, 100) < 35) s->gate[SA_L_CHORD].pat = (uint8_t)(1 + rnd(&r, SA_GATES - 1));
        if (rnd(&r, 100) < 40) s->pan[SA_L_CHORD].mode = SA_PAN_SWEEP;
    }
    code_synth(&s->code[SA_L_LEAD], pick_sound(&r, p->lead, p->nlead),
               rnd(&r, 100) < 25 ? pick_pat(&r, SA_T_SYNTH, LEADS, (int)(sizeof LEADS / sizeof LEADS[0])) : SA_PAT_OFF,
               SA_L_LEAD);
    sa_settings_fix(s);
}

/* ---- the engine ------------------------------------------------------------------------------------------------ */
void sa_init(sa_groove *g, float sample_rate)
{
    memset(g, 0, sizeof *g);
    g->sr = sample_rate;
    sa_defaults(&g->s);
    g->tick = -1;
    g->bar = 0;
    for (int k = 0; k < 27; ++k) {
        g->key_note[k] = -1;
        g->key_part[k] = -1;
    }
    sa_edit_reset(g);
    chord_of_bar(g, 0);
}

void sa_set_sound(sa_groove *g, void (*fn)(void *ctx, const sa_event *e), void *ctx)
{
    g->sound = fn;
    g->sound_ctx = ctx;
}

void sa_edit_reset(sa_groove *g)
{
    memcpy(g->ed, g->s.code, sizeof g->ed);
    memcpy(g->ed_gate, g->s.gate, sizeof g->ed_gate);
    g->ed_harm = g->s.harm;
    g->drop_armed = false;
}

bool sa_layer_pending(const sa_groove *g, int l)
{
    return l >= 0 && l < SA_LAYERS && (memcmp(&g->ed[l], &g->s.code[l], sizeof g->ed[l]) != 0 ||
                                       memcmp(&g->ed_gate[l], &g->s.gate[l], sizeof g->ed_gate[l]) != 0);
}

int sa_pending(const sa_groove *g)
{
    int n = 0;
    for (int l = 0; l < SA_LAYERS; ++l)
        n += sa_layer_pending(g, l);
    n += memcmp(&g->ed_harm, &g->s.harm, sizeof g->ed_harm) != 0;
    return n;
}

bool sa_used(const sa_groove *g, int l)
{
    return l >= 0 && l < SA_LAYERS && (g->ed[l].type != SA_T_NONE || g->s.code[l].type != SA_T_NONE);
}

int sa_synth_count(const sa_groove *g)
{
    int n = 0;
    for (int l = 0; l < SA_LAYERS; ++l)
        n += g->ed[l].type == SA_T_SYNTH;
    return n;
}

int sa_synth_free_part(const sa_groove *g, int except)
{
    uint8_t used = 0;
    for (int l = 0; l < SA_LAYERS; ++l)
        if (l != except && g->ed[l].type == SA_T_SYNTH && g->ed[l].part < SA_SYNTHS)
            used |= (uint8_t)(1u << g->ed[l].part);
    for (int p = 0; p < SA_SYNTHS; ++p)
        if (!((used >> p) & 1u)) return p;
    return -1;
}

static void emit(sa_groove *g, int on, int part, int note, int vel)
{
    if (!g->sound) return;
    sa_event e = {(uint8_t)on, (uint8_t)part, (uint8_t)clampi(note, 0, 127), (uint8_t)clampi(vel, 1, 127)};
    g->sound(g->sound_ctx, &e);
}

static void msg(sa_groove *g, const char *m)
{
    cpy(g->msg, m, sizeof g->msg);
    g->msg_new = 1;
}

static void note_off_now(sa_groove *g, int i)
{
    emit(g, 0, g->held[i].part, g->held[i].note, 0);
    g->held[i] = g->held[--g->nheld];
}

void sa_all_off(sa_groove *g)
{
    while (g->nheld > 0)
        note_off_now(g, g->nheld - 1);
    g->npend = 0;
}

static void part_off(sa_groove *g, int part)
{
    for (int i = g->nheld - 1; i >= 0; --i)
        if (g->held[i].part == part) note_off_now(g, i);
    for (int i = g->npend - 1; i >= 0; --i)
        if (g->pend[i].part == part) g->pend[i] = g->pend[--g->npend];
}

static float tick_frames(const sa_groove *g)
{
    return g->sr * 60.0f / (float)clampi(g->s.bpm, 60, 200) / 24.0f;
}

/* a note now, its note-off after len frames; the part's note at that pitch still sounding is cut first */
static void note(sa_groove *g, int part, int n, int vel, uint32_t len)
{
    if (part == SA_PART_DRUMS && n == 36) {        /* a kick: the pump starts again */
        g->kick_clock = g->clock;
        g->kicked = true;
    }
    for (int i = 0; i < g->nheld; ++i)
        if (g->held[i].part == part && g->held[i].note == n) {
            note_off_now(g, i);
            break;
        }
    if (g->nheld >= SA_HELD) {                     /* full: the oldest goes */
        int o = 0;
        for (int i = 1; i < g->nheld; ++i)
            if ((int32_t)(g->held[i].off - g->held[o].off) < 0) o = i;
        note_off_now(g, o);
    }
    emit(g, 1, part, n, vel);
    g->held[g->nheld].note = (uint8_t)n;
    g->held[g->nheld].part = (uint8_t)part;
    g->held[g->nheld].off = g->clock + (len > 0 ? len : 1u);
    g->nheld++;
}

/* REC: the copy goes in */
static void drop_now(sa_groove *g)
{
    for (int l = 0; l < SA_LAYERS; ++l) {
        const sa_code *o = &g->s.code[l], *n = &g->ed[l];
        if (!memcmp(o, n, sizeof *o)) continue;
        if (o->type == SA_T_SYNTH && (n->type != SA_T_SYNTH || n->sound != o->sound || n->part != o->part))
            part_off(g, o->part);                  /* (its sound changes or it goes: let go of its notes) */
        if (n->type != o->type)
            g->s.live[l].mute = 0;
        g->arp_i[l] = 0;
    }
    memcpy(g->s.code, g->ed, sizeof g->ed);
    memcpy(g->s.gate, g->ed_gate, sizeof g->ed_gate);
    g->s.harm = g->ed_harm;
    g->drop_armed = false;
    g->sound_sig++;
}

void sa_sound_now(sa_groove *g, int l, int sound)
{
    if (l < 0 || l >= SA_LAYERS || g->s.code[l].type != SA_T_SYNTH) return;
    part_off(g, g->s.code[l].part);
    g->s.code[l].sound = (uint8_t)sound;
    g->ed[l].sound = (uint8_t)sound;
    g->sound_sig++;
}

void sa_drop(sa_groove *g)
{
    if (sa_pending(g) == 0) {
        msg(g, "NOTHING TO DROP");
        return;
    }
    if (!g->playing) {
        drop_now(g);
        chord_of_bar(g, 0);
        msg(g, "DROPPED");
        return;
    }
    g->drop_armed = true;
}

static void set_now(sa_groove *g)
{
    const sa_settings *n = g->set_next;
    uint8_t pal = g->s.palette, look = g->s.look;
    g->set_next = 0;
    sa_all_off(g);
    g->s = *n;
    g->s.palette = pal;
    g->s.look = look;
    sa_settings_fix(&g->s);
    memset(g->arp_i, 0, sizeof g->arp_i);
    sa_edit_reset(g);
    g->sound_sig++;
}

void sa_set_arm(sa_groove *g, const sa_settings *s)
{
    g->set_next = s;
    if (!g->playing) {
        set_now(g);
        chord_of_bar(g, 0);
    }
}

static void bar_line(sa_groove *g)
{
    for (int l = 0; l < SA_LAYERS; ++l) {
        g->hits_last[l] = g->hits[l];
        g->hits[l] = 0;
    }
    if (g->brk_on && !g->brk_held) {               /* BREAK let go: everything back on this 1 */
        g->brk_on = false;
        msg(g, "DROP");
    }
    if (g->set_next) {
        set_now(g);
        msg(g, "IN ON THE 1");
    } else if (g->drop_armed) {
        drop_now(g);
        msg(g, "DROPPED");
    }
    bar_build(g);
}

void sa_play(sa_groove *g, bool on)
{
    if (on == g->playing) return;
    sa_all_off(g);
    g->playing = on;
    g->clock = 0;
    g->wait = 0.0f;
    g->tick = SA_TPB - 1;                          /* the first tick wraps into bar 0 (its bar line) */
    g->bar = -1;
    if (!on) {
        g->tick = -1;
        g->bar = 0;
        if (g->set_next) set_now(g);               /* (stopped: what was armed goes in now) */
        else if (g->drop_armed) drop_now(g);
        chord_of_bar(g, 0);
        for (int l = 0; l < SA_LAYERS; ++l) g->hits[l] = g->hits_last[l] = 0;
    }
}

void sa_mute(sa_groove *g, int l, bool m)
{
    if (l < 0 || l >= SA_LAYERS) return;
    g->s.live[l].mute = (uint8_t)m;
    if (m && g->s.code[l].type == SA_T_SYNTH)
        part_off(g, g->s.code[l].part);
}

/* ---- the drum fill (held) ---------------------------------------------------------------------------------------- */
enum { FILL_ROLL, FILL_SNARE, FILL_BUILD, FILL_TOMS, FILL_STUTTER, FILLS };
static const char *const FILL_NAMES[FILLS] = {"ROLL", "SNARE", "BUILD", "TOMS", "STUTTER"};
const char *sa_fill_name(int kind) { return FILL_NAMES[clampi(kind, 0, FILLS - 1)]; }
void sa_fill(sa_groove *g, bool down)
{
    if (!down && g->fill_on) g->fill_kind = (uint8_t)((g->fill_kind + 1) % FILLS);   /* (next time, the next fill) */
    g->fill_held = down;
    if (!down) g->fill_on = 0;                     /* (let go: the groove again, at once) */
}
void sa_break(sa_groove *g, bool down)
{
    g->brk_held = down;
    if (down && !g->brk_on) {                      /* out at once: let go of what sounds */
        g->brk_on = true;
        part_off(g, SA_PART_DRUMS);
        part_off(g, g->s.code[SA_L_BASS].part);
        msg(g, "BREAK");
    }
    if (!g->playing && !down) g->brk_on = false;   /* (stopped: no 1 to wait for) */
}
void sa_sweep(sa_groove *g, bool down)
{
    g->sweep_held = down;
    if (!down) g->sweep = 0.0f;                    /* (let go: open at once) */
}
static int row_gm(const sa_groove *g, int row, int dv)   /* the row's drum as it is, or dv when the row has none */
{
    const sa_code *c = &g->s.code[SA_L_DRUM0 + row];
    return sa_dv_gm(c->type == SA_T_DRUM ? c->sound : dv);
}
/* a sixteenth of the fill: s from where it started (each beat its four) */
static void fill_tick(sa_groove *g, int s)
{
    uint32_t len = (uint32_t)(g->sr * 0.25f);
    int sn = row_gm(g, SA_DR_SNARE, SA_DV_SNARE), kick = sa_dv_gm(SA_DV_KICK), ramp = clampi(70 + s * 4, 70, 127);
    if (s % 4 == 0 && g->fill_kind != FILL_STUTTER) note(g, SA_PART_DRUMS, kick, 110, len);
    switch (g->fill_kind) {
    case FILL_ROLL: note(g, SA_PART_DRUMS, sn, ramp, len); break;
    case FILL_SNARE: if (s % 4 != 1) note(g, SA_PART_DRUMS, sn, s % 4 == 0 ? 120 : 96, len); break;
    case FILL_BUILD: if (s >= 8 || s % 2 == 0) note(g, SA_PART_DRUMS, sn, ramp, len); break;
    case FILL_TOMS: note(g, SA_PART_DRUMS, sa_dv_gm((s / 2) % 2 ? SA_DV_LOTOM : SA_DV_TOM), ramp, len); break;
    default:                                       /* STUTTER: kick and hat on every sixteenth */
        note(g, SA_PART_DRUMS, kick, 104, len);
        note(g, SA_PART_DRUMS, sa_dv_gm(SA_DV_HAT), 90, len);
        break;
    }
}

static void do_tick(sa_groove *g)
{
    if (++g->tick >= SA_TPB) {
        g->tick = 0;
        g->bar++;
        bar_line(g);
    }
    int t = g->tick;
    float tf = tick_frames(g);
    if (t % 24 == 0 && g->fill_held && !g->fill_on) {   /* the fill: from the beat after ENV went down */
        g->fill_on = 1;
        g->fill_step = 0;
        msg(g, FILL_NAMES[g->fill_kind % FILLS]);
    }
    if (g->fill_on && t % SA_TPS == 0 && !g->brk_on) {
        fill_tick(g, g->fill_step);
        g->fill_step = (uint8_t)(g->fill_step + 1);
        if (g->fill_step >= 16) g->fill_step = 0;   /* (held past a bar: it goes round) */
    }
    uint32_t delay = 0;
    if (t % 12 == 6 && g->s.feel)                  /* FEEL: the off sixteenths late */
        delay = (uint32_t)(tf * 6.0f * (float)g->s.feel / 100.0f * 0.66f);
    while (g->bn_at < g->nbn && g->bn[g->bn_at].tick <= t) {
        const sa_bar_note *b = &g->bn[g->bn_at++];
        if (b->tick != t) continue;
        const sa_code *c = &g->s.code[b->layer];
        if (c->type == SA_T_NONE || g->s.live[b->layer].mute) continue;
        if (c->type == SA_T_DRUM && g->fill_on) continue;   /* (the fill plays in place of the drums) */
        if (g->brk_on && (c->type == SA_T_DRUM || b->layer == SA_L_BASS)) continue;   /* BREAK: out */
        if (g->solo && !((g->solo >> (c->type == SA_T_DRUM ? 3 : b->layer)) & 1u)) continue;   /* SOLO: the others out */
        int part = c->type == SA_T_DRUM ? SA_PART_DRUMS : c->part;
        uint32_t len = c->type == SA_T_DRUM ? (uint32_t)(g->sr * (c->sound == SA_DV_CRASH ? 1.0f :
                                                                  c->sound == SA_DV_OPEN ? 0.4f : 0.3f))
                                            : (uint32_t)((float)b->dur * tf);
        g->hits[b->layer] |= (uint16_t)(1u << (t / SA_TPS));
        if (delay == 0 || g->npend >= SA_PEND) {
            pan_note(g, b->layer);
            note(g, part, b->note, b->vel, len);
        } else {
            g->pend[g->npend].at = g->clock + delay;
            g->pend[g->npend].part = (uint8_t)part;
            g->pend[g->npend].note = b->note;
            g->pend[g->npend].vel = b->vel;
            g->pend[g->npend].layer = b->layer;
            g->pend[g->npend].len = len;
            g->npend++;
        }
    }
}

/* the pump: down fast on the kick (6 ms), back over 70 % of a beat, as deep as PUMP says */
static void pump_block(sa_groove *g)
{
    float beat = 60.0f / (float)clampi(g->s.bpm, 60, 200), master = (float)g->s.pump / 100.0f * 0.85f;
    for (int p = 0; p < SA_SYNTHS; ++p) {
        const sa_pumpl *pl = &g->s.pl[p];
        float d = 0.0f, t = -1.0f;
        if (g->playing && pl->on && pl->depth && g->s.pump) {
            if (pl->src == SA_PS_BEAT) {
                uint32_t fpb = (uint32_t)(g->sr * beat);
                t = fpb ? (float)(g->clock % fpb) / g->sr : -1.0f;
            } else if (g->kicked) {
                t = (float)(g->clock - g->kick_clock) / g->sr;
            }
        }
        if (t >= 0.0f) {
            float depth = master * (float)pl->depth / 100.0f, back = (0.25f + (float)pl->rel * 0.009f) * beat;
            if (t < 0.006f) {
                d = depth * t / 0.006f;
            } else {
                float k = (t - 0.006f) / back;
                if (k < 1.0f) d = depth * (1.0f - k) * (1.0f - k);
            }
        }
        g->duck[p] = (uint8_t)(d * 127.0f + 0.5f);
    }
}

void sa_process(sa_groove *g, int frames)
{
    if (g->sweep_held && g->sweep < 1.0f) {        /* SWEEP: closing over two bars */
        float bars2 = 2.0f * 4.0f * 60.0f / (float)clampi(g->s.bpm, 60, 200) * g->sr;
        g->sweep += (float)frames / bars2;
        if (g->sweep > 1.0f) g->sweep = 1.0f;
    }
    if (!g->playing) {
        for (int p = 0; p < SA_SYNTHS; ++p) { g->duck[p] = 0; g->gate_gain[p] = 1.0f; g->gate_off[p] = 0; }
        pan_block(g);
        return;
    }
    float tf = tick_frames(g);
    int left = frames;
    while (left > 0) {
        if (g->wait <= 0.0f) {
            do_tick(g);
            g->wait += tf;
        }
        int chunk = (int)g->wait + 1;
        if (chunk > left) chunk = left;
        if (chunk < 1) chunk = 1;
        g->clock += (uint32_t)chunk;
        g->wait -= (float)chunk;
        left -= chunk;
        for (int k = 0; k < g->npend;) {           /* the swung notes */
            if ((int32_t)(g->clock - g->pend[k].at) >= 0) {
                if (!g->s.live[g->pend[k].layer].mute) {
                    pan_note(g, g->pend[k].layer);
                    note(g, g->pend[k].part, g->pend[k].note, g->pend[k].vel, g->pend[k].len);
                }
                g->pend[k] = g->pend[--g->npend];
            } else {
                ++k;
            }
        }
        for (int k = 0; k < g->nheld;) {           /* the note-offs */
            if ((int32_t)(g->clock - g->held[k].off) >= 0) note_off_now(g, k);
            else ++k;
        }
    }
    pump_block(g);
    gate_block(g, frames);
    pan_block(g);
}

void sa_key_drum(sa_groove *g, int k, int dv, bool down) { sa_key(g, k, -1 - clampi(dv, 0, SA_DVS - 1), down, -2); }

void sa_key(sa_groove *g, int k, int note_in, bool down, int layer)
{
    if (k < 0 || k >= 27) return;
    if (down) {
        if (g->key_note[k] >= 0) emit(g, 0, g->key_part[k], g->key_note[k], 0);
        int part = -1, n = note_in, vel = 104;
        if (layer == -2) {                         /* (sa_key_drum: a drum voice, note -1 - dv) */
            part = SA_PART_DRUMS;
            n = sa_dv_gm(-1 - note_in);
        } else if (layer >= 0 && layer < SA_LAYERS && g->s.code[layer].type == SA_T_DRUM) {
            part = SA_PART_DRUMS;
            n = sa_dv_gm(g->s.code[layer].sound);
        } else {
            if (layer >= 0 && layer < SA_LAYERS && g->s.code[layer].type == SA_T_SYNTH)
                part = g->s.code[layer].part;      /* the open layer's part */
            if (part < 0)                          /* no synth layer open: the first synth layer's */
                for (int l = 0; l < SA_LAYERS && part < 0; ++l)
                    if (g->s.code[l].type == SA_T_SYNTH) part = g->s.code[l].part;
            if (part < 0) return;
            n = sa_snap(&g->s.harm, note_in);
        }
        g->keys |= 1u << k;
        g->key_note[k] = n;
        g->key_part[k] = part;
        emit(g, 1, part, n, vel);
    } else {
        g->keys &= ~(1u << k);
        if (g->key_note[k] >= 0)
            emit(g, 0, g->key_part[k], g->key_note[k], 0);
        g->key_note[k] = -1;
        g->key_part[k] = -1;
    }
}
