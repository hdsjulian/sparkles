# 22. Bursts of ten, outliers dropped, physics corrected

- Status: Accepted
- Date: 2026-08-19 (`1b08d21`), corrections 2026-10-09 to 10-13

## Context

"One calibration used to be one chirp, so a single bad correlation was the measurement." Any constant error, whether from the speaker, the air or stale clocks, lands in every distance.

## Decision

- A calibration is a burst of 10 chirps, 500 ms apart. Emitter and clients count windows from the same broadcast, and every reply carries a `chirpIndex`, so a late reply still counts.
- The master drops outliers by median absolute deviation (0.30 m floor) and averages the rest.
- Speaker delay is measured once with a latency test and subtracted from every calibration chirp (`8bca865`): "40.34 ms for the current boombox, stable to about 2 cm across power cycles once it has been on for 4 minutes."
- Speed of sound comes from the air temperature, `331.3 * sqrt(1 + T/273.15)` (`31833aa`). A fixed 343 m/s read 1.7% long at 10 °C, 34 cm at 20 m. The Pi stores the temperature and sends it on every connect. The master keeps it in NVS.
- Boards are measured in batches of 20, each resynced just before its chirps (`c705420`). Clocks drift a few ppm, about 2 cm a minute per ppm, and with 100 boards synced up front the first ones were a minute stale. The start command stays a broadcast so the batch isn't staggered by send time.

## Consequences

- Each spot takes longer to measure.
- The speaker delay has to be measured again when the boombox changes.
- The map solver still fits a constant offset (ADR 23). With the speaker delay subtracted, it should come out near zero.
