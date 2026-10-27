# 7. Clients sleep fail-closed in short naps

- Status: Accepted
- Date: 2026-07-15 (`214c748`), wake path unified 2026-08-19 (`ed75cda`)

## Context

Lamps run on batteries and the installation is dark during the day. A lamp that misses "wake up" is lost for the night. A lamp that misses "go to sleep" drains its battery. A board that transmits to ask what time it is wakes the mesh.

## Decision

- The master never sleeps. While the fleet should sleep it rebroadcasts `MSG_SLEEP_WAKEUP` (5 min) at 1 Hz.
- A client naps in chunks: Wi-Fi off, `esp_light_sleep_start`, Wi-Fi and ESP-NOW back on, then a 6 s passive listen.
  - Hears a sleep message: sleep again.
  - Hears duration 0, the morning sentinel: wake.
  - Hears nothing: sleep again. "We never transmit to find out what time it is."
  - Hears other master traffic, after the full 6 s window: treat it as morning, or a board would nap forever (`a8e4101`).
- Waking is verified: one reannounce, and the sentinel repeats until every board is back. At least 2 nap cycles, 20-minute cap, 5-minute stall cutoff.
- Animations stop before any sleep broadcast. The master ignores the sleep schedule while its clock reads earlier than epoch 1700000000, so a fresh master that thinks it is 1970 can't put the fleet to bed.
- Sleep state, schedule and manual mode survive master reboots in NVS.
- `sleep_all.sh` writes `sleep_now` or `wake_now` straight into the bridge's music socket, so sleep works even with the web UI or auth broken.

## Consequences

- The worst case is a board that keeps napping, never one that burns its battery transmitting.
- Waking takes up to one nap period (5 min).
- Nap timing accuracy stops mattering, which is what made ADR 8 possible.
