"""
muse.py — Muse 2 headband → values you can drive lights with.

Connects to a Muse 2 over BLE, decodes the raw EEG / PPG / IMU streams itself
(no LSL, no muselsl) and emits a handful of normalised numbers a few times a
second:

  settle                        0..1, how far this visitor has actually settled
  delta/theta/alpha/beta/gamma  relative band power, each 0..1, summing to 1
  calm / focus                  auto-ranged 0..1 — the two worth mapping to hue
  motion / pitch / roll         head movement from the accelerometer
  blink                         frontal blink event, good for a one-off flash
  bpm                           heart rate from the PPG sensor (--ppg)
  contact                       how many of the 4 electrodes look attached

`settle` is the one meant to drive an installation. Putting the headband on
starts a session; for the first --anchor seconds nothing happens while the
visitor's own arrival level is learned, and after that the score integrates
slowly toward 1 for as long as they keep genuinely calming down.

It is deliberately hard to fake. Alpha is the one channel a visitor can spike
on purpose — close your eyes and it jumps within a second — so alpha only
modulates the score by ±15%. What actually drives it is the evidence nobody can
fake by wanting to: jaw and temple tension (EMG), physical stillness, and heart
rate against their own arrival baseline. Those are combined with a geometric
mean, so one strong channel cannot carry the others. Closing your eyes moves
almost nothing; closing your eyes and truly settling moves everything, which is
the correct answer.

Install:
    pip install bleak numpy

Run:
    python muse.py --scan          # find the headband, print its address
    python muse.py                 # live meters in the terminal
    python muse.py --json          # one JSON object per line, for piping

The headband must not be connected to the Muse phone app at the same time, and
on macOS the terminal needs Bluetooth permission (System Settings → Privacy).
"""

import argparse
import asyncio
import fcntl
import json
import logging
import math
import os
import queue
import re
import shutil
import signal
import struct
import sys
import threading
import time
from collections import deque

import numpy as np
from bleak import BleakClient, BleakScanner

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s %(levelname)s %(message)s",
)
log = logging.getLogger("muse")
writer = None          # set in main(); stdout never written from the event loop

EEG_FS = 256.0          # EEG sample rate, fixed by the headband
PPG_FS = 64.0           # PPG sample rate
ACC_FS = 52.0           # accelerometer sample rate
RETRY_DELAY = 3.0
KEEPALIVE = 5.0         # must beat the headband's ~7.5s control-idle timeout
CONNECT_TIMEOUT = 20.0  # BlueZ can sit in Connect() long past bleak's own timeout
LOCK_PATH = "/tmp/sparkles-muse.lock"
LOCK_WAIT = 12.0        # a session that was just stopped may still be tearing down
BLUETOOTHCTL = "bluetoothctl"
SETUP_TIMEOUT = 20.0    # subscribing to seven characteristics, one at a time

EEG_LSB      = 0.48828125   # µV per count, 12 bits over a 2 mVpp range
ACCEL_SCALE  = 0.0000610352 # g per count
GYRO_SCALE   = 0.0074768    # deg/s per count

# Contact thresholds, on the 2-44Hz amplitude (see channel_amplitude). These
# are starting guesses, not measurements — tune them with --flat-uv/--railed-uv
# against what your own headband actually reports.
FLAT_UV    = 5.0    # below this the electrode isn't on skin
RAILED_UV  = 900.0  # above it something is very wrong with the contact
BLINK_UV   = 110.0  # frontal excursion that counts as a blink
BLINK_HOLD = 0.30   # refractory, seconds

BANDS = {
    "delta": (1.0, 4.0),
    "theta": (4.0, 8.0),
    "alpha": (8.0, 13.0),
    "beta":  (13.0, 30.0),
    "gamma": (30.0, 44.0),
}

# Jaw/temple muscle tension. Sits below mains hum so no notch filter is needed,
# and the temporal electrodes sit right over the temporalis.
EMG_BAND = (30.0, 45.0)
TEMPORAL = ("TP9", "TP10")

SETTLE_CONTACT = 2      # electrodes needed before a session counts as running
SESSION_DROP = 15.0     # seconds of bad contact before the visitor is gone
ALPHA_INFLUENCE = 0.3   # how far the fakeable channels can move the score
# Evidence needed to hold the level, where 0.5 is exactly how the visitor
# arrived. Above 0.5 so the score is not a ratchet: drifting back to your
# arrival state has to let the light recede, or the room stays lit for someone
# who settled once and then went back to fidgeting.
EVIDENCE_BIAS = 0.6      # overridden by --bias in main


def _uuid(short: int) -> str:
    return f"273e{short:04x}-4c4d-454d-96be-f03bac821358"


CONTROL   = _uuid(0x0001)
EEG_CHANS = {"TP9": _uuid(0x0003), "AF7": _uuid(0x0004),
             "AF8": _uuid(0x0005), "TP10": _uuid(0x0006)}
ACCEL     = _uuid(0x000A)
TELEMETRY = _uuid(0x000B)
PPG_IR    = _uuid(0x0010)   # infrared channel, cleanest pulse of the three

FRONTAL = ("AF7", "AF8")

parser = argparse.ArgumentParser(description="Muse 2 → light-control values")
parser.add_argument("--address",  default=None,
                    help="BLE address/UUID of the headband (default: scan for one)")
parser.add_argument("--scan",     action="store_true",
                    help="List nearby Muse headbands and exit")
parser.add_argument("--json",     action="store_true",
                    help="Emit one JSON object per line instead of meters")
parser.add_argument("--rate",     type=float, default=10.0,
                    help="Output updates per second (default 10)")
parser.add_argument("--window",   type=float, default=2.0,
                    help="FFT window in seconds (default 2.0, longer = steadier)")
