# 9. Wi-Fi across naps: bare WiFi.begin, nothing stored

- Status: Accepted
- Date: 2026-07-21 (`1bcc60a`), 2026-08-24 (`7b60d13`)

## Context

Clients turn Wi-Fi off for every nap and back on after (ADR 7). After waking, some boards went deaf or crashed, and finding out why took several wrong turns.

## Decision

- `turnWifiOn()` stays the same as the boot path: STA mode, a bare `WiFi.begin()`, `setSleep(false)`, `esp_now_init`, callbacks and broadcast peer registered again.
- No board stores Wi-Fi credentials. Both OTA paths set `WiFi.persistent(false)` before connecting, and the client erases any stored AP at boot. "A board that talks ESP-NOW has no business holding credentials."
- On warm boot `esp_now_init` is retried up to 10 times, then the board reboots (`333cb8c`).

## Consequences

- The bare `WiFi.begin()` is only harmless because NVS holds no network. Anything that persists credentials on a client brings the post-nap storm back.
- The fix for a stored network is to not store one. Don't add `WiFi.disconnect()` or channel pinning to `turnWifiOn()`.

## Tried and dropped

- `40ae257` stopped `turnWifiOn` from associating with stored Wi-Fi. Reverted in `1bcc60a`: association was not what made boards deaf after a nap. The RTC clock crash was (ADR 8).
- The real stored-network problem turned up later (`7b60d13`). A version-mismatch OTA had written "fogscreen" into NVS on every board, and the bare `WiFi.begin()` hunted for it after every nap, "a storm that only ever appears after waking, never on a cold boot". Fixed by not storing it, with `turnWifiOn` untouched.
