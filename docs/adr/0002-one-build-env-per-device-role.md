# 2. One build env per device role

- Status: Accepted
- Date: 2024-12-17, refined through 2026

## Context

One repo builds firmware for very different boards: the master, the lamp clients, a clap device, a chirp emitter, a log sniffer, and some test and blink rigs. They share the wire format and some logic but little else.

## Decision

- `platformio.ini` has one `[env:*]` per role, and `build_src_filter` picks one `src/<Role>-Device/` directory. The shared `[env]` pins pioarduino `51.03.07`.
- `-DDEVICE_MODE` selects the role inside the shared libs (0 master, 1 client, 3 clap/chirp, 4 raspi, 6 music; see `MyDefines.h`). Master-only and client-only `MessageHandler_*.cpp` files compile out for the other role.
- `-DDEVICE_USED` selects the board revision's pin map. Every env uses 5 (V4).
- `lib/MessageHandler` and `lib/LedHandler` serve master and client. Auxiliary devices (chirp, clap, log, blink, music) include only `MyDefines.h` for the wire format and are otherwise self-contained, duplicating what they need. Chirp and clap each carry a copy of the client's timer sync, named the same so it diffs cleanly.
- `Master_Pi` is the env the Pi builds and flashes (ADR 16). It adds explicit `-I` paths for the libs.

## Consequences

- An auxiliary device can change or break without touching the fleet firmware.
- A change to the sync algorithm has to be made in three places: client, chirp, clap.
- Diagnostic variants are cheap. `Client_Diag` extends `Client_Device` with `-DCHIRP_DIAG=1`.
- `Music_Device`, `Music_Pi` and `Raspi-Device` are leftovers (ADR 11). `Music_Pi` still has the `platform = espressif32` override that `5c97e02` removed from `Master_Pi` because it pulled an older arduino-esp32 core.