parser.add_argument("--smooth",   type=float, default=0.3,
                    help="EMA factor for band powers, 0..1 (lower = smoother)")
parser.add_argument("--ppg",      action="store_true",
                    help="Also stream PPG and report heart rate")
parser.add_argument("--anchor",   type=float, default=40.0,
                    help="Seconds of a new session spent learning the visitor's "
                         "arrival level, during which settle stays 0 (default 40)")
parser.add_argument("--rise",     type=float, default=120.0,
                    help="Seconds of perfect evidence to go from 0 to full settle")
parser.add_argument("--fall",     type=float, default=40.0,
                    help="Seconds to fall back to 0 when the visitor tenses up")
parser.add_argument("--bias",     type=float, default=0.6,
                    help="Evidence needed to hold the level, where 0.5 is exactly "
                         "how they arrived (default 0.6 — lower is easier to earn)")
parser.add_argument("--flat-uv",  type=float, default=FLAT_UV,
                    help=f"Below this 2-44Hz amplitude an electrode counts as "
                         f"detached (default {FLAT_UV})")
parser.add_argument("--railed-uv", type=float, default=RAILED_UV,
                    help=f"Above this it counts as bad contact (default {RAILED_UV})")
parser.add_argument("--preset",   default=None,
                    help="Override the headband preset (default p50 with --ppg, else p21)")
parser.add_argument("--no-reconnect", action="store_true",
                    help="Exit on disconnect instead of retrying")
parser.add_argument("--soak",     type=float, default=None, metavar="MINUTES",
                    help="Run for this long, then stop and print the stability "
                         "summary — the number to compare before and after a change")
args = parser.parse_args()

PRESET = args.preset or ("p50" if args.ppg else "p21")
EVIDENCE_BIAS = args.bias
FLAT_UV = args.flat_uv
RAILED_UV = args.railed_uv


def _cmd(text: str) -> bytes:
    """Muse control frames are a length byte, the ASCII command, then newline."""
    return bytes([len(text) + 1, *text.encode("ascii"), 0x0A])


def _unpack_eeg(packet: bytes):
    """20 bytes: uint16 packet index, then 12 samples packed 12 bits each."""
    bits = int.from_bytes(packet[2:20], "big")
    raw = [(bits >> (12 * (11 - i))) & 0xFFF for i in range(12)]
    return [(v - 2048) * EEG_LSB for v in raw]


def _unpack_imu(packet: bytes, scale: float):
    """20 bytes: uint16 index, then 3 samples of int16 x/y/z."""
    vals = struct.unpack(">9h", packet[2:20])
    return [tuple(v * scale for v in vals[i:i + 3]) for i in (0, 3, 6)]


def _unpack_ppg(packet: bytes):
    """20 bytes: uint16 index, then 6 samples of uint24."""
    return [int.from_bytes(packet[2 + 3 * i:5 + 3 * i], "big") for i in range(6)]


class AutoRange:
    """Maps a value to 0..1 against a slowly-decaying observed min/max, so the
    output uses the whole LED range instead of hugging the middle."""

    def __init__(self, decay=0.9995):
        self.lo = None
        self.hi = None
        self.decay = decay

    def __call__(self, x: float) -> float:
        if self.lo is None:
            self.lo = self.hi = x
        self.lo = min(self.lo, x)
        self.hi = max(self.hi, x)
        mid = 0.5 * (self.lo + self.hi)          # let the envelope relax back in
        self.lo = mid + (self.lo - mid) * self.decay
        self.hi = mid + (self.hi - mid) * self.decay
        span = self.hi - self.lo
        return 0.5 if span < 1e-9 else min(1.0, max(0.0, (x - self.lo) / span))


