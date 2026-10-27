# 23. Map the fleet with anchor-free 2D SMACOF on the Pi

- Status: Accepted (replaces trilateration on the master)
- Date: 2026-08-19 (`a097101`)

## Context

The master could trilaterate a board from chirps at known spots, but that needs the emitter's coordinates for every spot. "In a forest on a slope those are the one thing you cannot measure."

## Decision

- The operator walks to spread-out spots and chirps. No coordinates.
- `sparkles-api/mapsolve.py` runs bipartite SMACOF over boards and spots, using the `chirp_distance` events. Time of flight fixes the scale in metres. Rotation, translation and mirror stay arbitrary. It fits a constant distance offset, uses random restarts, and normalises the frame. Exposed as `/solveMap`.
- It runs on the Pi, because "they have no business in the mesh's hot path", in pure Python, because it is "not worth a numpy dependency on the installation".
- 2D only. "Solving in 3D is a trap": with near-coplanar spots the 3D fit reported a better residual (0.11 against 0.33) while putting boards 3.9 m out horizontally instead of 0.42 m. "It buys stress with height it cannot see."
- The mirror is resolved by hand. Tap a board on the Map page to blink it and compare with the real space.

## Consequences

- On synthetic forest data (40 boards over 30 x 30 m, 15 cm noise, 20% of measurements dropped) it recovers positions to 0.14 m from 8 spots and 0.17 m from 4. A tight cluster of spots doesn't work, and the residual says so.
- Hanging height is lost. It ends up in the fitted offset.
- The master's trilateration code is still there.
