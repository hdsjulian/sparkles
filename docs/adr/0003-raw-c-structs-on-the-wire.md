# 3. Raw C structs on the wire

- Status: Accepted
- Date: 2024-12-17, compatibility rules 2026-05-26 (`d046c47`)

## Context

ESP-NOW frames are at most 250 bytes. Every device is the same chip built by the same toolchain. The fleet is never all on one firmware: boards miss OTAs, sit in a box, come back months later.

## Decision

- One `message_data` struct: type, target MAC, sender MAC, receive time, then a `union message_payload`. Copied with `memcpy`, no serialisation layer. All of it lives in `MyDefines.h`.
- `MSG_*` numbers identify the message. `CMD_*` numbers ride inside `message_command`.
- Compatibility is kept by rules in the code, not by a schema:
  - `animationEnum` is append-only, clients decode it by number.
  - `static_assert`s keep the frame under 250 bytes and stop new payloads from growing `sizeof(message_data)`.
  - `ESPNOW_CLIENT_COMPAT_SIZE = 80`. Clients built before `message_log` existed check `len == 80` strictly, so the master sends exactly 80 bytes for everything except `MSG_LOG` and `MSG_OTA_REQUEST`. Receivers now accept any length up to `sizeof(message_data)`.
- The firmware version is packed into a `uint32` (`lib/Version`) and travels in the `MSG_ADDRESS` announce. A mismatch is logged and nothing more (ADR 16).

## Consequences

- No parsing cost and no dependency on the hot path.
- Reordering or resizing a payload breaks every board that has not been updated yet.
- Changing `client_address` also invalidates the master's stored address file (ADR 4).
