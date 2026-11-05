# Sardine manual

Sardine 0.7. Live club mixing on the FM-1: four layers, always there, each one on or OFF: **LEAD**, **CHORD**,
**BASS** and **DRUMS**. A layer is a sound and a pattern, bent by modifiers. Its sound, filter, pan and level act at
once, as you turn; its pattern, steps and modifiers wait, in the second colour, until **SEQ** drops them in on the
one: the next bar line.

Every start is a fresh groove: other sounds, patterns, kit, key and tempo, ready to play. What you want to keep goes
into one of eight sets (SAVE, below).

## Finding your way

| Control | What it does |
| --- | --- |
| HOME | HOME, from anywhere; held: MENU |
| PRESETS | HOME, LEAD, CHORD, BASS, DRUMS: a layer always opens on its home page |
| FX | the layer's next page; past the last, back to its home page |
| SELECT | the tempo, everywhere |
| ALGORITHM | the layer's pattern (OFF first); it waits for SEQ |
| SEQ | drops what waits, on the next bar line (stopped: at once) |
| REC | step recording on the layer, on and off |
| SCL | the key and the chords, again: back |
| SAVE | the sets, again: back |
| PLAY | start, stop |
| OCT- OCT+ | the keys' octave (the one it moved to lit) |

The footer shows the layer, a dot for each of its pages (the one open lit) and what the next press does.

<p align="center">
<img src="screens/home.png" alt="HOME" width="200">
<img src="screens/layer.png" alt="A layer" width="200">
<img src="screens/pending.png" alt="Waiting for SEQ" width="200">
</p>

## HOME

Every layer playing: LEAD, CHORD and BASS with their sound and their bar, DRUMS a thin bar for each drum; an OFF layer
shows its sound dimmed and OFF, a muted one is crossed out. The knobs act at once: **FILTER** (the whole mix darker to the left, OPEN at the top), **FEEL**
(the swing of the off sixteenths), **PUMP** (how deep the synth layers duck under the kick) and **SPACE** (the room:
reverb and delay together; at 70 each sound has its own).

## LEAD, CHORD, BASS

