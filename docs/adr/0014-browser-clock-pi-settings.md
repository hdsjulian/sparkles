# 14. The browser is the clock, the Pi holds the settings

- Status: Accepted
- Date: 2026-07-15 (`214b952`, `7311c0d`, `22e6e39`)

## Context

In the field the Pi has no internet, so no NTP, and no battery-backed RTC. The master forgets everything on reboot except what it keeps in NVS. The sleep schedule (ADR 7) needs real wall-clock time.

## Decision

- The only trusted time source is a person's browser. An admin login pushes the browser's time; the kiosk never does. The Pi sets its own clock from it, marks it trusted for this boot, and saves it with fake-hwclock.
- On connect the Pi and the master negotiate. The Pi adopts a clock the master kept through a reboot (its NVS checkpoint), or gives the master its own when the master is blank. An untrusted Pi never pushes time.
- The Pi is the source of truth for settings the master would lose: lamp colours, schedule, speaker delay, air temperature, MIDI and shimmer distance params (`colors.json`, `schedule.json`, `speaker.json`, `air.json`, `midi_params.json`). The bridge sends them again on every connect.
- The master in turn hands a board its distance and MIDI params again whenever that board announces and finishes its sync, since a rebooted client has lost them.

## Consequences

- After a cold start with no admin login, nothing knows the time, and the sleep schedule doesn't run (ADR 7 refuses a clock before late 2023).
- Losing the master loses nothing that matters.
