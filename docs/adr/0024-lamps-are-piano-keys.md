# 24. Lamps are piano keys by index

- Status: Accepted
- Date: 2024-12-17 (`44ba745`), test mode 2026-05-29 (`24ab697`)

## Context

A keyboard player or a song should light the forest so the music is visible. A piano has 88 keys and there are up to 200 lamps.

## Decision

- A lamp's note is its index in the address list (ADR 4) plus 21, so lamp 0 is A0. A lamp lights for any note of its pitch class, 20% dimmer per octave away from its home octave, with a small hue and saturation shift.
- Decay runs from 8 s for low notes to 2 s for high ones, scaled by velocity. The sustain pedal (CC64) halves the decay rate.
- Measured position and distance from centre are not used for MIDI.
- Test mode maps chromatically: note 60 + n lights lamp n. It also sets `distanceFromCenter = spacing * n`, so distance effects can be tried without calibrating.
- Live keys, karaoke and MIDI file playback all take the same path. With no keyboard connected, songs play to the lamps only.
- Songs for the single stage piano are thinned out with `piano_only.py` and `play_midi.py`, since full arrangements "turn to mush".

## Consequences

- Which lamp plays which note depends on announce order, and removing a board shifts every board after it.
- Mapping notes to places in the forest would mean using the map from ADR 23. That hasn't been done.
