# 6. Animations are scheduled in master time and rendered on the board

- Status: Accepted
- Date: 2024-12-17

## Context

A broadcast reaches different boards at slightly different times, and the mesh can't carry per-frame data for 200 lamps.

## Decision

- The master broadcasts one message per animation with a `startTime` in master microseconds, a little in the future (+100 ms for a blink, +1 s for the idle loop).
- Each board converts it with its sync offset (ADR 5) and runs the animation itself. Hearth, bioluminescence and candle are generated on the board: "nothing is sent per frame, the board owns its own flame". Candle with duration 0 loops until stopped.
- The wait is clamped to 0-5 s so a bad offset can't freeze animation dispatch (`calculateMicrosUntilStart`).
- MIDI and shimmer frames are the exception. They play on arrival and skip the receive queue.
- Output is two RGB LEDs on six LEDC PWM pins at 5 kHz. FastLED is only used for its colour types.
- The idle animation loop is opt-in and not persisted. It is stopped cooperatively, never with `vTaskDelete`, because an async delete "made sleep_now silently kill the master".

## Consequences

- Animations look simultaneous across the forest as long as clocks are synced.
- A new effect means a client OTA, since the board renders it.
- `animationEnum` is append-only (ADR 3).
