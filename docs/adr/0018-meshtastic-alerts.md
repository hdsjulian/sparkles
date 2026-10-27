# 18. Alerts over Meshtastic LoRa

- Status: Accepted
- Date: 2026-05-27 (`7c72fa4`), on the main line 2026-07-14 (`abe55e9`)

## Context

The installation runs unattended in a forest without Wi-Fi or mobile data. Someone needs to know when a lamp's battery is low or the system falls over.

## Decision

- An optional Meshtastic T-Beam on the Pi sends short text alerts over LoRa to a handheld T-Deck through the `meshtastic` CLI: battery low, heap low, installation active, karaoke song.
- The serial bridge's `HealthMonitor` reports boards lost and back, and critical batteries.
- `mesh_heartbeat.sh` sends a periodic alive beacon.
- Configured with `SPARKLES_MESH_BIN` and the Meshtastic port in `sparkles.service`.

## Consequences

- Monitoring works with no network at all, at LoRa's tiny bandwidth: alerts, not logs.
- With no T-Beam connected, nothing happens.

## Tried and dropped

- `raspi/lora_log_bridge.py` on `feature/lora-meshtastic-bridge` (`8b59b66`), which tailed `journalctl` and sent every log line. Never merged. Short alerts replaced it.
