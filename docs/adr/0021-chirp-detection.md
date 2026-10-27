# 21. Clients find the chirp on the correlation's envelope

- Status: Accepted
- Date: 2026-10-13 (`9fe6d94`, `5279103`)

## Context

Each lamp hears the chirp through a cheap electret mic read with `analogRead`, in a forest full of echoes. Detection runs on the ESP32 in the 500 ms between chirps.

## Decision

- Each client records a 300 ms window at 10 kHz, removes the mic's DC offset, and correlates against the template and its 90 degree twin. The envelope is `sqrt(c^2 + q^2)`.
- No normalisation. A window counts when its peak is at least 6 times the median lag. The earliest local peak at half the strongest or more wins, refined with a parabolic fit.
- The search starts at the emitter's announced emission time, not at a fixed offset into the window.
- It costs about 40 ms (~660k multiply-adds) on the board, and only a timestamp goes over the air.
- A board that heard nothing flashes blue once. A heard chirp is two red flashes.

## Consequences

- The direct sound beats the echoes: 12 times the median at 4 m and 37 at 1 m, where noise reaches about 3.5.
- `Client_Diag` (`-DCHIRP_DIAG=1`) prints what the old rules would have picked and dumps the raw recording. It is for one board on a cable, not the fleet.

## Tried and dropped

- Normalised correlation with a 0.15 gate and the first peak above 70% (`24ab697`). The raw correlation ripples at the chirp's pitch, and the earliest peak "came one wave (0.43 ms, 15 cm) early in 13 to 31 of 50 chirps" (`9fe6d94`).
- Normalising each lag by its own energy, with a mean + 5 sigma gate. It "made each faint, chirp-shaped echo score like the direct sound", and at 4 m board 0 rejected every chirp (`5279103`).
- A fixed skip at the start of the window, which "made close boards invisible".