class Stability:
    """The numbers that say whether the link holds: how much of the time it was
    up, how often it fell over, and why connects failed. Eyeballing log lines
    across runs never settled that; one uptime figure does."""

    def __init__(self):
        self.started = time.monotonic()
        self.attempts = 0
        self.failures = {}          # "phase: reason" -> count
        self.sessions = []          # length of every link, seconds
        self.drops = 0              # links the headband or BlueZ ended, not us
        self.stalls = 0
        self._up = 0.0
        self.up_since = None

    def attempt(self):
        self.attempts += 1

    def failed(self, phase, exc):
        reason = str(exc).strip() or type(exc).__name__
        key = f"{phase}: {reason[:60]}"
        self.failures[key] = self.failures.get(key, 0) + 1

    def connected(self):
        self.up_since = time.monotonic()

    def disconnected(self, dropped):
        if self.up_since is not None:
            length = time.monotonic() - self.up_since
            self._up += length
            self.sessions.append(length)
            self.up_since = None
        if dropped:
            self.drops += 1

    def up_total(self, now):
        return self._up + (now - self.up_since if self.up_since is not None else 0.0)

    def summary(self):
        now = time.monotonic()
        total = now - self.started
        up = self.up_total(now)
        lengths = sorted(self.sessions + ([now - self.up_since] if self.up_since else []))
        lines = [f"stability over {total / 60:.1f} min: link up {100 * up / total:.1f}% of the time"
                 if total > 0 else "stability: no time elapsed"]
        if lengths:
            mid = lengths[len(lengths) // 2]
            lines.append(f"  {len(lengths)} link(s): longest {lengths[-1]:.0f}s, "
                         f"shortest {lengths[0]:.0f}s, median {mid:.0f}s")
        every = f", one every {total / 60 / self.drops:.1f} min" if self.drops else ""
        lines.append(f"  {self.drops} drop(s){every}")
        failed = sum(self.failures.values())
        lines.append(f"  {self.attempts} connect attempt(s), {failed} failed"
                     + "".join(f"\n    {k} ×{n}" for k, n in
                               sorted(self.failures.items(), key=lambda kv: -kv[1])))
        lines.append(f"  {self.stalls} event loop stall(s)")
        for line in lines:
            log.info(line)


async def _stability_report(stability, every=60.0):
    """One line a minute, so a long run shows its trend as it goes."""
    last_t = time.monotonic()
    last_up, last_drops, last_fail = 0.0, 0, 0
    while True:
        await asyncio.sleep(every)
        now = time.monotonic()
        up = stability.up_total(now)
        fails = sum(stability.failures.values())
        log.info("stability: link up %.0fs of the last %.0fs, %d drop(s), %d failed connect(s)",
                 up - last_up, now - last_t, stability.drops - last_drops, fails - last_fail)
        last_t, last_up, last_drops, last_fail = now, up, stability.drops, fails


class FrameWriter:
    """stdout, written from its own thread.

    print() from inside the event loop looks harmless until the reader is slow:
    the pipe fills, the write blocks, and it takes the whole loop down with it —
    no BLE notifications serviced, no keepalive sent. That showed up as the
    packet rate collapsing to 0 and then bursting to 112/s (above the headband's
    physical maximum) when the pipe finally drained, with the link timing out
    because we had stopped talking. Frames are cheap and continuous, so dropping
    a few beats stalling the radio."""

    def __init__(self):
        self.q = queue.Queue(maxsize=32)
        self.dropped = 0
        threading.Thread(target=self._run, daemon=True, name="stdout").start()

    def _run(self):
        while True:
            line = self.q.get()
            try:
                sys.stdout.write(line)
                sys.stdout.flush()
            except Exception:
                return

    def write(self, line: str):
        try:
            self.q.put_nowait(line)
        except queue.Full:
            self.dropped += 1


async def _loop_lag_watch(stability):
    """The same loop services BLE notifications, so a stall here is a stall
    there. Measure it rather than infer it from the packet rate."""
    while True:
        t = time.monotonic()
        await asyncio.sleep(0.1)
        lag = time.monotonic() - t - 0.1
        if lag > 0.4:
            stability.stalls += 1
            log.warning("event loop stalled %.2fs — BLE notifications were not "
                        "serviced during that time", lag)


class Baseline:
    """One channel's arrival level for one visitor.

    Learned during the anchor window and then left alone. A baseline that kept
    chasing the signal would cancel out the very drift it exists to measure —
    settle down for five minutes and a rolling baseline would quietly decide
    that was your normal and take the lights away again."""

    def __init__(self, better_when_lower=True):
        self.lower = better_when_lower
        self.samples = []
        self.med = None
        self.scale = 1.0

    def observe(self, x):
        if self.med is None and x is not None:
            self.samples.append(float(x))

    def freeze(self) -> bool:
        if len(self.samples) < 5:
            return False
        a = np.asarray(self.samples, dtype=float)
        self.med = float(np.median(a))
        mad = float(np.median(np.abs(a - self.med)))
        # MAD, not stddev: one jaw clench during the anchor window shouldn't
        # define the scale for the whole visit.
        self.scale = max(mad * 1.4826, abs(self.med) * 0.05, 1e-9)
        self.samples = []
        return True

    def score(self, x):
        """0..1, sitting at 0.5 when the visitor is exactly where they arrived
        and rising as they move the right way."""
        if self.med is None or x is None:
            return None
        d = (self.med - x) if self.lower else (x - self.med)
        return 1.0 / (1.0 + math.exp(-max(-20.0, min(20.0, d / self.scale))))


class Settle:
    """Turns the per-frame channels into one slow 0..1 score.

    Rises only while the honest evidence says the visitor is calmer than they
    arrived, and falls roughly three times faster — fast enough that tensing up
    is visibly punished within a few seconds, slow enough that nobody can sprint
    it. The anti-gaming mechanism and the intended experience are the same
    mechanism: the only way to win is to be patient."""

    def __init__(self):
        self.last_t = None      # deliberately not cleared by reset()
        self.reset()

    def reset(self):
        self.t0 = None
        self.level = 0.0
        self.frozen = False
        self.lost_since = None
        self.base = {
            "still": Baseline(True),    # accelerometer variance
            "emg":   Baseline(True),    # jaw / temple tension
            "bpm":   Baseline(True),    # heart rate
            "alpha": Baseline(False),   # fakeable, low weight
            "beta":  Baseline(True),    # semi-honest, low weight
        }

    def update(self, now: float, dt: float, live: bool, ch: dict) -> dict:
        # A gap in calls means the link was down. A few seconds of that is a
        # dropped connection, not a new person — only a long one starts over.
        gap = None if self.last_t is None else now - self.last_t
        self.last_t = now
        if gap is not None and gap > SESSION_DROP and self.t0 is not None:
            log.info("no data for %.0fs — starting a fresh visitor", gap)
            self.reset()
            self.last_t = now

        if not live:
            # Headband off or slipping. Bleed the level away, and once they have
            # really gone, forget them so the next visitor starts clean.
            if self.lost_since is None:
                self.lost_since = now
            self.level = max(0.0, self.level - dt / args.fall)
            if now - self.lost_since > SESSION_DROP and self.t0 is not None:
                log.info("session ended — baselines reset for the next visitor")
                self.reset()
            return {"settle": self.level, "session": 0.0, "phase": "waiting"}

        self.lost_since = None
        if self.t0 is None:
            log.info("new session — holding still for %.0fs to learn their baseline",
                     args.anchor)
            self.t0 = now
        elapsed = now - self.t0

        if not self.frozen:
            for name, b in self.base.items():
                b.observe(ch.get(name))
            if elapsed >= args.anchor:
                got = [n for n, b in self.base.items() if b.freeze()]
                self.frozen = True
                log.info("baseline set from %s — the lamps start listening now",
                         ", ".join(got) or "nothing usable")
            return {"settle": 0.0, "session": elapsed, "phase": "anchor"}

        scored = {n: b.score(ch.get(n)) for n, b in self.base.items()}
        honest = [scored[n] for n in ("still", "emg", "bpm") if scored[n] is not None]
        if not honest:
            return {"settle": self.level, "session": elapsed, "phase": "live",
                    "evidence": None, "scores": {}}

        # Geometric mean, not a sum: a visitor sitting perfectly still with a
        # clenched jaw should not be able to average their way to a lit room.
        h = math.exp(sum(math.log(max(s, 1e-6)) for s in honest) / len(honest))

        soft = [scored[n] for n in ("alpha", "beta") if scored[n] is not None]
        e = sum(soft) / len(soft) if soft else 0.5
        evidence = h * (1.0 + ALPHA_INFLUENCE * (e - 0.5))

        # Scaled so both ends saturate at ±1: fully settled rises in --rise
        # seconds, fully agitated falls in --fall, and merely returning to the
        # arrival state gives a gentle negative drift rather than nothing.
        gap = evidence - EVIDENCE_BIAS
        drift = gap / (1.0 - EVIDENCE_BIAS) if gap >= 0 else gap / EVIDENCE_BIAS
        drift = max(-1.0, min(1.0, drift))
        self.level += drift * dt / (args.rise if drift >= 0 else args.fall)
        self.level = max(0.0, min(1.0, self.level))
        return {"settle": self.level, "session": elapsed, "phase": "live",
                "evidence": evidence,
                "scores": {k: (None if v is None else round(v, 3))
                           for k, v in scored.items()}}


def _band_powers(x: np.ndarray) -> dict:
    """Mean spectral power per band. Mean (not sum) so wide bands aren't favoured."""
    x = x - x.mean()
    spec = np.abs(np.fft.rfft(x * np.hanning(len(x)))) ** 2
    freqs = np.fft.rfftfreq(len(x), 1.0 / EEG_FS)
    out = {}
    for name, (lo, hi) in BANDS.items():
        mask = (freqs >= lo) & (freqs < hi)
        out[name] = float(spec[mask].mean()) if mask.any() else 0.0
    return out


def _heart_rate(ppg: np.ndarray):
    """Dominant frequency in the 42–180 bpm range. Cheaper and far steadier than
    peak detection on a wrist-grade sensor. Bins are ~7 bpm apart at this window
    length, so interpolate around the peak or a pulsing LED visibly steps."""
    x = ppg.astype(float)
    x -= x.mean()
    spec = np.abs(np.fft.rfft(x * np.hanning(len(x))))
    freqs = np.fft.rfftfreq(len(x), 1.0 / PPG_FS)
    lo, hi = int(np.searchsorted(freqs, 0.7)), int(np.searchsorted(freqs, 3.0))
    if hi <= lo:
        return None
    k = lo + int(np.argmax(spec[lo:hi]))
    if 0 < k < len(spec) - 1:
        a, b, c = spec[k - 1], spec[k], spec[k + 1]
        denom = a - 2 * b + c
        if denom:
            k += max(-0.5, min(0.5, 0.5 * (a - c) / denom))
    return float(k * PPG_FS / len(x) * 60.0)


class Muse:
    def __init__(self):
        n_eeg = int(EEG_FS * max(args.window, 0.5))
        self.eeg = {ch: deque(maxlen=n_eeg) for ch in EEG_CHANS}
        self.ppg = deque(maxlen=int(PPG_FS * 8))
        self.acc_mag = deque(maxlen=int(ACC_FS * 12))
        self.settle = Settle()
        self.last_update = None
        # A healthy link delivers ~85 eeg packets/s (4 channels, 21.3 each).
        # Watching that number is how a stall shows itself before the link dies.
        self.packets = 0
        self.total_packets = 0
        self.replies = asyncio.Queue(maxsize=16)   # control replies, in arrival order
        self.pkt_mark = time.monotonic()
        self.pkt_rate = 0.0
        # Every eeg packet carries a 16-bit sequence number per channel. It is the
        # only way to tell packets lost in transit from a headband that simply
        # sends fewer, so count the gaps instead of guessing at them.
        self.last_index = {}
        self.lost = 0
        self.lost_window = 0
        self.loss = 0.0
        self.link_since = None
        self.drops = 0
        self.stability = Stability()
        self.accel = (0.0, 0.0, 1.0)
        self.battery = None
        self.bands = None                      # smoothed relative powers
        self.blink_until = 0.0
        self.blink_pending = False
        self.control_buf = ""
        self.calm_range = AutoRange()
        self.focus_range = AutoRange()
        self.started = time.monotonic()

    # notification handlers ------------------------------------------------

    def on_eeg(self, channel: str):
        buf = self.eeg[channel]

        def handler(_sender, data: bytearray):
            if len(data) < 20:
                return
            packet = bytes(data)
            index = int.from_bytes(packet[0:2], "big")
            samples = _unpack_eeg(packet)
            last = self.last_index.get(channel)
            self.last_index[channel] = index
            if last is not None:
                missed = (index - last - 1) & 0xFFFF
                if 0 < missed < 256:
                    self.lost += missed
                    self.lost_window += missed
                    # Bridge the gap rather than splice across it. Joining the
                    # packets either side makes a step, and a step reads as
                    # broadband energy — thousands of microvolts of nothing.
                    # Interpolating also keeps the time axis uniform, which the
                    # FFT assumes.
                    if buf:
                        fill = np.linspace(buf[-1], samples[0], 12 * missed + 2)[1:-1]
                        buf.extend(fill.tolist())
            self.packets += 1
            self.total_packets += 1
            buf.extend(samples)
            if channel in FRONTAL:
                self._check_blink(samples)
        return handler

    def on_control(self, _sender, data: bytearray):
        """The headband answers v1/s here, as JSON split across packets: a length
        byte then that many chars. We barely care what it says — but muse-lsl
        subscribes to this and so must we, because the device hangs up when its
        replies have nowhere to go."""
        if not data:
            return
        n = data[0]
        chunk = bytes(data)[1:1 + n].decode("ascii", errors="replace")
        if chunk.startswith("{"):
            self.control_buf = ""   # a new reply; drop any fragment a lost packet orphaned
        self.control_buf += chunk
        if not self.control_buf.rstrip().endswith("}"):
            return
        msg = self.control_buf.strip()
        self.control_buf = ""
        try:
            rc = json.loads(msg).get("rc")
        except ValueError:
            rc = None
        if rc not in (0, None):
            log.warning("headband rejected a command: %s", msg)
        elif msg == '{"rc":0}':
            log.debug("headband ack")      # keepalive acks, every 5s — noise
        else:
            log.info("headband says: %s", msg)
        try:
            self.replies.put_nowait(msg)
        except asyncio.QueueFull:
            pass    # only matters while a command is waiting, and none is

    def on_accel(self, _sender, data: bytearray):
        if len(data) < 20:
            return
        samples = _unpack_imu(bytes(data), ACCEL_SCALE)
        self.accel = samples[-1]
        for x, y, z in samples:
            self.acc_mag.append(math.sqrt(x * x + y * y + z * z))

    def on_ppg(self, _sender, data: bytearray):
        if len(data) < 20:
            return
        self.ppg.extend(_unpack_ppg(bytes(data)))

    def on_telemetry(self, _sender, data: bytearray):
        if len(data) < 10:
            return
        self.battery = struct.unpack(">5H", bytes(data)[:10])[1] / 512.0

    # derived values -------------------------------------------------------

    def _check_blink(self, samples):
        now = time.monotonic()
        if now < self.blink_until:
            return
        centred = np.asarray(samples) - np.mean(samples)
        if np.abs(centred).max() > BLINK_UV:
            self.blink_until = now + BLINK_HOLD
            self.blink_pending = True

    def drop_stale_signal(self):
        """Called on reconnect. The samples either side of an outage are not
        continuous, so throw them away — but the visitor's session carries on."""
        for buf in self.eeg.values():
            buf.clear()
        self.ppg.clear()
        self.acc_mag.clear()
        # Sequence numbers restart with the link, and a rate measured across the
        # outage would average the dead time in.
        self.last_index.clear()
        self.packets = 0
        self.lost_window = 0
        self.pkt_mark = time.monotonic()

    def _emg(self):
        """Absolute 30-45 Hz power at the temporal electrodes — jaw and temple
        tension. The one 'not relaxed' signal a visitor cannot consciously fake,
        and the reason closing your eyes doesn't light the room."""
        need = int(EEG_FS * args.window)
        vals = []
        for ch in TEMPORAL:
            buf = self.eeg[ch]
            if len(buf) < need:
                continue
            x = np.asarray(buf, dtype=float)[-need:]
            x = x - x.mean()
            spec = np.abs(np.fft.rfft(x * np.hanning(len(x)))) ** 2
            freqs = np.fft.rfftfreq(len(x), 1.0 / EEG_FS)
            mask = (freqs >= EMG_BAND[0]) & (freqs < EMG_BAND[1])
            if mask.any():
                vals.append(float(spec[mask].mean()))
        return float(np.mean(vals)) if vals else None

    def _stillness(self):
        """Spread of the accelerometer magnitude — fidgeting, shifting, looking
        around. Honest by construction: you cannot fake it while restless."""
        if len(self.acc_mag) < int(ACC_FS * 4):
            return None
        return float(np.std(np.asarray(self.acc_mag)))

    def channel_amplitude(self):
        """Per-channel peak-to-peak in microvolts, measured on 1-44Hz content.

        Raw peak-to-peak was the wrong measure: a freshly placed electrode
        drifts hundreds of microvolts below 1Hz, so a perfectly good channel
        sailed past the railed threshold and was written off as detached.
        Band-limiting drops that drift and the mains hum with it."""
        need = int(EEG_FS * args.window)
        out = {}
        for ch, buf in self.eeg.items():
            if len(buf) < need:
                out[ch] = None
                continue
            x = np.asarray(buf, dtype=float)[-need:]
            # Detrend before filtering. A 2s window only resolves 0.5Hz, so a
            # slow wander leaks straight through a 1Hz cutoff; fitting it out
            # first is what actually removes it.
            idx = np.arange(len(x))
            x = x - np.polyval(np.polyfit(idx, x, 2), idx)
            spectrum = np.fft.rfft(x)
            freqs = np.fft.rfftfreq(len(x), 1.0 / EEG_FS)
            spectrum[(freqs < 2.0) | (freqs >= 44.0)] = 0
            band = np.fft.irfft(spectrum, n=len(x))
            out[ch] = float(band.max() - band.min())
        return out

    def _contact(self, amps=None):
        """Channels whose in-band amplitude looks like skin rather than air."""
        amps = self.channel_amplitude() if amps is None else amps
        return [ch for ch, a in amps.items()
                if a is not None and FLAT_UV < a < RAILED_UV]

    def sample(self):
        """One frame of light-ready values, or None until the buffers fill."""
        need = int(EEG_FS * args.window)
        ready = [ch for ch, buf in self.eeg.items() if len(buf) >= need]
        if not ready:
            return None

        amps = self.channel_amplitude()
        good = set(self._contact(amps))
        use = [ch for ch in ready if ch in good] or ready   # fall back rather than stall

        totals = {b: 0.0 for b in BANDS}
        for ch in use:
            x = np.asarray(self.eeg[ch], dtype=float)[-need:]
            for band, power in _band_powers(x).items():
                totals[band] += power

        overall = sum(totals.values())
        if overall <= 0:
            return None
        rel = {b: totals[b] / overall for b in BANDS}

        if self.bands is None:                              # EMA to stop the flicker
            self.bands = rel
        else:
            a = args.smooth
            self.bands = {b: a * rel[b] + (1 - a) * self.bands[b] for b in BANDS}
        rel = self.bands

        calm = rel["alpha"] / (rel["alpha"] + rel["beta"] + 1e-9)
        focus = rel["beta"] / (rel["alpha"] + rel["theta"] + 1e-9)

        ax, ay, az = self.accel
        mag = math.sqrt(ax * ax + ay * ay + az * az)
        motion = min(1.0, abs(mag - 1.0) * 5.0)
        pitch = math.degrees(math.atan2(-ax, math.sqrt(ay * ay + az * az) + 1e-9))
        roll = math.degrees(math.atan2(ay, az if abs(az) > 1e-9 else 1e-9))

        blink = self.blink_pending
        self.blink_pending = False

        bpm = None
        if args.ppg and len(self.ppg) >= int(PPG_FS * 6):
            bpm = _heart_rate(np.asarray(self.ppg))

        now = time.monotonic()
        dt = 0.0 if self.last_update is None else min(now - self.last_update, 1.0)
        self.last_update = now

        elapsed = now - self.pkt_mark
        if elapsed >= 1.0:
            self.pkt_rate = self.packets / elapsed
            expected = self.packets + self.lost_window
            self.loss = self.lost_window / expected if expected else 0.0
            self.packets = 0
            self.lost_window = 0
            self.pkt_mark = now
        st = self.settle.update(now, dt, len(good) >= SETTLE_CONTACT, {
            "still": self._stillness(),
            "emg":   self._emg(),
            "bpm":   bpm,
            "alpha": rel["alpha"],
            "beta":  rel["beta"],
        })

        out = {
            "t": round(now - self.started, 2),
            "settle": round(st["settle"], 4),
            "session": round(st["session"], 1),
            "phase": st["phase"],
            **{b: round(rel[b], 4) for b in BANDS},
            "calm": round(self.calm_range(calm), 4),
            "focus": round(self.focus_range(focus), 4),
            "motion": round(motion, 4),
            "pitch": round(pitch, 1),
            "roll": round(roll, 1),
            "blink": blink,
            "contact": len(good),
            "pps": round(self.pkt_rate, 1),
            "loss": round(self.loss, 3),
            "lost": self.lost,
            "link": round(now - self.link_since, 1) if self.link_since else 0.0,
            "drops": self.drops,
            "channels": {ch: (None if a is None else round(a, 1))
                         for ch, a in amps.items()},
            "evidence": (None if st.get("evidence") is None
                         else round(st["evidence"], 3)),
            "scores": st.get("scores", {}),
        }
        if bpm:
            out["bpm"] = round(bpm, 1)
        if self.battery is not None:
            out["battery"] = round(self.battery, 1)
        return out


def _bar(value: float, width=24) -> str:
    filled = int(round(min(1.0, max(0.0, value)) * width))
    return "█" * filled + "░" * (width - filled)


class Meters:
    """Redraws a fixed block of lines in place."""

    def __init__(self):
        self.lines = 0

    def draw(self, s: dict):
        phase = {"waiting": "no headband",
                 "anchor": f"learning baseline, {max(0, args.anchor - s['session']):.0f}s left",
                 "live": f"session {s['session']:.0f}s"}[s["phase"]]
        rows = [
            f"Muse 2  ·  battery {s.get('battery', '--')}%  ·  "
            f"contact {s['contact']}/4  ·  {phase}",
            "",
            f"  {'SETTLE':<6} {_bar(s['settle'])}  {s['settle']:.2f}",
            "",
        ]
        for band in BANDS:
            rows.append(f"  {band:<6} {_bar(s[band] * 2.5)}  {s[band]:.2f}")
        rows.append("")
        rows.append(f"  {'calm':<6} {_bar(s['calm'])}  {s['calm']:.2f}")
        rows.append(f"  {'focus':<6} {_bar(s['focus'])}  {s['focus']:.2f}")
        rows.append(f"  {'motion':<6} {_bar(s['motion'])}  {s['motion']:.2f}"
                    f"   pitch {s['pitch']:>6.1f}°  roll {s['roll']:>6.1f}°")
        bpm = f"{s['bpm']:.0f}" if "bpm" in s else "--"
        rows.append(f"  {'bpm':<6} {bpm:<6}  blink {'●' if s['blink'] else '·'}")
        rows.append("")
        fit = []
        for ch, a in s.get("channels", {}).items():
            mark = "·" if a is None else ("ok" if FLAT_UV < a < RAILED_UV else "--")
            fit.append(f"{ch} {'?' if a is None else f'{a:.0f}'}µV {mark}")
        rows.append("  " + "   ".join(fit))

        if self.lines:
            sys.stdout.write(f"\033[{self.lines}A")
        sys.stdout.write("".join(f"\033[2K{r}\n" for r in rows))
        sys.stdout.flush()
        self.lines = len(rows)


async def scan_report(timeout: float = 5.0):
    """Every Muse in range, with signal strength. RSSI is the point: the drops
    look like a marginal link, and a number beats guessing. Roughly, -60 is
    comfortable, -80 is where a steady EEG stream starts to struggle."""
    found = await BleakScanner.discover(timeout=timeout, return_adv=True)
    out = []
    for device, adv in found.values():
        if device.name and device.name.lower().startswith("muse"):
            out.append({"address": device.address, "name": device.name,
                        "rssi": getattr(adv, "rssi", None)})
    return sorted(out, key=lambda d: d["rssi"] if d["rssi"] is not None else -999,
                  reverse=True)


def _session_lock():
    """One muse session at a time.

    The headband takes a single connection, so two sessions fight over it and
    both measurements come out wrong — which has happened twice. The OS drops a
    flock however the process dies, SIGKILL included, so a crash can never
    leave it stuck. Holding it is also what makes _release_stale_link safe:
    while we have the lock, no other session can own a Muse link."""
    f = open(LOCK_PATH, "a+")
    deadline = time.monotonic() + LOCK_WAIT
    while True:
        try:
            fcntl.flock(f, fcntl.LOCK_EX | fcntl.LOCK_NB)
            break
        except OSError:
            if time.monotonic() > deadline:
                f.seek(0)
                holder = f.read().strip() or "?"
                f.close()
                return None, holder
            time.sleep(0.5)
    f.seek(0)
    f.truncate()
    f.write(str(os.getpid()))
    f.flush()
    return f, None


async def _release_stale_link():
    """Disconnect any Muse BlueZ is still holding from a run that died.

    BlueZ owns the connection, not the process that asked for it, so a session
    killed without its teardown leaves the headband connected to nobody. A
    connected Muse stops advertising, so every scan after that comes up empty —
    the "waiting for the headband" after a Stop. Called between links, while
    we hold the session lock, so any Muse connected now cannot be ours."""
    if not shutil.which(BLUETOOTHCTL):
        return
    try:
        proc = await asyncio.create_subprocess_exec(
            BLUETOOTHCTL, "devices", "Connected",
            stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.DEVNULL)
        out, _ = await asyncio.wait_for(proc.communicate(), 5.0)
    except Exception:
        return
    for line in out.decode(errors="replace").splitlines():
        parts = re.sub(r"\x1b\[[0-9;]*m", "", line).split(None, 2)
        if len(parts) == 3 and parts[0] == "Device" and parts[2].lower().startswith("muse"):
            log.warning("BlueZ is still holding %s from a run that did not tear "
                        "down — releasing it", parts[2])
            try:
                p = await asyncio.create_subprocess_exec(
                    BLUETOOTHCTL, "disconnect", parts[1],
                    stdout=asyncio.subprocess.DEVNULL, stderr=asyncio.subprocess.DEVNULL)
                await asyncio.wait_for(p.wait(), 10.0)
            except Exception:
                pass


async def find_muse(address: str = None):
    """Returns BLEDevice objects, never bare addresses.

    Handing BleakClient an address string makes the BlueZ backend go and resolve
    it to a dbus path of its own accord, and the Muse only advertises in bursts —
    by the time it looks, the cache has gone cold and you get "Device with
    address ... was not found" despite the scan having just seen it."""
    log.info("scanning for a Muse…")
    if address:
        device = await BleakScanner.find_device_by_address(address, timeout=8.0)
        return [device] if device else []
    devices = await BleakScanner.discover(timeout=8.0)
    return [d for d in devices if d.name and d.name.lower().startswith("muse")]


async def _keepalive(client):
    """Poke the control characteristic periodically. Streaming notifications on
    their own do not count as activity — the headband hangs up on a control
    channel that has gone quiet, which reads as random disconnects."""
    while True:
        await asyncio.sleep(KEEPALIVE)
        try:
            await client.write_gatt_char(CONTROL, _cmd("k"), response=False)
        except Exception:
            return          # link is already gone; the session loop handles it


async def _first_data_watch(muse, since):
    """Say plainly whether the stream ever started. Accepting the commands and
    then sending nothing looks identical to a healthy connection otherwise.

    Counts from this session's starting point: the total is cumulative, and
    checking it raw let a previous session's packets pass for this one's."""
    start = muse.total_packets
    for _ in range(100):
        await asyncio.sleep(0.1)
        if muse.total_packets > start:
            log.info("first eeg packet %.1fs after start", time.monotonic() - since)
            return
    log.warning("no eeg packets 10s after start — the headband took the "
                "commands but is not streaming")


async def _command(client, muse, text, expect=None, timeout=2.0):
    """Send one control command and wait for its own reply.

    The headband acks every command, but the acks lag. Waiting on 'any reply
    since I cleared' let the late ack to the preset satisfy the wait for 's',
    so 'd' went out two seconds before the headband had answered the status
    request — and that session connected, reported streaming, and sent nothing.
    Drain whatever is queued, send, then wait for a reply that matches."""
    while not muse.replies.empty():
        muse.replies.get_nowait()
    await client.write_gatt_char(CONTROL, _cmd(text), response=False)
    deadline = time.monotonic() + timeout
    while True:
        left = deadline - time.monotonic()
        try:
            if left <= 0:
                raise asyncio.TimeoutError
            msg = await asyncio.wait_for(muse.replies.get(), timeout=left)
        except asyncio.TimeoutError:
            log.warning("no reply to %r within %.0fs", text, timeout)
            return None
        if expect is None or expect(msg):
            return msg


async def _teardown(client, halt=True):
    """Best effort and bounded — a link that is already gone must not hang us."""
    if halt:
        try:
            await asyncio.wait_for(
                client.write_gatt_char(CONTROL, _cmd("h"), response=False), 3.0)
        except Exception:
            pass
    try:
        await asyncio.wait_for(client.disconnect(), 5.0)
    except Exception:
        pass


async def _subscribe_and_start(client, muse):
    # Control first, like muse-lsl: the replies to v1/s start coming back
    # as soon as they are asked for.
    await client.start_notify(CONTROL, muse.on_control)
    for ch, uuid in EEG_CHANS.items():
        await client.start_notify(uuid, muse.on_eeg(ch))
    await client.start_notify(ACCEL, muse.on_accel)
    await client.start_notify(TELEMETRY, muse.on_telemetry)
    if args.ppg:
        await client.start_notify(PPG_IR, muse.on_ppg)

    # One command at a time, each waiting for its own reply, so 'd' can only
    # go out once the headband has actually answered 's'.
    await _command(client, muse, "v1", expect=lambda m: '"fw"' in m)
    await _command(client, muse, PRESET)
    status = await _command(client, muse, "s",
                            expect=lambda m: '"hn"' in m or '"sn"' in m, timeout=3.0)
    if status is None:
        log.warning("no status reply — starting anyway, but it may not stream")
    await _command(client, muse, "d")


async def run_session(device, muse):
    dropped = asyncio.Event()
    started = time.monotonic()
    muse.drop_stale_signal()

    client = BleakClient(device, disconnected_callback=lambda _c: dropped.set(),
                         timeout=CONNECT_TIMEOUT)
    # Every step is bounded. BlueZ will happily block forever on a stale link,
    # and hanging with nothing in the log is the worst way for this to fail —
    # far better to give up, say so, and scan again.
    log.info("connecting to %s…", device.address)
    muse.stability.attempt()
    try:
        await asyncio.wait_for(client.connect(), timeout=CONNECT_TIMEOUT)
    except BaseException as e:
        if not isinstance(e, asyncio.CancelledError):
            muse.stability.failed("connect", e)
        # The cleanup below only covered links that finished connecting. A
        # connect that failed or timed out mid-flight can leave BlueZ holding a
        # half-open link, and every later attempt then wedges against it — the
        # stale link that took bluetoothctl to clear by hand. Always tear down.
        await _teardown(client, halt=False)
        raise
    log.info("connected to %s", device.address)
    muse.link_since = time.monotonic()
    muse.stability.connected()

    alive = None
    watch = None
    phase = "setup"
    try:
        await asyncio.wait_for(_subscribe_and_start(client, muse), timeout=SETUP_TIMEOUT)
        phase = "streaming"
        log.info("streaming (preset %s) — ctrl-c to stop", PRESET)

        meters = None if args.json else Meters()
        period = 1.0 / args.rate
        next_at = time.monotonic()
        alive = asyncio.create_task(_keepalive(client))
        watch = asyncio.create_task(_first_data_watch(muse, time.monotonic()))
        while not dropped.is_set():
            next_at += period
            await asyncio.sleep(max(0.0, next_at - time.monotonic()))
            s = muse.sample()
            if s is None:
                continue
            if meters:
                meters.draw(s)
            else:
                writer.write(json.dumps(s) + "\n")
    except asyncio.CancelledError:
        raise
    except Exception as e:
        muse.stability.failed(phase, e)
        raise
    finally:
        for task in (alive, watch):
            if task:
                task.cancel()
        await _teardown(client)
        muse.stability.disconnected(dropped=dropped.is_set())

    muse.drops += 1
    muse.link_since = None
    log.warning("headband disconnected after %.1fs (drop #%d this run)",
                time.monotonic() - started, muse.drops)


async def main():
    global writer
    writer = FrameWriter()
    if args.scan:
        devices = await scan_report()
        if args.json:
            print(json.dumps(devices), flush=True)
        else:
            for d in devices:
                print(f"{d['address']}  {d['name']}  {d['rssi']} dBm")
        return

    lock, holder = _session_lock()
    if lock is None:
        log.error("another muse session is already running (pid %s) — stop it "
                  "first; two sessions fight over a headband that takes one "
                  "connection", holder)
        raise SystemExit(1)

    # Stop cleanly on SIGTERM, which is what the Brain page's Stop and the
    # service's restart sweep both send, and on SIGINT even when it arrives
    # ignored — a background job inherits SIGINT as ignored, and Python keeps
    # it that way. Without this both skip the teardown: no halt, no disconnect,
    # and BlueZ goes on holding a link nobody is reading.
    main_task = asyncio.current_task()
    loop = asyncio.get_running_loop()
    for sig in (signal.SIGTERM, signal.SIGINT):
        try:
            loop.add_signal_handler(sig, main_task.cancel)
        except (NotImplementedError, RuntimeError):
            pass

    # One Muse for the whole run. A reconnect must not wipe the visitor's
    # baselines — losing the link for three seconds is not a new person, and
    # rebuilding this per session meant the anchor window could never finish.
    muse = Muse()
    asyncio.create_task(_loop_lag_watch(muse.stability))
    asyncio.create_task(_stability_report(muse.stability))
    try:
        if args.soak:
            log.info("soak test: %.0f min, then a stability summary", args.soak)
            try:
                await asyncio.wait_for(_run(muse), timeout=args.soak * 60)
            except asyncio.TimeoutError:
                pass
        else:
            await _run(muse)
    except asyncio.CancelledError:
        log.info("stopping — link torn down")
    finally:
        muse.stability.summary()
        lock.close()


async def _run(muse):
    while True:
        try:
            await _release_stale_link()
            # Rescan every attempt: a BLEDevice from a previous pass is stale
            # once the link drops, and the headband has usually moved on anyway.
            found = await find_muse(args.address)
            if not found:
                log.warning("no Muse found — is it on and out of the phone app?")
                await asyncio.sleep(RETRY_DELAY)
                continue
            device = found[0]
            log.info("found %s (%s)", device.name, device.address)
            await run_session(device, muse)
        except asyncio.CancelledError:
            raise
        except Exception as e:
            log.error("session failed: %s", e)
        if args.no_reconnect:
            return
        await asyncio.sleep(RETRY_DELAY)


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print()
