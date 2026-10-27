# 16. Firmware is built on the Pi, and OTA happens only on request

- Status: Accepted
- Date: 2026-05-28 (`compile.py`), 2026-08-25 (`8967429`), 2026-09-09 (`fcf6109`)

## Context

In the field there is no laptop with PlatformIO, and 200 lamps can't be flashed by cable. Before this, clients fetched firmware from the master's SoftAP at `192.168.4.1`, and any version mismatch started an OTA on its own.

## Decision

- PlatformIO is installed in the Pi's venv. The admin-only `/compile-stream` endpoint (`compile.py`) optionally pulls, bumps the patch version in `MyDefines.h`, builds `Client_Device` into `sparkles-api/firmware.bin`, then builds and flashes `Master_Pi` over USB. The master build is skipped when its sources haven't changed.
- To flash the master, the bridge releases the serial port and takes it back afterwards. The board is reset with esptool's hard-reset pulse (`6584627`).
- OTA is only ever started by a person. A version mismatch is logged, not acted on (`498da5c`, `8967429`): "an update happens when someone asks for one."
- The Pi tells the master which network to join and which URL to fetch, based on the network the Pi is on right now. The master keeps both in RAM only and sends them in `MSG_OTA_REQUEST` to each active board, 2 s apart.

## Consequences

- A board that missed an OTA runs the old firmware until someone updates it, so the wire format has to stay backwards compatible (ADR 3).
- The OTA network has to be one the Pi is on and knows the password for (`_OTA_NETWORKS`). Otherwise nothing is sent, since a board sent to the wrong network leaves the mesh and doesn't come back.
- Boards fetch `/firmware.bin` without a session, so it has to be reachable without a login (ADR 13).
