# 1. ESP-NOW is the lamp network

- Status: Accepted
- Date: 2024-12-17 (`44ba745`, initial commit)

## Context

The installation is up to 200 battery lamps (`NUM_CLIENTS`) spread through a forest, with no router and nothing to plug into. Every lamp is an ESP32-S3. Animations and music need low-latency broadcast, and clock sync and per-board commands need some unicast.

## Decision

All devices talk raw ESP-NOW: no access point, unencrypted peers on the current channel.

- Broadcast: animations, commands, sleep and wake, client announces, chirp bursts, MIDI and shimmer.
- Unicast: `MSG_TIMER` sync (it needs the send ACK to measure TX delay), health replies, OTA requests, per-board resets, `MSG_LOG`.

Real Wi-Fi is used for exactly one thing. "An OTA is the one time a client leaves ESP-NOW for a real AP" (`MyDefines.h`).

## Consequences

- Nothing to set up at a venue. The mesh works whether or not the Pi has Wi-Fi.
- The ESP-NOW peer table holds 20 entries, so unicast peers are added and removed around each send. Since `a8564c8` the table is behind a mutex and pruned on `ESP_ERR_ESPNOW_FULL`. Before that, unicast silently stopped while broadcast carried on.
- The master takes a sender's identity from the radio's source address, not from the payload (`6ab8049`).
- Frames are capped at 250 bytes (ADR 3).
- Clients run with `WiFi.setSleep(false)`, since modem sleep adds RX latency and skews timestamps.

## Tried and dropped

- A master that ran a SoftAP and served the web UI itself (ADR 10).
- The Pi sending ESP-NOW frames itself through ESPythoNOW (ADR 11).
