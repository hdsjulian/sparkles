# 25. Pitch detection with aubio on the Pi

- Status: Accepted
- Date: 2026-06-08 (`54124fa`), levels from the room 2026-08-22 (`d33dc61`)

## Context

Lamps should also react to live sound, a singer or an instrument, not only to MIDI. Pitch detection needs a real audio interface and more CPU than a lamp has.

## Decision

- `pitchdetector/aubioAlgo.py` runs on the Pi as a service: aubio `yinfft`, 1024/256 at 44.1 kHz, sending at 30 Hz into the bridge's music socket (ADR 11).
- Two modes, chosen in the UI. FREQUENCY: pitch drives the shimmer hue, loudness drives brightness, and it only resends on a quarter-tone change, a 10% volume change or every 300 ms. MIDI: pitch becomes a note, loudness a velocity.
- Levels come from the room. It listens for about 1.5 s, puts the gate 8 dB over the median noise, and widens the range when someone is louder: "Swapping a USB mic for an interface and a dynamic mic moves the whole scale." A level set in the UI wins.
- It scores the input devices so it doesn't pick a pseudo-device.
- aubio comes from apt, because it fails to build from source on Python 3.13/aarch64 (`9ecdf81`).

## Consequences

- The pitch detector used to be a separate repo and now lives in `pitchdetector/` (`54124fa`). Its Praat and pygame stubs came along.
- It reads its parameters from `/getMidiParams`, which has to stay public (ADR 13).