A layer opens on its home page: **CUTOFF**, **RESO**, **DRIVE**, **ENV** (the filter envelope, 50: the sound's own),
at once. ALGORITHM turns its pattern: OFF, then the patterns of its kind (BASS the bass lines, CHORD stabs and pads,
LEAD lead lines and the bass lines). FX turns through its pages:

| Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 | |
| --- | --- | --- | --- | --- | --- |
| SOUND | TYPE (BASS KEYS PADS LEADS FX) | SOUND | PUMP ON / OFF | | at once |
| ARP (CHORD only) | MODE (OFF, UP, DOWN, UP-DOWN, DOWN-UP, CONVERGE, DIVERGE, RANDOM, THUMB, PINKY, STRUM) | RATE | RANGE | GATE | waits for SEQ |
| PAN | MODE (FIXED, SWEEP, PING-PONG, RANDOM) | PAN | WIDTH | RATE (SWEEP) | at once |
| GATE | a ready gate (or OFF) | DEPTH | WIDTH | SMOOTH | waits for SEQ |
| MOD | MOD 1 | MOD 2 | MOD 3 | MOD 4 | waits for SEQ |
| MIX | MUTE | LEVEL (fades it in and out) | | | at once |

<p align="center">
<img src="screens/sound.png" alt="SOUND" width="200">
<img src="screens/arp.png" alt="ARP" width="200">
<img src="screens/pan.png" alt="PAN" width="200">
</p>
<p align="center">
<img src="screens/gate.png" alt="GATE" width="200">
<img src="screens/mod.png" alt="MOD" width="200">
<img src="screens/mix.png" alt="MIX" width="200">
</p>

The arpeggiator is CHORD's: OFF until you turn MODE. It plays the chord of the bar over the notes the pattern holds,
so it is always in key, and swings with FEEL. GATE cuts a held chord to a rhythm by its volume, without a new attack.

## Steps (REC)

<p align="center">
<img src="screens/steps.png" alt="Steps" width="200">
<img src="screens/drums_rec.png" alt="Drum steps" width="200">
</p>

**REC** on a layer turns step recording on (REC lit), again off; another layer turns it off. On LEAD and BASS
**KNOB 1** picks the step, **a key** writes its note there (snapped to the scale) and the cursor moves on, so a line
goes in as fast as you play it; the same note again takes the step off. **KNOB 2** is the step's length. On CHORD a
key puts the chord on the step or takes it off: it plays the chord of the bar. On the GATE page the 16 lowest keys
are the gate's steps. The keys of the scale (or the step keys) glow dim, what is set full. The first edit makes the
pattern the layer's own (MY STEPS); like a pattern, the steps wait for SEQ, and the modifiers still act on them.

## DRUMS

<p align="center">
<img src="screens/drums.png" alt="DRUMS" width="200">
<img src="screens/drums_add.png" alt="+ ADD A DRUM" width="200">
<img src="screens/kit.png" alt="KIT" width="200">
</p>

DRUMS shows the drum KNOB 1 picked, large: which of how many, its pattern and its 16 steps (HOME shows them all). The
home page: **DRUM** (which drum; the last
one, **+ ADD**, adds one), **LEVEL**, **ACCENT** (how hard the accented steps hit against the rest; 50: as written)
and **CHANCE** (how likely each hit plays: 100 % always, lower and it drops out at random). ALGORITHM turns the drum's
pattern: OFF, the patterns, and left of OFF, REMOVE. On + ADD it picks the drum (KICK SNARE CLAP HAT OPEN HAT RIDE
CRASH RIM TOM LOW TOM CONGA COWBELL CLAVE SHAKER); SEQ drops it in. With REC the 16 lowest keys are the drum's steps;
the keys above play it.

FX: **KIT** (KIT, TUNE, TONE, DECAY: the whole kit, every drum plays on it), **MOD** (the drum's modifiers) and
**MIX** (MUTE, LEVEL). The kits: TECH, HOUSE, DEEP, HARD, BOOM, TIGHT, AFRO, DUSTY, CLASSIC, all synthesised.

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

## The key and the chords

<p align="center">
<img src="screens/key.png" alt="KEY" width="200">
</p>

**SCL** opens KEY: **KEY**, **SCALE** (MINOR, DORIAN, PHRYGIAN, MAJOR), **CHORDS** (ten progressions, from ONE
CHORD to four chords) and **BARS** (each chord 1, 2 or 4 bars). The synth layers follow the chord of the bar; the
drums do not. A key change waits for SEQ. SCL again goes back.

## The keys

The keys play the layer open, live, snapped to the scale (on HOME: LEAD); the header says what, briefly (KEYS: LEAD,
SAW LEAD). With REC they write its steps. They never
change a setting.

## SETS

<p align="center">
<img src="screens/sets.png" alt="SETS" width="200">
</p>

**SAVE** opens SETS: eight grooves kept in the flash and **LAST** (L), a row each with its tempo and key; beside the
rows, what the set picked holds (its kit, its synth sounds). **KNOB 1** (or PRESETS) picks a set, **KNOB 2** the action,
**REC** does it:

| Action | What REC does |
| --- | --- |
| LOAD | the set in, on the one while playing (at once when stopped); what waited for SEQ goes |
| STORE | what plays into the set (over what was there) |
| NEW | a fresh groove, as at a start, on the one |
| CLEAR | the set empty |

The flash is written only while stopped: a STORE or CLEAR while playing is kept and written as you stop (the row says
WAITS; one set at a time). SAVE again goes back.

**LAST** keeps itself: three seconds after a change, stopped (and three seconds after you stop), the groove as it is
goes into LAST. It never comes back on its own, but LOAD brings it back. STORE and CLEAR leave it alone. Every set
stays as it is until you STORE over it or CLEAR it, and loads as often as you like. A set stored with 0.6 loads into
the layers by its sounds: a bass sound to BASS, keys and pads to CHORD, leads and FX to LEAD.

## AMBIENT

The last sixteen sounds are for wide, deep club nights: pads (WIDE PAD, DARK PAD, GLASS, VAPOR, SWELL, NIGHT, DRONE
PAD, SKY, TAPE PAD, CHOIR AIR), dub stabs with long tails (DUB STAB, SPACE STAB, ECHO CHORD), RUMBLE (a low bass sent
into the hall), DEEP SUB and AIR LEAD. Each spreads its voices across the stereo field and has its own reverb and
delay. The room every layer plays in is a big hall; the drums send little into it.

## MENU

<p align="center">
<img src="screens/menu.png" alt="MENU" width="200">
</p>

HOME held: the colour (SHOAL by default, and Felucca's) and the look (FLAT, LINE).

## MIDI out

LEAD, CHORD and BASS on channels 1, 2 and 3, the kit on 10 with General MIDI notes.

## Saving

A start is a fresh groove; what you keep goes into SETS, and the last groove into LAST by itself. The colour and the
look are kept.
