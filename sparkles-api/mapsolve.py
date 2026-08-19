"""Anchor-free map solve.

Every chirp burst gives each board its distance to the spot the emitter stood on.
Nobody has to measure where that spot was: with enough spots the whole
constellation — boards and spots alike — is pinned by the distances themselves,
up to rotation, translation and a mirror flip. Scale is already absolute,
time-of-flight gives real metres.

That leaves the operator with the one job a forest actually allows: walk to
spots that are spread out, and chirp. No coordinates.

Runs on the Pi rather than the master: a few hundred unknowns and a few thousand
iterations are nothing here, and they have no business in the mesh's hot path.
Pure Python on purpose, this is not worth a numpy dependency on the installation.
"""

import math
import random
import time

MIN_SPOTS_PER_BOARD = 3          # a 2D fix needs three, fewer leaves the board floating
# Chirps are stamped when queued to I2S, not when they leave the cone, so every
# distance reads long by whatever the DMA ring holds. That ring is 6 descriptors
# of 240 frames at 44.1 kHz — 33 ms, or 11 m of air — so the ceiling has to clear
# that. It was 5 m, which silently pinned the fit and bent the map instead.
OFFSET_LIMITS = (-1.0, 15.0)


class MapMeasurements:
    """The distance matrix as it arrives from the master's chirp_distance events."""

    def __init__(self):
        self.distances = {}      # (boardId, slot) -> metres
        self.updated = 0.0

    def record(self, board_id: int, slot: int, distance: float):
        key = (int(board_id), int(slot))
        if distance is None or distance <= 0:
            self.distances.pop(key, None)   # board missed this spot
        else:
            self.distances[key] = float(distance)
        self.updated = time.time()

    def clear_slot(self, slot: int):
        """A re-run of a spot replaces it, it does not add to it."""
        for key in [k for k in self.distances if k[1] == int(slot)]:
            del self.distances[key]
        self.updated = time.time()

    def clear(self):
        self.distances.clear()
        self.updated = time.time()

    def boards(self):
        return sorted({k[0] for k in self.distances})

    def slots(self):
        return sorted({k[1] for k in self.distances})

    def summary(self):
        boards = self.boards()
        slots = self.slots()
        per_board = {b: sum(1 for s in slots if (b, s) in self.distances) for b in boards}
        per_slot = {s: sum(1 for b in boards if (b, s) in self.distances) for s in slots}
        return {
            "boards": len(boards),
            "slots": len(slots),
            "measurements": len(self.distances),
            "perBoard": per_board,
            "perSlot": per_slot,
            "ready": len(slots) >= 4 and sum(1 for c in per_board.values() if c >= MIN_SPOTS_PER_BOARD) >= 3,
            "updated": self.updated,
        }


def _dist(a, b):
    return math.sqrt(sum((a[k] - b[k]) ** 2 for k in range(len(a))))


def _stress(boards, spots, pairs, offset):
    total = 0.0
    for bi, si, d in pairs:
        err = _dist(boards[bi], spots[si]) - (d - offset)
        total += err * err
    return total


def _sweep(boards, spots, by_board, by_spot, offset, dim):
    """One SMACOF (Guttman) pass over boards then spots.

    Each point moves to the average of "where each of its measurements says it
    should be": the neighbour's position pushed out along the current bearing by
    the measured distance. Monotonically decreasing, no step size to tune.
    """
    for i, plist in enumerate(by_board):
        if not plist:
            continue
        acc = [0.0] * dim
        for j, d in plist:
            target = d - offset
            delta = [boards[i][k] - spots[j][k] for k in range(dim)]
            norm = math.sqrt(sum(x * x for x in delta)) or 1e-9
            for k in range(dim):
                acc[k] += spots[j][k] + target * delta[k] / norm
        boards[i] = [acc[k] / len(plist) for k in range(dim)]

    for j, plist in enumerate(by_spot):
        if not plist:
            continue
        acc = [0.0] * dim
        for i, d in plist:
            target = d - offset
            delta = [spots[j][k] - boards[i][k] for k in range(dim)]
            norm = math.sqrt(sum(x * x for x in delta)) or 1e-9
            for k in range(dim):
                acc[k] += boards[i][k] + target * delta[k] / norm
        spots[j] = [acc[k] / len(plist) for k in range(dim)]


