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
                                               "CLASSIC"};
const char *sa_kit_name(int kit) { return KIT_NAMES[clampi(kit, 0, SA_KITS - 1)]; }
static const char *const DV_NAMES[SA_DVS] = {"KICK", "SNARE", "CLAP", "HAT", "OPEN HAT", "RIDE", "CRASH", "RIM",
                                             "TOM", "LOW TOM", "CONGA", "COWBELL", "CLAVE", "SHAKER"};
static const uint8_t DV_GM[SA_DVS] = {36, 38, 39, 42, 46, 51, 49, 37, 45, 41, 63, 56, 75, 70};
const char *sa_dv_name(int dv) { return DV_NAMES[clampi(dv, 0, SA_DVS - 1)]; }
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
typedef struct { int32_t t; uint8_t d, tone, v; int8_t oct; } qev;
#define QMAX 64
static qev QB[SA_MODS + 1][QMAX];
static int QN[SA_MODS + 1];
static qev QT[QMAX];                               /* a transform's inner result, copied out of the deeper buffer */

static void qadd(int k, int32_t t, int d, int tone, int oct, int v)
{
    if (QN[k] >= QMAX) return;
    qev *e = &QB[k][QN[k]++];
    e->t = t;
    e->d = (uint8_t)clampi(d, 1, 255);
    e->tone = (uint8_t)tone;
    e->oct = (int8_t)clampi(oct, -4, 4);
    e->v = (uint8_t)clampi(v, 1, 127);
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
            if (c->type == SA_T_DRUM) {
                const dpat *p = &DPAT[c->pat % NDPAT];
                for (int i = 0; i < p->n; ++i) {
                    int32_t t = cy * SA_TPB + p->s[i].t;
                    if (t >= a && t < b) qadd(0, t, 3, 0, 0, p->s[i].v);
                }
            } else {
                const spat *p = &SPAT[c->pat % NSPAT];
                for (int i = 0; i < p->n; ++i) {
                    int32_t t = cy * SA_TPB + p->s[i].t;
                    if (t >= a && t < b) qadd(0, t, p->s[i].d, p->s[i].tone, p->s[i].oct, p->s[i].v);
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
        for (int i = 0; i < n; ++i) qadd(k, fdiv(QT[i].t, x), QT[i].d / x, QT[i].tone, QT[i].oct, QT[i].v);
        break;
    case SA_M_SLOW:
        n = inner(c, m, k, layer, fdiv(a, x), fdiv(b + x - 1, x));
        for (int i = 0; i < n; ++i) {
            int32_t t = QT[i].t * x;
            if (t >= a && t < b) qadd(k, t, QT[i].d * x, QT[i].tone, QT[i].oct, QT[i].v);
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
                if (t >= a && t < b) qadd(k, t, QT[i].d, QT[i].tone, QT[i].oct, QT[i].v);
            }
        }
        break;
    case SA_M_THIN:
        n = inner(c, m, k, layer, a, b);
        for (int i = 0; i < n; ++i)
            if ((int)(hash2((uint32_t)QT[i].t, (uint32_t)layer * 31u + (uint32_t)k) % 100u) >= x * 10)
                qadd(k, QT[i].t, QT[i].d, QT[i].tone, QT[i].oct, QT[i].v);
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
                if (t >= a && t < b) qadd(k, t, e->d < dmax ? e->d : dmax, e->tone, e->oct, e->v);
                j++;
                last = s;
            }
            (void)last;
        }
        break;
    case SA_M_SHIFT: {
        int32_t sh = (x - 8) * SA_TPS;
        n = inner(c, m, k, layer, a - sh, b - sh);
        for (int i = 0; i < n; ++i) qadd(k, QT[i].t + sh, QT[i].d, QT[i].tone, QT[i].oct, QT[i].v);
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
                if (t >= a && t < b) qadd(k, t, d, QT[i].tone, QT[i].oct, QT[i].v);
            }
        }
        break;
    }
    case SA_M_OCT:
        n = inner(c, m, k, layer, a, b);
        for (int i = 0; i < n; ++i) qadd(k, QT[i].t, QT[i].d, QT[i].tone, QT[i].oct + (x - 2), QT[i].v);
        break;
    case SA_M_PLY:
        n = inner(c, m, k, layer, a, b);
        for (int i = 0; i < n; ++i) {
            int d = QT[i].d / x;
            if (d < 1) d = 1;
            for (int r = 0; r < x; ++r) {
                int32_t t = QT[i].t + r * d;
                if (t >= a && t < b) qadd(k, t, d, QT[i].tone, QT[i].oct, QT[i].v - r * 8);
            }
        }
        break;
    default:
        n = inner(c, m, k, layer, a, b);
        for (int i = 0; i < n; ++i) qadd(k, QT[i].t, QT[i].d, QT[i].tone, QT[i].oct, QT[i].v);
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
    if (c->type == SA_T_NONE) return 0;
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
    if (c->type == SA_T_NONE) return;
    qev *e;
    int n = bar_query(c, l, bar, &e);
    int32_t b0 = (int32_t)bar * SA_TPB;
    if (c->type == SA_T_DRUM) {
        int lv = g->s.live[l].level, note = sa_dv_gm(c->sound);
        for (int i = 0; i < n; ++i)
            bn_add(g, (int)(e[i].t - b0), e[i].d, note, e[i].v * (20 + lv * 80 / 100) / 100, l);
        return;
    }
    int poct = SPAT[c->pat % NSPAT].oct;
    for (int i = 0; i < n; ++i) {
        int tick = (int)(e[i].t - b0), oct = poct + e[i].oct;
        if (c->amode == SA_AR_OFF || c->amode >= SA_AR_MODES) {
            if (e[i].tone == SA_CH) {
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
    s->harm.key = 9;                               /* A minor */
    s->harm.scale = SA_SC_MINOR;
    s->harm.prog = 1;
    s->harm.bars = 2;
    for (int l = 0; l < SA_LAYERS; ++l)
        live_default(&s->live[l]);
    code_drum(&s->code[0], SA_DV_KICK, 0);         /* 4 FLOOR */
    code_drum(&s->code[1], SA_DV_HAT, 1);          /* OFFBEAT */
    code_drum(&s->code[2], SA_DV_CLAP, 2);         /* 2 + 4 */
    code_synth(&s->code[3], SA_DEF_BASS, 1, 0);    /* ROLLER */
    code_synth(&s->code[4], SA_DEF_STAB, 13, 1);   /* STAB 2 */
}

void sa_settings_fix(sa_settings *s)
{
    s->bpm = (uint8_t)clampi(s->bpm, 60, 200);
    s->filter = (uint8_t)clampi(s->filter, 0, 100);
    s->feel = (uint8_t)clampi(s->feel, 0, 60);
    s->kit = (uint8_t)clampi(s->kit, 0, SA_KITS - 1);
    s->ktune = (uint8_t)clampi(s->ktune, 0, 100);
    s->ktone = (uint8_t)clampi(s->ktone, 0, 100);
    s->kdecay = (uint8_t)clampi(s->kdecay, 0, 100);
    s->kdrive = (uint8_t)clampi(s->kdrive, 0, 100);
    s->look = (uint8_t)(s->look ? 1 : 0);
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
                c->pat %= (uint8_t)NSPAT;
            }
        }
        if (c->type == SA_T_DRUM) {
            c->part = SA_PART_DRUMS;
            c->sound %= SA_DVS;
            c->pat %= (uint8_t)NDPAT;
        }
        for (int i = 0; i < SA_MODS; ++i) {
            sa_mod *m = &c->mod[i];
            if (m->type >= SA_M_TYPES) m->type = SA_M_OFF;
            for (int r = 0; r < NMODR; ++r)
                if (MODS[r].type == m->type) m->val = (uint8_t)clampi(m->val, MODS[r].lo, MODS[r].hi);
            if (mod_skip(c->type, m->type, m->val)) m->type = SA_M_OFF;
        }
        c->amode = (uint8_t)(c->amode < SA_AR_MODES ? c->amode : 0);
        c->arate = (uint8_t)clampi(c->arate, 0, SA_RATES - 1);
        c->arange = (uint8_t)clampi(c->arange, 1, 4);
        c->agate = (uint8_t)clampi(c->agate, 10, 100);
        v->mute = (uint8_t)(v->mute ? 1 : 0);
        v->cut = (uint8_t)clampi(v->cut, 0, 100);
        v->reso = (uint8_t)clampi(v->reso, 0, 100);
        v->drive = (uint8_t)clampi(v->drive, 0, 100);
        v->env = (uint8_t)clampi(v->env, 0, 100);
        v->level = (uint8_t)clampi(v->level, 0, 100);
    }
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
    g->ed_harm = g->s.harm;
    g->drop_armed = false;
}

