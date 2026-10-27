# 11. One ESP32 carries both control and live music

- Status: Accepted
- Date: 2026-07-14 (`abe55e9`)

## Context

Live music (a MIDI keyboard, karaoke playback, aubio pitch detection) produces about 30 frames a second that have to reach the lamps with low latency. The question was where those frames enter the mesh. It took four designs in six weeks.

## Decision

The master ESP32 carries them.

- Pi-side producers write JSON lines to the bridge's `/tmp/music.sock`. The bridge stamps the current hue and saturation into every frame (`22e6e39`), so clients never render stale colours.
- The master broadcasts music frames straight away, bypassing its send queue. Clients put MIDI and shimmer frames straight into the LED queue.
- To keep the master from choking: a 2048-byte RX buffer, non-blocking TX, the housekeeping dump skipped while music plays, NVS clock writes deferred until 30 s after the last note. The idle loop resumes 4 minutes after the last MIDI.

## Consequences

- One USB device, one serial owner, no device-identity handshake.
- Music and mesh management share one radio and one CPU. A busy music evening is where master stalls would show up first.
- `src/Music-Device`, `src/Raspi-Device` and their envs are leftovers.

## Tried and dropped

1. `Raspi-Device` (2025): an ESP32 next to the Pi that read CSV pitch over serial and broadcast ESP-NOW.
2. The Pi sending ESP-NOW itself with ESPythoNOW on a monitor-mode USB dongle (`64f0540`, `feature/raspi-espnow`, 2026-06-05). It needed an AR9271 dongle, a patched `ath9k_htc` driver and `message_data` packed by hand in Python. Replaced three days later by JSON over serial to the master (`54124fa`).
3. A dedicated Music-Device ESP32 on a second USB port behind `serial_mux` (`6591c0c`, 2026-06-18), "so the 30Hz media stream stops competing with the master's sync/management work (the likely cause of the master hangs)". Reverted in `abe55e9`, which has no message body. The same day `2cb2156` fixed `getLocalTime` blocking for 5 s and tripping the loop watchdog. That may have been the real cause of the hangs, but no commit says so.
