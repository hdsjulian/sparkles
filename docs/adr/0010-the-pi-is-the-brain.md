# 10. The Pi is the brain, the master is its serial peripheral

- Status: Accepted (replaces the master's onboard web server)
- Date: 2026-05-26 (`d046c47`), committed to on 2026-06-14 (`5f024a1`)

## Context

Until May 2026 the master ESP32 ran a SoftAP and an async web server, served the UI from LittleFS and pushed events with `AsyncEventSource`. UI, logic, Wi-Fi and the mesh all shared one chip. The installation was also growing MIDI, pitch detection, map solving and a touchscreen.

## Decision

- A Raspberry Pi 5 runs the API and the UI. The master ESP32 hangs off it on USB CDC and stays the only ESP-NOW authority, clock sync included.
- They talk newline-delimited JSON at 115200: `{"cmd": ...}` from the Pi, `{"event": ...}` from the master. The spec is `sparkles-api/protocol.md`. An unknown command gets an `unknown_cmd` reply, because "a silently ignored command is indistinguishable from a line that never arrived".
- One writer at each end of the line:
  - On the master, all output goes through a ring buffer to one task on core 1 (`7a5c32e`). Concurrent prints "cut the 64-byte packet the interrupt was still filling".
  - The master builds with `CORE_DEBUG_LEVEL=0` because its serial line is the protocol. Its logs go over the mesh to a Log-Device with `LOG_I`. Anything that must always be seen is a JSON event.
  - On the Pi, `serial_bridge.py` inside the FastAPI process owns the port. Other local processes (aubio, keyboard, `sleep_all.sh`) write JSON lines to `/tmp/music.sock` and the bridge forwards them.
- The bridge matches replies to requests by event name, returns 503 when the master is gone and 504 on timeout, reconnects every 3 s, and treats a mangled line as one bad line rather than a lost port.
- `5f024a1` deleted `lib/WebServer` and the old master. Endpoint names were kept 1:1 with the old web server, so the UI's API did not change.

## Consequences

- The ESP32 does only mesh work. Everything heavy runs on Linux.
- Control needs the Pi. Without it the lamps keep doing what the master last told them and the master keeps its sleep schedule (ADR 7), but nothing can be changed.
- The Pi does not speak ESP-NOW itself (ADR 11).
- Client OTA is served by the Pi (ADR 16).

## Tried and dropped

- `serial_mux.py` as a separate daemon owning the port (`2ecc712`). Removed in `abe55e9` when music went back to one device (ADR 11).
