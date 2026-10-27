# 27. Muse scores against the visitor's own arrival

- Status: Experimental, only on `feature/muse-headband`
- Date: 2026-10-15 (`ce9a6c7`), lamp curve 2026-10-16 (`a197442`)

## Context

Every head is different, the signals are noisy, and alpha is the one channel a visitor can spike on purpose. The lamps should reward actually settling, not gaming one number.

## Decision

- Each session learns a baseline while the visitor arrives, then freezes it. "A baseline that kept chasing the signal would cancel out the very drift it exists to measure." Spread is measured with MAD, so one jaw clench doesn't set the scale.
- Channels are scored as ratios against that arrival. Stillness, jaw and temple EMG (30-45 Hz on TP9/TP10), alpha and beta are compared in log2, one step being at least a doubling. Heart rate moves in steps of at least 5 bpm.
- The channels a visitor can't fake (stillness, EMG, heart rate) combine by geometric mean: "a visitor sitting perfectly still with a clenched jaw should not be able to average their way to a lit room." Alpha and beta only nudge the result, by up to 15% either way.
- Bias 0.6, above 0.5 so the score isn't a ratchet.
- Lamp curve: a session starts part-lit (0.3), and each update closes a share of the remaining distance up or down. Tuned live with sliders on the Brain page.
- Lifecycle:
  - on a head, the lamps glow at about 5% so the visitor can see they are listening
  - off a head, they go dark after 3 s
  - off for 15 s, the session ends
  - a dropped BLE link gets 60 s of grace, because "a reconnect is not a new visitor"
  - an admin can reset the baseline

## Consequences

- Every 10 s the log has each channel's score, value and arrival level, so a session that won't light up shows which channel held it down.

## Tried and dropped

- A step equal to how much a channel wobbled while the baseline was learned, at least 5% of its level. Stillness barely wobbles, so its step came out at 0.07 mg and the score sat at 0.1 "for a whole session however calm the visitor got" (`ce9a6c7`).
- A linear rise from 0. A visitor who got tenser saw nothing happen (`a197442`).