bool sa_layer_pending(const sa_groove *g, int l)
{
    return l >= 0 && l < SA_LAYERS && memcmp(&g->ed[l], &g->s.code[l], sizeof g->ed[l]) != 0;
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
    g->s.harm = g->ed_harm;
    g->drop_armed = false;
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

static void bar_line(sa_groove *g)
{
    for (int l = 0; l < SA_LAYERS; ++l) {
        g->hits_last[l] = g->hits[l];
        g->hits[l] = 0;
    }
    if (g->drop_armed) {
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
        if (g->drop_armed) drop_now(g);            /* (stopped: what was armed goes in now) */
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

static void do_tick(sa_groove *g)
{
    if (++g->tick >= SA_TPB) {
        g->tick = 0;
        g->bar++;
        bar_line(g);
    }
    int t = g->tick;
    float tf = tick_frames(g);
    uint32_t delay = 0;
    if (t % 12 == 6 && g->s.feel)                  /* FEEL: the off sixteenths late */
        delay = (uint32_t)(tf * 6.0f * (float)g->s.feel / 100.0f * 0.66f);
    while (g->bn_at < g->nbn && g->bn[g->bn_at].tick <= t) {
        const sa_bar_note *b = &g->bn[g->bn_at++];
        if (b->tick != t) continue;
        const sa_code *c = &g->s.code[b->layer];
        if (c->type == SA_T_NONE || g->s.live[b->layer].mute) continue;
        int part = c->type == SA_T_DRUM ? SA_PART_DRUMS : c->part;
        uint32_t len = c->type == SA_T_DRUM ? (uint32_t)(g->sr * (c->sound == SA_DV_CRASH ? 1.0f :
                                                                  c->sound == SA_DV_OPEN ? 0.4f : 0.3f))
                                            : (uint32_t)((float)b->dur * tf);
        g->hits[b->layer] |= (uint16_t)(1u << (t / SA_TPS));
        if (delay == 0 || g->npend >= SA_PEND) {
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

void sa_process(sa_groove *g, int frames)
{
    if (!g->playing) return;
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
                if (!g->s.live[g->pend[k].layer].mute)
                    note(g, g->pend[k].part, g->pend[k].note, g->pend[k].vel, g->pend[k].len);
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
}

void sa_key(sa_groove *g, int k, int note_in, bool down, int layer)
{
    if (k < 0 || k >= 27) return;
    if (down) {
        if (g->key_note[k] >= 0) emit(g, 0, g->key_part[k], g->key_note[k], 0);
        int part = -1, n = note_in, vel = 104;
        if (layer >= 0 && layer < SA_LAYERS && g->s.code[layer].type == SA_T_DRUM) {
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
