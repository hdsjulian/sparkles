# Changelog

## [Unreleased] — 2026-05-29

### Added

#### Chirp-Device firmware (`src/Chirp-Device/`)
- Standalone ESP32-S3 firmware for acoustic distance measurement
- Plays an 8-step pseudo-random chirp (1–2 kHz, 3 ms/step, 44.1 kHz stereo) via PCM5102A I2S DAC (BCLK=GPIO6, WS=GPIO7, DOUT=GPIO16)
- Announces itself to master on boot via `MSG_ADDRESS`; master starts clap-sync delay measurement then follows with a full MSG_TIMER clock-sync sequence
- Timer sync mirrors the client algorithm exactly: accumulates offset over `TIMER_ARRAY_COUNT` samples, corrects for propagation delay, sends `MSG_GOT_TIMER` once converged
- Broadcasts `MSG_CLAP(true/false)` with synced timestamps around the audio playback

#### Timer sync for Chirp-Device (master side)
- `runClapSync` now sends `TIMER_ARRAY_COUNT + 5` `MSG_TIMER` packets to the chirp device immediately after the hardware-delay measurement, using the measured delay as `lastDelay` so the chirp device's offset calculation is accurate from the first sample
- `MSG_GOT_TIMER` handler guards against the chirp device's response: removes the peer and clears `settingTimer` without touching `addressList` or writing to flash

#### Test mode
- New "Test Mode" toggle on the Settings page with a configurable lamp spacing (m) input
- In test mode, MIDI note C4 (MIDI 60) maps directly to client 0, C#4 → client 1, C5 → client 12, etc. (chromatic, no octave matching)
- Each lamp's distance from center is set to `spacing_meters × client_id` on mode activation
- `message_command` gains a `float param` field; `CMD_TEST_MODE_ON` carries the spacing value

#### Chirp cross-correlation on client (`MessageHandler_clientClap.cpp`)
- When in test mode, `runClapTask` records 220 ms of audio at 10 kHz instead of using peak detection
- Normalised cross-correlation against the 8-step chirp template; result below `MIN_CORR_THRESHOLD` (0.15) reports `clapHappened = false`
- Detection timestamp converted to master time domain via `getTimerOffset()` and sent as `MSG_CLAP`

#### Continuous Candle animation
- `CANDLE` animation with `duration = 0` now loops indefinitely: one fade-in, then realistic per-lamp flicker (±25% brightness, occasional gust drop to 60%, 30–90 ms intervals, slight hue wander) until stopped
- New "Candle" card on the Animations page with hue / saturation / brightness controls and a toggle button (turns orange when active)
- `/commandCandleAll` API endpoint; `commandCandleAll()` in `api.js`
- Stopped by the existing "All Off" button

#### `platformio.ini`
- Added `[env:Chirp_Device]` build environment targeting `src/Chirp-Device/`

### Changed
- `LedHandler`: added `setTestMode` / `getTestMode`; MIDI `ledTask` and `runMidi` branch on `testModeActive`
- `MessageHandler_helpers`: `setTestMode` accepts optional `spacingMeters` param, broadcasts `CMD_TEST_MODE_ON` with spacing in `command.param`
- `MessageHandler_client`: `CMD_TEST_MODE_ON` reads spacing, computes and sets `distanceFromCenter`
- `sparkles-ui/src/app.css`: added `btn-warning` (orange) utility class
