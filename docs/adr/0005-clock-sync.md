# 5. Clock sync by hardware timestamps and burst median

- Status: Accepted (replaces the running-average sync)
- Date: 2026-06-12 (`15ea5ba`), serial again 2026-09-10 (`27ac32e`)

## Context

Animations are scheduled in master time (ADR 6), and acoustic distance needs clocks good to well under a millisecond (ADR 19): 1 ms is 34 cm of sound. The old sync averaged offsets and corrected by half the round trip.

## Decision

- The master unicasts `MSG_TIMER` every 20 ms. Each packet carries its `sendTime` and `lastDelay`, the measured send-to-ACK delay of the previous packet (from `onDataSent`).
- The client stamps arrival with the radio's hardware RX timestamp, mapped into `esp_timer` time.
- A sample is the previous packet's `sendTime - receiveTime + lastDelay`. It counts only if the sequence is unbroken and `0 < lastDelay < 6000` us. After 10 samples the median becomes the offset and the client sends `MSG_GOT_TIMER`.
- `esp_timer_get_time()` everywhere, never `micros()`, which wraps at 71 minutes (`9623293`).
- One sync at a time. During a sync the master drops everything except the target's `MSG_GOT_TIMER` and handles announces afterwards. A new delay measurement opens only once the previous one has been used.
- The master keeps sending until the board confirms: up to 100 packets, giving up after 6 undelivered (`f939f5a`).
- The chirp and clap devices carry copies of the same algorithm (ADR 2).

## Consequences

- When a client's offset jumps by more than 2 s, the master has rebooted, and the client flushes its queued animations (`1a70666`).
- Syncing a big fleet is serial and takes a while. Calibration therefore resyncs in batches of 20 right before chirping (ADR 22).

## Tried and dropped

- A pool of 5 parallel syncs (`09f4bb3`). Back to serial in `27ac32e`: "Five workers' traffic through one radio doubles the delay each of them then measures... contention biases the clock the sync produces." Measured mean delay 2768 us serial against 5584 us parallel, with 9% against 43% of samples rejected.
- A 100 ms sync period, cut to 20 ms (`20d096c`).
