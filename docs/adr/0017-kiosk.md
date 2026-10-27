# 17. The kiosk boots into the public karaoke page

- Status: Accepted
- Date: 2026-06-20 (`a75ae1f`)

## Context

The Pi has a 7" DSI touchscreen in the installation. Visitors use it to play songs that drive the lamps. It runs on battery in a dark forest.

## Decision

- Console autologin, `startx`, Openbox, then a loop that runs `chromium --kiosk http://localhost/karaoke`. The loop restarts Chromium after a crash, and lets `update.sh` reload it with `pkill chromium`.
- `/karaoke` needs no login and no SSE. It polls `/keyboard/status`. A screensaver overlay swallows the tap that wakes it.
- The panel is dimmed to 40 for battery and light spill. Idle blanking uses `xrandr --off`, and a blocking read on the touch device wakes it, because DPMS is unreliable on DSI and the panel ignores `bl_power`.

## Consequences

- Anyone at the screen can play songs and can't reach anything else.
- The kiosk never pushes the clock (ADR 14).