def _best_offset(boards, spots, pairs):
    """Least-squares additive offset: what every distance reads too long by.

    This is the speaker latency (chirps are stamped when queued to I2S, not when
    they leave the cone). It is identifiable because a constant shifts every
    distance equally while the geometry can only scale them.
    """
    measured = sum(d for _, _, d in pairs)
    model = sum(_dist(boards[bi], spots[si]) for bi, si, _ in pairs)
    offset = (measured - model) / len(pairs)
    return max(OFFSET_LIMITS[0], min(OFFSET_LIMITS[1], offset))


def _refine(boards, spots, pairs, dim, fit_offset, iterations, offset=0.0, tolerance=1e-9):
    """Sweep to convergence, refitting the offset as the shape settles."""
    by_board = [[] for _ in boards]
    by_spot = [[] for _ in spots]
    for bi, si, d in pairs:
        by_board[bi].append((si, d))
        by_spot[si].append((bi, d))

    previous = None
    for step in range(iterations):
        _sweep(boards, spots, by_board, by_spot, offset, dim)
        if fit_offset and step % 5 == 4:
            offset = _best_offset(boards, spots, pairs)
        current = _stress(boards, spots, pairs, offset)
        if previous is not None and previous - current < tolerance * max(previous, 1.0):
            break
        previous = current
    if fit_offset:
        offset = _best_offset(boards, spots, pairs)
    return offset, _stress(boards, spots, pairs, offset)


def _fix_gauge(boards, spots, dim, flip):
    """Pin the arbitrary rotation/translation so repeated solves look the same.

    The distances cannot tell us the orientation, so the choice is ours: centre
    the boards, lay their longest axis along x. The mirror is a coin toss the
    data cannot settle either — flip it by hand once you recognise two boards.
    """
    count = len(boards)
    centre = [sum(p[k] for p in boards) / count for k in range(dim)]
    for group in (boards, spots):
        for p in group:
            for k in range(dim):
                p[k] -= centre[k]

    # longest axis of the board cloud onto x, in the xy plane
    sxx = sum(p[0] * p[0] for p in boards)
    syy = sum(p[1] * p[1] for p in boards)
    sxy = sum(p[0] * p[1] for p in boards)
    angle = 0.5 * math.atan2(2.0 * sxy, sxx - syy)
    cos_a, sin_a = math.cos(-angle), math.sin(-angle)
    for group in (boards, spots):
        for p in group:
            x, y = p[0], p[1]
            p[0] = x * cos_a - y * sin_a
            p[1] = x * sin_a + y * cos_a

    if flip:
        for group in (boards, spots):
            for p in group:
                p[1] = -p[1]

    # deterministic sign: the first board sits on the +x side
    if boards and boards[0][0] < 0:
        for group in (boards, spots):
            for p in group:
                p[0] = -p[0]
                p[1] = -p[1]


