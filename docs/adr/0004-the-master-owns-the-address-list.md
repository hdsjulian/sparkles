# 4. The master owns the address list

- Status: Accepted
- Date: 2024-12-17, revised 2026-07-16 (`e600b97`) and 2026-08-19 (`ed75cda`)

## Context

Lamps need a stable identity for the map, the music (ADR 24) and the UI. They are identical hardware and get swapped, recharged and rebooted all the time.

## Decision

- The master keeps `addressList[NUM_CLIENTS]`. A lamp's index in that list is its identity. A new board gets the first empty slot when it announces (`addOrGetAddressId`).
- Clients don't store their index. They learn it from `MSG_TIMER.addressId` on every sync.
- The list is written raw to LittleFS `/clientAddress` after confirmed syncs, calibrations and removals. LittleFS formats itself if it fails to mount.
- Removing a board compacts the list, since "a hole in the middle would hide every board after it", and resyncs the shifted boards so they learn their new index (`e600b97`).
- On boot the master restores the list with every board inactive and does nothing else: no reannounce, no sync sweep (`ed75cda`, reversing `59191ba`). Boards come back by announcing.
- Small master state goes in NVS (Preferences), not LittleFS: clock checkpoint, sleep schedule and state, manual mode, speaker delay, speed of sound. It "survives reboots without the pi" (`e5333a1`).

## Consequences

- One authority, nothing to reconcile.
- Identity is positional. Removing a board renumbers every board after it, which also changes which note it plays.
- The file is a raw struct dump, so any change to `client_address` makes every stored file unreadable. `generate_clientAddress.py` still packs the old 140-byte layout (144 since `d592b4f`), and `update_master.sh` fetches from the old master web server. Both are stale.
