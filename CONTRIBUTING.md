# Contributing to Sardine

Sardine is a small fork. Bugs and ideas: [Issues](https://github.com/delirioux/sardine/issues) on this repository,
please; not Felucca's, Tuna's or Sill's trackers: Sardine is maintained here.

With a bug, say the Sardine version (the splash), the layers you had and what they did, the steps to reproduce it,
what you expected and what happened. A change to the patterns, the modifiers or the arpeggiator goes in
`firmware/src/sa_layer.c`, to the sounds and kits in `sa_sounds.h`. The keys only ever play music. `sh tests/sa_tests.sh`
must pass.
