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
import json
import logging
import math
import struct
import sys
import time
from collections import deque

import numpy as np
from bleak import BleakClient, BleakScanner

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s %(levelname)s %(message)s",
)
log = logging.getLogger("muse")

EEG_FS = 256.0          # EEG sample rate, fixed by the headband
PPG_FS = 64.0           # PPG sample rate
ACC_FS = 52.0           # accelerometer sample rate
RETRY_DELAY = 3.0
KEEPALIVE = 5.0         # must beat the headband's ~7.5s control-idle timeout

EEG_LSB      = 0.48828125   # µV per count, 12 bits over a 2 mVpp range
ACCEL_SCALE  = 0.0000610352 # g per count
GYRO_SCALE   = 0.0074768    # deg/s per count

FLAT_UV    = 2.0    # below this peak-to-peak the electrode isn't touching skin
RAILED_UV  = 800.0  # above it we're picking up mains hum or a railed amp
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
EVIDENCE_BIAS = 0.6


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
parser.add_argument("--preset",   default=None,
                    help="Override the headband preset (default p50 with --ppg, else p21)")
parser.add_argument("--no-reconnect", action="store_true",
                    help="Exit on disconnect instead of retrying")
args = parser.parse_args()

PRESET = args.preset or ("p50" if args.ppg else "p21")


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
            return {"settle": self.level, "session": elapsed, "phase": "live"}

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
        return {"settle": self.level, "session": elapsed, "phase": "live"}


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
            samples = _unpack_eeg(bytes(data))
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
        self.control_buf += bytes(data)[1:1 + n].decode("ascii", errors="replace")
        if self.control_buf.rstrip().endswith("}"):
            log.debug("control: %s", self.control_buf.strip())
            self.control_buf = ""

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

    def _contact(self):
        """Channels whose peak-to-peak looks like skin rather than air."""
        good = []
        for ch, buf in self.eeg.items():
            if len(buf) < 32:
                continue
            x = np.asarray(buf)
            if FLAT_UV < (x.max() - x.min()) < RAILED_UV:
                good.append(ch)
        return good

    def sample(self):
        """One frame of light-ready values, or None until the buffers fill."""
        need = int(EEG_FS * args.window)
        ready = [ch for ch, buf in self.eeg.items() if len(buf) >= need]
        if not ready:
            return None

        good = set(self._contact())
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

        if self.lines:
            sys.stdout.write(f"\033[{self.lines}A")
        sys.stdout.write("".join(f"\033[2K{r}\n" for r in rows))
        sys.stdout.flush()
        self.lines = len(rows)


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


async def run_session(device, muse):
    dropped = asyncio.Event()
    started = time.monotonic()
    muse.drop_stale_signal()

    async with BleakClient(device, disconnected_callback=lambda _c: dropped.set()) as client:
        log.info("connected to %s", device.address)

        # Control first, like muse-lsl: the replies to v1/s start coming back
        # as soon as they are asked for.
        await client.start_notify(CONTROL, muse.on_control)
        for ch, uuid in EEG_CHANS.items():
            await client.start_notify(uuid, muse.on_eeg(ch))
        await client.start_notify(ACCEL, muse.on_accel)
        await client.start_notify(TELEMETRY, muse.on_telemetry)
        if args.ppg:
            await client.start_notify(PPG_IR, muse.on_ppg)

        await client.write_gatt_char(CONTROL, _cmd("v1"), response=False)
        await client.write_gatt_char(CONTROL, _cmd(PRESET), response=False)
        await client.write_gatt_char(CONTROL, _cmd("s"), response=False)
        await client.write_gatt_char(CONTROL, _cmd("d"), response=False)
        log.info("streaming (preset %s) — ctrl-c to stop", PRESET)

        meters = None if args.json else Meters()
        period = 1.0 / args.rate
        next_at = time.monotonic()
        alive = asyncio.create_task(_keepalive(client))
        try:
            while not dropped.is_set():
                next_at += period
                await asyncio.sleep(max(0.0, next_at - time.monotonic()))
                s = muse.sample()
                if s is None:
                    continue
                if meters:
                    meters.draw(s)
                else:
                    print(json.dumps(s), flush=True)
        finally:
            alive.cancel()
            try:
                await client.write_gatt_char(CONTROL, _cmd("h"), response=False)
            except Exception:
                pass

    # The duration is the diagnostic: the same number every time is something
    # hanging up on us, a scattered one is the radio link failing.
    log.warning("headband disconnected after %.1fs", time.monotonic() - started)


async def main():
    if args.scan:
        for d in await find_muse():
            print(f"{d.address}  {d.name}")
        return

    # One Muse for the whole run. A reconnect must not wipe the visitor's
    # baselines — losing the link for three seconds is not a new person, and
    # rebuilding this per session meant the anchor window could never finish.
    muse = Muse()
    while True:
        try:
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
