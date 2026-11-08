# The road to 1.0

Sardine 0.8.40 has everything planned for 1.0. What is left is listening on the FM-1 and tuning what it shows.

## To check on the FM-1

- PERC: tambourine, bongo, wood block, cowbell, cabasa, zap, in all their variations
- Phrases: bar 3's lift and bar 4's answer on LEAD, CHORD and BASS; PHRASE OFF
- PART B: switching on the phrase from HOME's ALGORITHM, A/B by itself every 8 and 16 bars, B CHORDS
- SONG: the steps in turn, each set on its bar line, empty and END steps passed
- BREAK held over four bars: the drums back one by one
- The LEAD after one pattern, its polyphony (a chord on the keys), and the lead sounds that lost their glide
  (HOOVER, VOX LEAD, WHISTLE, AIR LEAD) and SUPER LEAD's unison
- PAN 3D: behind no longer lost
- The detune on the chord and pad sounds (TRANCE, RAVE, STRINGS, SOFT PAD and the rest at 12 to 16 cents)
- ARP LEAD's sound
- The style leads (SOFT LEAD, FILTER LEAD, BLIP, DARK SAW, ACID LEAD, RUBBER, PLUCK LEAD, RAVE LEAD, SCREECH, DUB
  LEAD, STRING LEAD, PAN FLUTE, VOX CHOP): levelled by numbers only (tools/sa_lead_levels.mjs), not by ear; the
  vibrato's depth and speed
- EVOLVE LIGHT and MORE over a long A/B run: does it stay the same track, moving? NEW on HOME (SEQ held)
- STYLE MIX: the fades (smooth, no clicks), the kept bass in the new key's chords (a fifth up, or the relative key), the tempo glide, the bass swap
  at the end; GLO's tap now acting on letting go
- The RAVE leads (STORM, TWISTER, BUZZSAW, SIREN, ZAPPER, WOBBLER, POWER, HYPERSAW, TORNADO, STROBE): their
  movement in time with the tempo, the gates starting open, the levels (by numbers only)
- The MIXER's scope of the whole mix and the synth layers' scope
- The load: everything playing at once, with 3D, PUMP and the arpeggiators on

## To tune by ear

- How much ENV and the filter move a sound
- How each of the twenty styles feels
- The fills, the filter dip into the 1 after them, the SWEEP
- The new kits HYPNO, RUMBLE, PUNCH, SOFT, their kick following the chords (the jumps between roots: musical, or
  too much?), and which styles now take them
- LIVE: KICK, PAD, EMPTY; SEQ bringing each layer in; BUILD by itself; the order it builds in
- MIDI OUT: each layer on its channel (1 2 3 10 at every start), OFF, the log; the drums' GM notes in a DAW; CC7, CC74
  and CC10 following the mix in a DAW (and not flooding it during a SWEEP or 3D ORBIT)
- The held moves: WASH, THROW, TAPE STOP, WOBBLE (and that a tap still does the button's job)

## Known limits

- RAM: 68.8 of 73.7 KB in use; a big new feature needs room found first
- No cymbals, for good: synthesised, they never sound right; the PERC row is short percussion

## Later

- MIDI clock in and out (USB and the TRS jack), [Felucca](https://github.com/hugelton/Felucca)'s midi_clock.c: in time with other gear
- More moves from Felucca's PERFORM: REPEAT 1/8 1/16 1/32, REVERSE, FREEZE, OCT UP / DN, FLANGER, PHASER, CRUSH; a
  MOVES page on GLO picks what each held button plays

## Not planned

- Bluetooth (audio and MIDI): the latency and the system's overhead are too much for a live box; MIDI is over USB
- Samples: the drums stay synthesised
- Motion recording (knob moves into the pattern)
