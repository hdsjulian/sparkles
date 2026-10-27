# 20. The chirp emitter never stops its I2S clock

- Status: Accepted
- Date: 2026-09-09 (`ac3a478`), idle offset 2026-10-12 (`36a19f2`)

## Context

A distance is only as good as its emission timestamp: 1 ms is 34 cm. The PCM5102A gets no MCLK here. It recovers its clock from BCLK with a PLL, and soft-unmutes over seconds every time its clock starts.

## Decision

- The I2S channel is enabled once in `setup()` and never disabled.
- One pump task is the only writer. It writes idle signal continuously and splices chirps into the stream.
- Emission time is arithmetic: enable time + frames written / 44100, counting the frames already preloaded into the DMA ring, converted to master time where it is used.
- Between chirps the stream holds a constant +64 offset. That keeps the DAC awake without making a sound.
- Philips I2S format. The chirp can go out on one stereo channel only, since two speakers apart blur the arrival (`91d5b7a`).
- A status LED: blue for a message from the master, green when a chirp went to the DAC (`d4cd696`).

## Consequences

- Every chirp in a burst comes out at the same level, at a sample-exact time.
- Nothing else may write to the I2S stream. `978f5da` fixed a second writer.

## Tried and dropped

1. Stamp, then write. Every distance read long by the 33 ms DMA ring (`1b08d21`, fixed in `bf9c72e`).
2. Gating the channel around each chirp. What came out was the PLL settling, not the chirp (`8d42a97`).
3. Enabling once per burst. The first seven chirps of a burst were inaudible while the DAC unmuted (`ac3a478`).
4. Idle signal: zeros let the DAC sleep, ±1 LSB kept it only partly awake, and a 22 kHz dither worked until "a boombox with a digital input turned it into a loud hum" (`36a19f2`).
5. MSB-justified I2S, which correlated at zero (`3389591`).
6. Stamps in raw emitter uptime instead of local time. Arrivals were rejected as four hours off (`832be92`).
