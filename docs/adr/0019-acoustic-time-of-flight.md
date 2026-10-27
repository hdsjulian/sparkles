# 19. Distance by acoustic time of flight from a chirp device

- Status: Accepted (replaces the clap for distance calibration)
- Date: 2026-05-29 (`24ab697`), emitter clock check 2026-09-09 (`0e1457c`)

## Context

Effects like light following sound outward need each lamp's distance from a point, and the map (ADR 23) needs distances from several points. A forest offers no tape measures and no useful GPS, but every lamp has a microphone.

Before this, distance came from a clap. A Clap-Device with a contact switch stamped the moment of contact, and clients found the bang with a z-score peak detector. A hand clap has no repeatable waveform to correlate against and no precise emission time.

## Decision

- A separate ESP32-S3 Chirp-Device plays a known chirp through a PCM5102A I2S DAC into a boombox's line-in.
- The emitter stamps when the chirp left, in master time. Each client stamps when it heard it, in master time. The master subtracts and converts to metres. Both clocks come from ADR 5.
- The chirp is 8 steps of 3 ms between 1 and 2 kHz in a fixed pseudo-random order (`1000, 1700, 1200, 2000, 1500, 1100, 1800, 1300`), well clear of voice and wind and easy for the cheap mics. Emitter and clients take it from `MyDefines.h`, and it has to stay identical on both sides.
- The device announces itself as `MSG_SOUND_DEVICE`, and keeps re-announcing so a master that rebooted can still find it.
- Before a burst the master checks the emitter's clock against its own and refuses if it is more than 50 ms off, since "its chirp timestamps would be meaningless" (`0e1457c`).
- Flight times over 200 ms are thrown out.

## Consequences

- The boombox adds a fixed latency, which is measured and subtracted (ADR 22).
- The old position calibration with a hand clap still exists alongside it.

## Tried and dropped

- Emission time taken as the master's receive time minus half a measured round trip, before `15ea5ba`. A fallback for it is still in the code.
- A 400 ms chirp (50 ms steps), a warning chirp and a separate ChirpTest-Device on `feature/chirp-device` (2026-06-01 to 06-05). Never merged.