def solve(measurements: MapMeasurements, dim: int = 2, fit_offset: bool = True,
          flip: bool = False, restarts: int = 12, iterations: int = 400, seed: int = 1):
    """Recover board and spot positions from the distance matrix alone.

    Returns positions plus the numbers that say whether to believe them: the RMS
    residual overall and per board, how many spots each board actually heard, and
    the fitted distance offset.

    Stay at dim=2. Solving in 3D is a trap and the API does not offer it: the
    emitter walks a surface, so the spots are near coplanar and a board's height
    is barely constrained. On synthetic forest data (boards hung 2-6 m up, 10%
    slope, 15 cm noise) the 3D solve reported a *better* residual than 2D — 0.11
    against 0.33 — while putting boards 3.9 m out horizontally instead of 0.42 m,
    and it stretched a 3.9 m spread of hanging heights into 27 m. It buys stress
    with height it cannot see. More restarts made it worse, not better; this is
    the geometry, not the optimiser. Flat is the honest answer, and the height
    mismatch lands harmlessly in the fitted offset.
    """
    board_ids = measurements.boards()
    slot_ids = measurements.slots()
    if len(slot_ids) < dim + 2 or len(board_ids) < dim + 1:
        return {
            "ok": False,
            "reason": f"need at least {dim + 2} spots and {dim + 1} boards, "
                      f"have {len(slot_ids)} spots and {len(board_ids)} boards",
        }

    board_index = {b: i for i, b in enumerate(board_ids)}
    slot_index = {s: j for j, s in enumerate(slot_ids)}
    pairs = [(board_index[b], slot_index[s], d) for (b, s), d in measurements.distances.items()]

    # unknowns: every position, minus the rigid motions the data cannot see
    unknowns = dim * (len(board_ids) + len(slot_ids)) - (dim * (dim + 1)) // 2
    if len(pairs) <= unknowns:
        return {
            "ok": False,
            "reason": f"{len(pairs)} measurements cannot pin {unknowns} unknowns, chirp from more spots",
        }

    scale = sorted(d for _, _, d in pairs)[len(pairs) // 2] or 10.0

    # Cheap heats first, then run the two best to convergence. Random starts land
    # in wildly different local minima and the hopeless ones show it early, so
    # there is no point converging all of them.
    heats = []
    for attempt in range(restarts):
        rng = random.Random(seed + attempt)
        boards = [[rng.uniform(-scale, scale) for _ in range(dim)] for _ in board_ids]
        spots = [[rng.uniform(-scale, scale) for _ in range(dim)] for _ in slot_ids]
        # Vary where the offset search starts as well as the shape. Refitting it
        # from zero every time drags a large latency into a bad basin: the shape
        # settles around the wrong distances before the offset can catch up.
        start_offset = OFFSET_LIMITS[1] * (attempt % 4) / 4.0 if fit_offset else 0.0
        offset, stress = _refine(boards, spots, pairs, dim, fit_offset,
                                 max(40, iterations // 6), offset=start_offset)
        heats.append((stress, attempt, boards, spots, offset))
    heats.sort(key=lambda h: h[0])

    best = None
    for stress, _, boards, spots, offset in heats[:2]:
        offset, stress = _refine(boards, spots, pairs, dim, fit_offset, iterations,
                                 offset=offset, tolerance=1e-11)
        if best is None or stress < best[3]:
            best = (boards, spots, offset, stress)

    boards, spots, offset, stress = best
    _fix_gauge(boards, spots, dim, flip)

    per_board = {}
    for bi, si, d in pairs:
        err = _dist(boards[bi], spots[si]) - (d - offset)
        entry = per_board.setdefault(board_ids[bi], {"count": 0, "sumSq": 0.0})
        entry["count"] += 1
        entry["sumSq"] += err * err

    board_out = []
    for i, board_id in enumerate(board_ids):
        entry = per_board.get(board_id, {"count": 0, "sumSq": 0.0})
        count = entry["count"]
        board_out.append({
            "boardId": board_id,
            "x": round(boards[i][0], 3),
            "y": round(boards[i][1], 3),
            "z": round(boards[i][2], 3) if dim > 2 else 0.0,
            "spots": count,
            "residual": round(math.sqrt(entry["sumSq"] / count), 3) if count else None,
            "trusted": count >= MIN_SPOTS_PER_BOARD,
        })

    spot_out = [{
        "slot": slot_ids[j],
        "x": round(spots[j][0], 3),
        "y": round(spots[j][1], 3),
        "z": round(spots[j][2], 3) if dim > 2 else 0.0,
    } for j in range(len(slot_ids))]

    spread = max((_dist(spots[a], spots[b])
                  for a in range(len(spots)) for b in range(a + 1, len(spots))), default=0.0)

    return {
        "ok": True,
        "dim": dim,
        "boards": board_out,
        "spots": spot_out,
        "rms": round(math.sqrt(stress / len(pairs)), 3),
        "offset": round(offset, 3),
        "measurements": len(pairs),
        "unknowns": unknowns,
        "spotSpread": round(spread, 2),
        "untrusted": sum(1 for b in board_out if not b["trusted"]),
        "note": "positions are up to a mirror flip, pass flip=true if the map comes out handed wrong",
    }
