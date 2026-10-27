# Architecture decision records

Why sparkles is built the way it is. One file per decision.

Each record has a status, a date, the context, the decision, its consequences and, where it matters, what was tried and dropped. Commit hashes are from the current history (after the 2026-06-12 rewrite).

This first batch was written in October 2026 from the code and the git history. Decisions from before May 2026 have little written reasoning behind them, so their context is reconstructed from the code. Later ones quote the commit messages.

Statuses: Accepted, Experimental, Superseded by N.

To add one: copy [template.md](template.md), take the next free number, add it to the list below.

## Mesh and firmware

| # | Decision | Status |
|---|---|---|
| 1 | [ESP-NOW is the lamp network](0001-espnow-is-the-lamp-network.md) | Accepted |
| 2 | [One build env per device role](0002-one-build-env-per-device-role.md) | Accepted |
| 3 | [Raw C structs on the wire](0003-raw-c-structs-on-the-wire.md) | Accepted |
| 4 | [The master owns the address list](0004-the-master-owns-the-address-list.md) | Accepted |
| 5 | [Clock sync by hardware timestamps and burst median](0005-clock-sync.md) | Accepted |
| 6 | [Animations are scheduled in master time and rendered on the board](0006-animations-scheduled-in-master-time.md) | Accepted |
| 7 | [Clients sleep fail-closed in short naps](0007-fail-closed-sleep.md) | Accepted |
| 8 | [Plain RC_SLOW as the RTC slow clock](0008-rc-slow-rtc-clock.md) | Accepted |
| 9 | [Wi-Fi across naps: bare WiFi.begin, nothing stored](0009-wifi-across-naps.md) | Accepted |

## Pi and master

| # | Decision | Status |
|---|---|---|
| 10 | [The Pi is the brain, the master is its serial peripheral](0010-the-pi-is-the-brain.md) | Accepted |
| 11 | [One ESP32 carries both control and live music](0011-one-esp32-for-control-and-music.md) | Accepted |
| 12 | [FastAPI with SSE, and a static SvelteKit app behind nginx](0012-pi-web-stack.md) | Accepted |
| 13 | [Auth: rules in git, credentials on the Pi](0013-auth.md) | Accepted |
| 14 | [The browser is the clock, the Pi holds the settings](0014-browser-clock-pi-settings.md) | Accepted |
| 15 | [Offline deploy with git bundles](0015-offline-deploy.md) | Accepted |
| 16 | [Firmware is built on the Pi, and OTA happens only on request](0016-firmware-from-the-pi.md) | Accepted |
| 17 | [The kiosk boots into the public karaoke page](0017-kiosk.md) | Accepted |
| 18 | [Alerts over Meshtastic LoRa](0018-meshtastic-alerts.md) | Accepted |

## Calibration and mapping

| # | Decision | Status |
|---|---|---|
| 19 | [Distance by acoustic time of flight from a chirp device](0019-acoustic-time-of-flight.md) | Accepted |
| 20 | [The chirp emitter never stops its I2S clock](0020-chirp-emitter-clock.md) | Accepted |
| 21 | [Clients find the chirp on the correlation's envelope](0021-chirp-detection.md) | Accepted |
| 22 | [Bursts of ten, outliers dropped, physics corrected](0022-measurement-protocol.md) | Accepted |
| 23 | [Map the fleet with anchor-free 2D SMACOF on the Pi](0023-anchor-free-map.md) | Accepted |

## Music

| # | Decision | Status |
|---|---|---|
| 24 | [Lamps are piano keys by index](0024-lamps-are-piano-keys.md) | Accepted |
| 25 | [Pitch detection with aubio on the Pi](0025-pitch-detection-aubio.md) | Accepted |

## Muse headband

| # | Decision | Status |
|---|---|---|
| 26 | [Muse headband over BLE into the music path](0026-muse-over-ble.md) | Experimental |
| 27 | [Muse scores against the visitor's own arrival](0027-muse-scoring.md) | Experimental |
