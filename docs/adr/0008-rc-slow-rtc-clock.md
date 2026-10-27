# 8. Plain RC_SLOW as the RTC slow clock

- Status: Accepted
- Date: 2026-07-17 (`1922d04`), master 2026-09-09 (`03febf5`)

## Context

Light sleep needs an RTC slow clock. The lamp boards have a 32 kHz crystal, but it is dead on about half of them.

## Decision

Use the default `RC_SLOW` source on clients and on the master. No crystal, no 8MD256, no probing.

From `1922d04`: "Chunked sleep made timing accuracy irrelevant (±3% of a 5-minute nap is seconds, refreshed every rebroadcast), and on a board batch where half the 32k crystals are dead, a marginal 'working' one that stalls mid-nap would strand its board asleep forever... Simplest clock config wins."

## Consequences

- Naps drift a few percent, which the chunked sleep in ADR 7 absorbs.
- When a client fails after a nap, look at the RTC crash breadcrumb and the reset reason first (ADR 9).
- The Clap-Device still probes its crystal.

## Tried and dropped

1. External 32 kHz crystal with a fallback (`6cad855`, 2026-05-22).
2. `RTC_SLOW_FREQ_8MD256` (`d046c47`). It caused `INT_WDT` resets inside `esp_light_sleep_start`: "8MD256 is the known-hazardous source for that wait" (`9cb23bc`).
3. Probing the crystal and using it only if it oscillates (`5a8d5ac`). Dropped in `1922d04`.
4. The master kept 8MD256 until `03febf5`. esptool's RTC watchdog reset was mistimed with it, and the master would not come back after a flash.
