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
| EDIT | the layer's pattern editor, again: back |
| ENV | held: a drum fill (below) |
| ARP | held: BREAK (below) |
| LFO | held: SWEEP (below) |
| GLO | GLOBAL: the MIXER, each press its next page (MUTE, SOLO, STYLE), past the last back |
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
(the swing of the off sixteenths), **PUMP** (how deep the synth layers duck under the kick; each layer's own share on its PUMP page) and **SPACE** (the room:
reverb and delay together; at 70 each sound has its own). **FX** on HOME turns the knobs over to **RESO** (resonance on the whole mix's
filter, over each synth layer's own: ride FILTER down with it up for the filter-house sweep) and **WIDTH** (the stereo
spread, 70: each sound's own); FX again back.

## LEAD, CHORD, BASS

At the top of a layer's pages its sound as it plays, a wave (a flat line when it is silent), beside its pattern (the
second colour under it: what SEQ drops). A layer opens on its home page: **CUTOFF**, **RESO**, **DRIVE**, **ENV** (the filter envelope, 50: the sound's own),
at once. ALGORITHM turns its pattern: OFF, then the patterns of its kind (BASS the bass lines, CHORD stabs and pads,
LEAD lead lines and the bass lines). FX turns through its pages:

| Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 | |
| --- | --- | --- | --- | --- | --- |
| MIX (the first) | VOLUME (fades it in and out) | TYPE (BASS KEYS PADS LEADS FX) | INSTR (the sound, by name; the header says it whole) | MUTE | at once |
| ARP | MODE (OFF, UP, DOWN, UP-DOWN, DOWN-UP, CONVERGE, DIVERGE, RANDOM, THUMB, PINKY, STRUM) | RATE | RANGE | GATE | waits for SEQ |
| PAN | MODE (FIXED, SWEEP, PING-PONG, RANDOM) | PAN | WIDTH | RATE (SWEEP) | at once |
| 3D | MODE (OFF FIXED ORBIT RANDOM FLY) | ANGLE (FIXED) or RATE (ORBIT, FLY) | HEIGHT | DIST | at once |
| PUMP | DEPTH (of HOME's PUMP) | RELEASE | SOURCE (KICK, BEAT) | PUMP ON / OFF | at once |
| GATE | a ready gate (or OFF) | DEPTH | WIDTH | SMOOTH | waits for SEQ |
| MOD | MOD 1 | MOD 2 | MOD 3 | MOD 4 | waits for SEQ |

<p align="center">
<img src="screens/mix.png" alt="MIX" width="200">
<img src="screens/arp.png" alt="ARP" width="200">
<img src="screens/pan.png" alt="PAN" width="200">
</p>
<p align="center">
<img src="screens/gate.png" alt="GATE" width="200">
<img src="screens/mod.png" alt="MOD" width="200">
</p>

<p align="center">
<img src="screens/3d.png" alt="3D" width="200">
</p>

**3D** places the layer around you, for headphones: from above (FRONT up) the dot is the layer, near you or far; beside
it, from the side, its height; the dot moves as the layer does. **MODE**: FIXED (where ANGLE says), ORBIT (round you
over RATE, a quarter bar to four bars), RANDOM (each note somewhere else round you) and FLY (an orbit rising and
falling about HEIGHT). Turning ANGLE, HEIGHT or DIST from OFF turns it on, FIXED. On, the angle pans the layer (the PAN
page rests), behind and below sound darker and a little softer, above brighter, far quieter, darker and wetter, and
the layer narrows to a point. These are cues the ear reads, not a measured head model: left, right and far come across
best; front against back and up against down are subtler and differ from one listener to the next. On speakers it is
panning and tone.

<p align="center">
<img src="screens/pump.png" alt="PUMP" width="200">
</p>

**PUMP** is how this layer ducks: HOME's PUMP is the whole mix's amount, DEPTH this layer's share of it. A sound's
type sets its DEPTH when you pick it: PADS 100 %, KEYS 80 %, BASS 50 %, LEADS 10 %, FX 0. RELEASE is how fast it comes
back (50: over most of a beat), SOURCE whether it follows the kick layer's hits (KICK: mute the kick and it stops) or
every beat (BEAT). The drums never duck.

Every synth layer has an arpeggiator: OFF until you turn MODE. It plays the chord of the bar over the notes the pattern holds,
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

## EDIT

<p align="center">
<img src="screens/edit.png" alt="EDIT" width="200">
<img src="screens/edit_drum.png" alt="EDIT, a drum" width="200">
</p>

**EDIT** on a layer opens its pattern editor: its 16 steps, the step **KNOB 1** is on framed. **KNOB 2** is **FILL**:
CURR (the pattern as it was when you opened EDIT) and ready fills that rewrite the steps; turn back to CURR and the
pattern is as it was. On DRUMS: ROLL (the last beat in sixteenths), BUILD, SNARE, BREAK (the first half only), STUTTER,
GALLOP, FLAM and DOUBLE, over the drum's own hits or in place of them. On LEAD and BASS ready lines in the key: ROOT
8THS, ARP UP, ARP DOWN, OCTAVES, WALK, RIFF, CALL, PULSE 16. On CHORD the chord's rhythm: STABS, OFFBEATS, PUSH,
PULSE, 8THS, 16THS, DOTTED, HOLD. **KNOB 3** is the step's LENGTH (on DRUMS its HIT, on or off).

With **REC** on, LEAD, BASS and CHORD: a key writes at the framed step (the note snapped to the scale, the same note
again takes it off; on CHORD the chord) and the frame moves on. DRUMS: the 16 lowest keys are the steps, glowing dim,
the hits set full; a key turns its step on or off, the keys above play the drum. Without REC the keys play the layer,
lit with what it plays. Like a pattern, the steps wait for SEQ. EDIT again goes back.

## DRUMS

<p align="center">
<img src="screens/drums.png" alt="DRUMS" width="200">
<img src="screens/drums_add.png" alt="A row turned on" width="200">
<img src="screens/kit.png" alt="KIT" width="200">
</p>

Seven rows, always there, each on a drum channel of its own, so no two ever cut each other off:

| Row | INSTR |
| --- | --- |
| KICK | KICK |
| SNARE | SNARE |
| CLAP | CLAP |
| HATS | HAT, OPEN HAT, SHAKER |
| TOMS | TOM, LOW TOM, CONGA |
| RIM | RIM, CLAVE |
| METAL | RIDE, CRASH, COWBELL |

Every drum comes in eight variations, all on the INSTR knob: as the kit has it, **TIGHT** (short and bright), **ROOM**
(long and darker), **HIGH**, **LOW**, and the same drum from three other drum machines (**80**, **10**, **DEEP**),
whatever the kit: CLAP, CLAP TIGHT, CLAP ROOM .. CLAP DEEP, then (on HATS) OPEN HAT, OPEN HAT TIGHT and so on. The
header says the whole name as it turns. The METAL row's (ride, crash, cowbell) are all darker and softer than the machines make them. The claps are their own: all eight short hand claps (from the 80, 55, 77 and
66 machines and DEEP; CLAP 77 the 80 and 10 swapped for the 77), whatever the kit.

Each row shows its drum and its bar (the second colour: waiting for SEQ); a row OFF is dimmed. The home page: **DRUM**
(KNOB 1, the row), **INSTR** (its drum, at once), **VOLUME** and **ACCENT** (how hard the accented steps hit against
the rest; 50: as written). ALGORITHM turns the row's pattern, OFF first: a pattern turns the row on, OFF turns it off;
SEQ drops it in. Without REC every key plays the row's drum; with REC the 16 lowest keys are its steps, the keys above
play it.

FX: **MIX** (VOLUME, KIT, INSTR, MUTE), **KIT** (KIT, TUNE, TONE, DECAY: the whole kit, every drum plays on it) and
**MOD** (three modifiers, and **CHANCE**: how likely each hit plays, 100 % always; lower and hits drop out at random,
so the pattern changes as it plays). The kits, all synthesised: TECH, HOUSE, DEEP, HARD, BOOM, TIGHT, AFRO, DUSTY,
CLASSIC, ROOM, VINTAGE, MINIMAL, VOLT (electro), GARAGE, DISCO, BREAKS, DUB, LOFI, IRON (industrial), ACID.

## MIXER

<p align="center">
<img src="screens/mixer.png" alt="MIXER" width="200">
</p>

**GLO** opens the MIXER: a fader for each layer, LEAD, CHORD, BASS and DRUMS (all the drums together), on the four
knobs, at once (80: as it is); the dot under a fader lights as the layer plays. **GLO** again turns the knobs over to their
MUTE, GLO again to their **SOLO** (turn right: that layer, with any others soloed, alone; left: back), GLO again
to STYLE (below), GLO once more back. SOLO is a live move: not kept with the groove. A layer's VOLUME here is the same as on its own first FX page.

## The drum fill

Hold **ENV**: from the next beat the drums give way to a fill (ENV blinks until the beat, then stays lit), for as
long as you hold it; let go and the groove is back at once, with a crash on the next beat. A fill is a bar: started
on beat 3 it plays from its third beat, so it always peaks on the bar's last sixteenth. Each time another, by turns:
ROLL (the snare in sixteenths, rising, 32nds at the end), TOMS (a run from the high tom down), BUILD (eighths, then
sixteenths, then 32nds, an open hat on top), STUTTER (kick and hat on every sixteenth, claps), FLAM (flammed
backbeats over ghost notes), SYNCO (3-3-2 accents, toms to end), CLAPS (claps building to 32nds) and GHOST (ghost
notes and rim around a flammed snare). They play the SNARE row's drum; the synth layers play on.

## BREAK and SWEEP

Hold **ARP** for a **BREAK**: the drums and the bass drop out at once, LEAD and CHORD play on. Let go and ARP blinks
until the next 1, where everything comes back in: the drop. Hold **LFO** for a **SWEEP**: the whole mix's filter closes
over two bars (as far as FILTER at about 15), its resonance rising as it goes; let go and it is open again at once. Together with the fill on ENV: the
build, the fill and the drop.

## STYLE, INTRO and ARC

<p align="center">
<img src="screens/style.png" alt="STYLE" width="200">
</p>

**GLO** four times: **STYLE**, **INTRO**, **ARC**. **KNOB 1** picks a style, **SEQ** makes a fresh
groove in it, on the next 1 (stopped: at once): its tempo, kit, scale, swing, pump, room and filter, a bass, a chord
and now and then a lead of its kind, its drums.

| Style | | Style | |
| --- | --- | --- | --- |
| NIGHT | slow, dark, filtered (98-104) | DISCO | bright, strings and bass (116-120) |
| FILTER | French touch, filtered disco (122-126) | AFRO | afro house, percussive (120-124) |
| DEEP | warm, soft deep house (120-122) | GARAGE | 2-step, swung (130-134) |
| TECH | tech house, rolling (124-127) | BREAKS | broken beats, rave (128-132) |
| MINIMAL | sparse, clicky (124-126) | DUB | dub techno, echoes (118-122) |
| TECHNO | dark, driving (128-132) | AMBIENT | space, barely a beat (88-96) |
| HARD | hard techno (140-148) | ELECTRO | robotic, broken (126-130) |
| GABBER | hardcore, distorted (170-180) | HAPPY | bright, euro, hands up (138-142) |
| ACID | a 303 line (128-134) | LOFI | lo-fi house, dusty (116-120) |
| TRANCE | uplifting, wide (136-140) | PROG | progressive, building (122-124) |

**INTRO** (OFF, 4, 8, 16 bars): from PLAY the layers come in one by one, the kick first, the other drums at a quarter,
BASS at half, CHORD at three quarters, LEAD at the end, the filter opening over it. **ARC** (OFF, 16, 32, 64 bars):
after the intro a cycle, the groove for half, a break (drums and bass out) for a quarter, a build (the filter closing)
for the last quarter with a roll on its last half bar, and the drop on the next 1. Both are kept with the groove.

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
SAW LEAD). On LEAD, CHORD and BASS the keys light up with the notes the layer plays (a bass line folded up into
the keys' reach). On DRUMS every key plays the drum picked, and its own note (its General MIDI note, moved into
the keys' reach) is lit. With REC they write its steps. They never
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

HOME held: the colour (SHOAL by default, and Felucca's) and the look (FLAT, LINE). Below them the **CPU**: the audio's load
now (its average over a tenth of a second; above 85 % it turns the second colour), its PEAK since the start, and how
many voices the load guard had to let go (SHED), if any.

## MIDI out

LEAD, CHORD and BASS on channels 1, 2 and 3, the kit on 10 with General MIDI notes.

## Saving

A start is a fresh groove; what you keep goes into SETS, and the last groove into LAST by itself. The colour and the
look are kept.
