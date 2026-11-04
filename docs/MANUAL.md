# Sardine manual

Sardine 0.1. Live club mixing on the FM-1: a stack of layers, each a line of code you build with the knobs. A layer
is a sound and a pattern, bent by modifiers and, on a synth layer, an arpeggiator. You change the code, and REC drops
your changes in on the one: the next bar line. The sound of a layer (its filter, drive, level) and the mutes act at
once, as you turn.

## The screens

SELECT goes through them: **MAIN**, then one screen per layer, then **NEW** (a free slot for a new layer).

<p align="center">
<img src="screens/main.png" alt="MAIN" width="200">
<img src="screens/layer.png" alt="A layer" width="200">
<img src="screens/pending.png" alt="Waiting for REC" width="200">
</p>

**MAIN** shows the whole stack: every layer, what it plays this bar, the muted ones crossed out. Its knobs act at
once: **TEMPO**, **FILTER** (the whole mix darker to the left, OPEN at the top), **FEEL** (the swing of the off
sixteenths; every layer follows it) and **KIT** (the drum kit).

**A layer** has two halves. The knobs below are its sound, at once:

| Layer | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- |
| synth | CUTOFF | RESO | DRIVE | ENV (the filter envelope, 50: the sound's own) |
| drum | LEVEL | TUNE | TONE | DECAY |

A drum layer's TUNE, TONE and DECAY move the whole kit: every drum layer plays on it.

The panel is its code, as chips: **SOUND**, **PATTERN**, up to four **modifiers**, **+** (room for one more) and, on
a synth layer, **ARP**. PRESETS picks a chip, ALGORITHM changes it. A changed chip turns the second colour, the header
counts what waits (**2 TO DROP**), the panel shows the bar as it plays and as it will after REC. Press **REC**: the
header says **ON THE 1** and REC blinks; on the next bar line it all goes in at once. Stopped, REC drops at once.

**FX** mutes the layer open (at once; again: back in). Turning SOUND down past its first entry says **REMOVE**: the
layer goes on the drop.

## The modifiers

They act in order, each on what the ones before it made, and on time itself, not on single steps:

| Modifier | What it does |
| --- | --- |
| FAST 2 3 4 | the pattern 2, 3 or 4 times in a bar |
| SLOW 2 3 4 | the pattern over 2, 3 or 4 bars |
| REV | the bar backwards |
| EVERY 2..8: REV | every 2nd .. 8th bar backwards |
| THIN 10..90 % | leaves that many of the notes out (the same ones each time) |
| EUCLID 1..15/16 | that many hits spread as evenly as they go over the bar, the pattern's notes on them |
| SHIFT -8..+7 | the pattern later or earlier by sixteenths |
| LEN 3..15 | the first 3..15 sixteenths, over and over: against a 16-step layer it drifts and comes back |
| OCT -2..+2 | octaves (synth layers) |
| PLY 2 3 4 | each note 2, 3 or 4 times in its own length |

## The arpeggiator

On a synth layer, **ARP** opens its page: **MODE** (UP, DOWN, UP-DOWN, DOWN-UP, CONVERGE, DIVERGE, RANDOM, THUMB,
PINKY, STRUM), **RATE** (1/4 to 1/32, with 1/8 and 1/16 triplets), **RANGE** (1 to 4 octaves) and **GATE**. It plays
the chord of the bar over the notes the pattern holds, so it is always in key; it swings with FEEL. ARP again goes
back. The arpeggiator is code too: it drops with REC.

<p align="center">
<img src="screens/arp.png" alt="ARP" width="200">
<img src="screens/key.png" alt="KEY" width="200">
<img src="screens/new.png" alt="NEW" width="200">
</p>

## The key and the chords

**SCL** opens the KEY page: **KEY**, **SCALE** (MINOR, DORIAN, PHRYGIAN, MAJOR), **CHORDS** (ten progressions, from
ONE CHORD to four chords) and **BARS** (each chord 1, 2 or 4 bars). Every synth layer follows the chord of the bar;
the drums do not. A key change is code: it drops with REC, on the one. SCL again goes back.

## A new layer

Turn SELECT past the last layer: **NEW**. KNOB 1 picks its sound: the drums first (KICK SNARE CLAP HAT OPEN HAT RIDE
CRASH RIM TOM LOW TOM CONGA COWBELL CLAVE SHAKER), then the synth sounds (BASS, KEYS, PADS, LEADS, FX: 65 of them).
Any number of drum layers play on the kit; up to three synth layers, each with its own sound and filter (the FM-1
runs four parts). With three synth layers, a new one can only be a drum. Up to twelve layers in all.

## The keys

The keys play the layer open, live: a synth layer snapped to the scale, a drum layer its drum. On MAIN they play the
first synth layer. OCT- and OCT+ move their octave. The keys never change a setting.

## The kits

All synthesised: TECH, HOUSE, DEEP, HARD, BOOM, TIGHT, AFRO, DUSTY, CLASSIC.

## MENU

HOME held: the colour (SHOAL by default, and Felucca's) and the look (FLAT, LINE).

## MIDI out

The synth layers' parts on channels 1, 2 and 3, the kit on 10 with General MIDI notes.

## Saving

The layers and the settings save themselves two seconds after the last change while stopped, never while playing
(writing the flash would stall the audio). They live in Felucca's first song slot.
