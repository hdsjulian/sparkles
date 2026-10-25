#!/usr/bin/env python3
"""
Tests for the Muse headband pipeline — muse.py and muse_bridge.py.

No hardware needed: the BLE side runs against a fake BleakClient that
misbehaves the way the real headband did — late acks, connects that fail or
hang, dropped packets, a sparkles restart under the bridge. Each of those
was a real bug; these are here so they stay fixed.

Usage:
    python3 test_muse.py
"""

import asyncio
import io
import json
import math
import os
import signal
import socket
import subprocess
import sys
import tempfile
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

# both scripts parse argv at import
sys.argv = ["muse.py", "--json", "--preset", "p21"]
import muse  # noqa: E402
SOCK = "/tmp/sparkles_test_music.sock"
sys.argv = ["muse_bridge.py", "--sock", SOCK]
import muse_bridge  # noqa: E402

G, R, X = "\033[32m", "\033[31m", "\033[0m"
failures = []


def check(name, cond, detail=""):
    print(f"  {G}✓{X} {name}" if cond else f"  {R}✗{X} {name}  {detail}")
    if not cond:
        failures.append(name)


def decode():
    print("decode")
    vals = [0, 2048, 4095, 1024, 3072, 100, 200, 300, 400, 500, 600, 700]
    bits = 0
    for v in vals:
        bits = (bits << 12) | v
    got = muse._unpack_eeg((7).to_bytes(2, "big") + bits.to_bytes(18, "big"))
    check("12-bit eeg samples", all(abs(g - (v - 2048) * muse.EEG_LSB) < 1e-9
                                    for g, v in zip(got, vals)))
    check("control frames", muse._cmd("d") == b"\x02d\n")


def settle():
    print("settle scoring")
    import random
    rng = random.Random(0)
    n = lambda v, p: v * (1 + rng.uniform(-p, p))
    arrival = lambda: {"still": n(0.010, .15), "emg": n(100.0, .15), "bpm": n(74.0, .04),
                       "alpha": n(0.20, .10), "beta": n(0.30, .10)}
    relaxed = lambda: {"still": n(0.003, .15), "emg": n(30.0, .15), "bpm": n(63.0, .04),
                       "alpha": n(0.34, .10), "beta": n(0.20, .10)}

    def eyes_closed():
        f = arrival()
        f["alpha"], f["beta"] = 0.75, 0.22
        return f

    def run(frames, secs):
        s, t = muse.Settle(), 0.0
        for _ in range(int(muse.args.anchor / 0.1)):
            t += 0.1
            s.update(t, 0.1, True, arrival())
        out = None
        for _ in range(int(secs / 0.1)):
            t += 0.1
            out = s.update(t, 0.1, True, frames())
        return out

    check("genuine settling lights up", run(relaxed, 150)["settle"] > 0.8)
    check("closing your eyes alone does not", run(eyes_closed, 180)["settle"] < 0.25)
    check("staying as you arrived earns nothing", run(arrival, 180)["settle"] < 0.15)

    def anchored():
        s, t, half = muse.Settle(), 0.0, None
        for i in range(int(muse.args.anchor / 0.1) + 5):
            t += 0.1
            out = s.update(t, 0.1, True, arrival())
            if i == int(muse.args.anchor / 0.2):
                half = out["settle"]
        return s, t, half, out["settle"]

    # a session comes up to its starting level while the baseline is learned
    s, t, half, level = anchored()
    check("the lamps come up while the baseline is learned",
          abs(half - muse.args.start / 2) < 0.02, f"{half:.3f} halfway through")
    check("and a session starts at the chosen level",
          abs(level - muse.args.start) < 0.01, f"{level:.3f}")

    # from there the climb is quick at first, flattening toward full, read
    # every 10 s of steady calm
    levels = [level]
    for _ in range(4):
        for _ in range(100):
            t += 0.1
            out = s.update(t, 0.1, True, relaxed())
        levels.append(out["settle"])
    gains = [b - a for a, b in zip(levels, levels[1:])]
    check("the climb is quick at first and flattens toward full",
          all(a > b > 0 for a, b in zip(gains, gains[1:])) and levels[-1] < 1.0,
          [round(v, 3) for v in levels])

    # and getting tenser than you arrived dims it from the start
    tense = lambda: {"still": n(0.030, .15), "emg": n(300.0, .15), "bpm": n(84.0, .04),
                     "alpha": n(0.20, .10), "beta": n(0.30, .10)}
    s, t, _, level = anchored()
    for _ in range(100):
        t += 0.1
        out = s.update(t, 0.1, True, tense())
    check("getting tenser than you arrived dims the lamps", out["settle"] < 0.6 * level,
          f"{level:.2f} -> {out['settle']:.2f} in 10 s")

    # a dropped link takes 16-19s to come back, and that is not a new visitor
    s, t = muse.Settle(), 0.0
    for _ in range(int(muse.args.anchor / 0.1) + 5):
        t += 0.1
        s.update(t, 0.1, True, arrival())
    t += 20.0
    out = s.update(t, 0.1, True, arrival())
    check("a reconnect keeps the visitor", out["phase"] == "live", out["phase"])
    t += muse.LINK_GRACE + 1
    out = s.update(t, 0.1, True, arrival())
    check("a long outage starts a new visitor", out["phase"] == "anchor", out["phase"])


async def commands():
    print("live tuning")
    m = muse.Muse()
    m.settle.frozen, m.settle.level = True, 0.3
    saved = (muse.args.start, muse.args.up, muse.args.down)
    loop = asyncio.get_running_loop()
    stream = io.StringIO("set up 0.25\nset start 0.5\nset bogus 1\nset down x\nnonsense\n")
    await loop.run_in_executor(None, muse._read_commands, loop, m, stream)
    await asyncio.sleep(0.05)
    try:
        check("set changes the curve under a running session",
              muse.args.up == 0.25 and muse.args.down == saved[2], (muse.args.up, muse.args.down))
        check("a new start level moves the lamps there now",
              m.settle.level == 0.5 and muse.args.start == 0.5, m.settle.level)
    finally:
        muse.args.start, muse.args.up, muse.args.down = saved


def baselines():
    print("baselines")
    # stillness barely moves while it is learned, the way a 12 s average does
    still = muse.Baseline(True, ratio=True)
    for i in range(110):
        still.observe(1.38e-3 + 1e-5 * math.sin(i))
    still.freeze()
    check("a breath more movement than at arrival is not total tension",
          still.score(1.8e-3) > 0.3, f"{still.score(1.8e-3):.3f}")
    check("twice the movement is one step tenser", abs(still.score(2.76e-3) - 0.269) < 0.01,
          f"{still.score(2.76e-3):.3f}")
    check("half the movement is one step calmer", abs(still.score(0.69e-3) - 0.731) < 0.01,
          f"{still.score(0.69e-3):.3f}")
    hr = muse.Baseline(True, floor=5.0)
    for i in range(110):
        hr.observe(70 + 0.5 * math.sin(i))
    hr.freeze()
    check("a few beats faster is not total tension", hr.score(76) > 0.2, f"{hr.score(76):.3f}")


def control_packets(text):
    """Muse control replies: 20-byte packets, length byte then <=19 chars."""
    out = []
    for i in range(0, len(text), 19):
        chunk = text[i:i + 19].encode()
        out.append(bytearray([len(chunk)]) + chunk + b"\x00" * (19 - len(chunk)))
    return out

def eeg_packet(idx, t0, ch, amp=40):
    vals = [2048 + int(amp * math.sin(2 * math.pi * 10 * (t0 + k / 256.0) + ch))
            for k in range(12)]
    bits = 0
    for v in vals:
        bits = (bits << 12) | (v & 0xFFF)
    return bytearray(idx.to_bytes(2, "big") + bits.to_bytes(18, "big"))

class FakeClient:
    behaviour = {}
    last = None
    races = 0                     # connects still to lose to BlueZ's race

    def __init__(self, device, disconnected_callback=None, timeout=None):
        self.cb = disconnected_callback
        self.notify = {}
        self.writes = []          # (t, cmd)
        self.delivered = []       # (t, reply)
        self.disconnect_calls = 0
        self.streaming = None
        self.b = dict(FakeClient.behaviour)
        FakeClient.last = self

    async def connect(self):
        mode = self.b.get("connect", "ok")
        if mode == "raise":
            raise RuntimeError("failed to discover services, device disconnected")
        if mode == "hang":
            await asyncio.sleep(3600)
        if mode == "race" and FakeClient.races > 0:
            FakeClient.races -= 1
            raise RuntimeError("failed to discover services, device disconnected")

    async def disconnect(self):
        self.disconnect_calls += 1
        if self.streaming:
            self.streaming.cancel()

    async def start_notify(self, uuid, handler):
        self.notify[uuid] = handler

    async def write_gatt_char(self, uuid, data, response=False):
        cmd = bytes(data[1:-1]).decode()
        self.writes.append((time.monotonic(), cmd))
        replies = {
            "v1": ('{"ap":"headset","sp":"Blackcomb_revB","fw":"1.0.27","rc":0}', 0.05),
            "p21": ('{"rc":0}', self.b.get("preset_delay", 0.4)),
            "s": ('{"hn":"Muse-3670","sn":"4302","bp":55,"rc":0}', self.b.get("s_delay", 0.8)),
            "d": ('{"rc":0}', 0.05),
            "k": ('{"rc":0}', 0.05),
            "h": ('{"rc":0}', 0.05),
        }
        if cmd in replies:
            text, delay = replies[cmd]
            asyncio.get_running_loop().call_later(delay, self._reply, text)
        if cmd == "d" and not self.b.get("silent"):
            self.streaming = asyncio.create_task(self._stream())

    def _reply(self, text):
        self.delivered.append((time.monotonic(), text))
        for pkt in control_packets(text):
            self.notify[muse.CONTROL](None, pkt)

    async def _stream(self):
        idx = {ch: 100 for ch in muse.EEG_CHANS}
        skip = self.b.get("skip_every")        # drop every Nth packet on TP9
        off_after = self.b.get("off_after")    # taken off a head: ~2uV, not ~40
        n = 0
        t = 0.0
        started = time.monotonic()
        try:
            while time.monotonic() - started < self.b.get("stream_for", 4.0):
                amp = 2 if off_after is not None and t >= off_after else 40
                for i, (ch, uuid) in enumerate(muse.EEG_CHANS.items()):
                    idx[ch] = (idx[ch] + 1) & 0xFFFF
                    if skip and ch == "TP9" and n % skip == 0:
                        continue                   # the headband sent it; we lost it
                    self.notify[uuid](None, eeg_packet(idx[ch], t, i, amp))
                acc = (0).to_bytes(2, "big") + b"".join(
                    int(v).to_bytes(2, "big", signed=True) for v in (0, 0, 16384) * 3)
                self.notify[muse.ACCEL](None, bytearray(acc))
                n += 1
                t += 12 / 256.0
                await asyncio.sleep(12 / 256.0)
        except asyncio.CancelledError:
            return
        if self.cb:
            self.cb(self)                           # the headband hangs up

class Sink:
    def __init__(self): self.frames = []
    def write(self, line): self.frames.append(json.loads(line))

muse.BleakClient = FakeClient
muse.CONNECT_TIMEOUT = 1.0
dev = type("D", (), {"address": "00:55:DA:B8:36:70", "name": "Muse-3670"})()

async def lifecycle():
    # 1. a connect that raises must still tear down
    FakeClient.behaviour = {"connect": "raise"}
    try:
        await muse.run_session(dev, muse.Muse())
        check("failed connect propagates", False, "no exception")
    except RuntimeError:
        check("failed connect propagates", True)
    check("failed connect is torn down", FakeClient.last.disconnect_calls == 1,
          f"disconnect called {FakeClient.last.disconnect_calls}x")

    # 2. a connect that hangs must time out AND tear down
    FakeClient.behaviour = {"connect": "hang"}
    t0 = time.monotonic()
    try:
        await muse.run_session(dev, muse.Muse())
        check("hung connect times out", False, "returned")
    except asyncio.TimeoutError:
        check("hung connect times out", time.monotonic() - t0 < 3, f"{time.monotonic()-t0:.1f}s")
    check("hung connect is torn down", FakeClient.last.disconnect_calls == 1,
          f"disconnect called {FakeClient.last.disconnect_calls}x")

    # 3. the race from the log: late preset ack must not release 'd' early
    FakeClient.behaviour = {"preset_delay": 0.4, "s_delay": 0.8, "stream_for": 1.0}
    muse.writer = Sink()
    await muse.run_session(dev, muse.Muse())
    c = FakeClient.last
    d_at = next(t for t, cmd in c.writes if cmd == "d")
    s_reply = next(t for t, r in c.delivered if '"hn"' in r)
    check("'d' waits for the status reply", d_at >= s_reply,
          f"d at +{d_at - s_reply:+.3f}s relative to status reply")
    order = [cmd for _, cmd in c.writes if cmd in ("v1", "p21", "s", "d")]
    check("commands go out in order", order == ["v1", "p21", "s", "d"], order)

    # 4. worse: preset ack later than its own timeout, arriving mid-'s'-wait
    FakeClient.behaviour = {"preset_delay": 2.3, "s_delay": 0.6, "stream_for": 1.0}
    await muse.run_session(dev, muse.Muse())
    c = FakeClient.last
    d_at = next(t for t, cmd in c.writes if cmd == "d")
    s_reply = next(t for t, r in c.delivered if '"hn"' in r)
    check("a stray late ack cannot stand in for the status reply", d_at >= s_reply,
          f"d at {d_at - s_reply:+.3f}s relative to status reply")

    # 5. clean stream: full rate, no loss, frames produced, torn down at the end
    FakeClient.behaviour = {"preset_delay": 0.05, "s_delay": 0.05, "stream_for": 4.0}
    m = muse.Muse()
    muse.writer = Sink()
    await muse.run_session(dev, m)
    frames = muse.writer.frames
    live = [f for f in frames if f["pps"] > 0]
    check("frames produced", len(frames) > 10, f"{len(frames)} frames")
    check("full rate measured", live and 75 < live[-1]["pps"] < 95,
          f"pps {live[-1]['pps'] if live else None}")
    check("no loss on a clean stream", m.lost == 0, f"lost {m.lost}")
    check("session torn down", FakeClient.last.disconnect_calls >= 1)
    check("drop counted", m.drops == 1, m.drops)

    # 6. lossy stream: gaps counted, buffer stays time-correct
    FakeClient.behaviour = {"preset_delay": 0.05, "s_delay": 0.05,
                            "stream_for": 4.0, "skip_every": 4}
    m = muse.Muse()
    muse.writer = Sink()
    await muse.run_session(dev, m)
    frames = muse.writer.frames
    check("lost packets counted", m.lost > 5, f"lost {m.lost}")
    lossy = [f for f in frames if f.get("loss", 0) > 0]
    check("loss reported in frames", bool(lossy),
          f"loss {lossy[-1]['loss'] if lossy else None}")
    tp9, af7 = len(m.eeg["TP9"]), len(m.eeg["AF7"])
    check("gaps are bridged, not spliced", abs(tp9 - af7) <= 12,
          f"TP9 {tp9} vs AF7 {af7} samples")
    amps = [f["channels"]["TP9"] for f in frames if f["channels"].get("TP9")]
    check("no step artifacts from gaps", amps and max(amps) < 300,
          f"TP9 max {max(amps) if amps else None:.0f}uV (signal is 80uV p2p)")

    # 7. watcher: a silent session must not borrow the previous session's packets
    FakeClient.behaviour = {"preset_delay": 0.05, "s_delay": 0.05,
                            "silent": True, "stream_for": 0}
    m = muse.Muse()
    m.total_packets = 500                # left over from an earlier session
    records = []
    class H(__import__("logging").Handler):
        def emit(self, r): records.append(r.getMessage())
    h = H(); muse.log.addHandler(h)
    async def hangup():
        await asyncio.sleep(1.5)
        FakeClient.last.cb(FakeClient.last)
    asyncio.create_task(hangup())
    await muse.run_session(dev, m)
    await asyncio.sleep(0.2)
    muse.log.removeHandler(h)
    check("silent session is not reported as streaming",
          not any("first eeg packet" in r for r in records), [r for r in records if "eeg" in r])
    pending = [t for t in asyncio.all_tasks() if "_first_data_watch" in repr(t)]
    check("watcher cancelled with its session", not pending, f"{len(pending)} still running")



async def stability():
    print("stability ledger")
    muse.RETRY_DELAY = 0.2

    async def one_muse(address=None):
        return [dev]
    muse.find_muse = one_muse

    # a connect that fails is an attempt and a failure, not a link
    FakeClient.behaviour = {"connect": "raise"}
    m = muse.Muse()
    try:
        await muse.run_session(dev, m)
    except RuntimeError:
        pass
    st = m.stability
    check("failed connect counted", st.attempts == 1 and sum(st.failures.values()) == 1,
          f"attempts {st.attempts}, failures {st.failures}")
    check("failed connect is not a link", not st.sessions and st.drops == 0)

    # a link the headband ends is a drop, with its length on the books
    FakeClient.behaviour = {"preset_delay": 0.05, "s_delay": 0.05, "stream_for": 1.0}
    m = muse.Muse()
    muse.writer = Sink()
    await muse.run_session(dev, m)
    st = m.stability
    check("dropped link counted", st.drops == 1 and len(st.sessions) == 1,
          f"drops {st.drops}, sessions {st.sessions}")
    check("link length recorded", st.sessions and 0.8 < st.sessions[0] < 3.0,
          f"{st.sessions}")

    muse.LOCK_PATH = os.path.join(tempfile.mkdtemp(), "muse.lock")
    muse.BLUETOOTHCTL = "no-such-bluetoothctl"
    # a soak that ends on its own timer must not count its own shutdown as a drop
    saved = sys.stdout
    sys.stdout = open(os.devnull, "w")
    try:
        FakeClient.behaviour = {"preset_delay": 0.05, "s_delay": 0.05, "stream_for": 60}
        muse.args.soak = 3 / 60
        records = []

        class H(__import__("logging").Handler):
            def emit(self, r):
                records.append(r.getMessage())
        h = H()
        muse.log.addHandler(h)
        await muse.main()
        muse.log.removeHandler(h)
    finally:
        sys.stdout.close()
        sys.stdout = saved
        muse.args.soak = None
    summary = [r for r in records if r.startswith("stability over")]
    check("soak prints a summary", bool(summary), summary)
    check("soak's own shutdown is not a drop",
          any("0 drop(s)" in r for r in records), [r for r in records if "drop" in r])
    up = float(summary[0].split("link up ")[1].split("%")[0]) if summary else 0
    check("uptime measured", up > 60, f"{up}% (connect takes part of a 3s soak)")

    # a flapping link: several drops, each one on the books
    sys.stdout = open(os.devnull, "w")
    try:
        FakeClient.behaviour = {"preset_delay": 0.05, "s_delay": 0.05, "stream_for": 0.8}
        muse.args.soak = 5 / 60
        records.clear()
        muse.log.addHandler(h)
        await muse.main()
        muse.log.removeHandler(h)
    finally:
        sys.stdout.close()
        sys.stdout = saved
        muse.args.soak = None
    drops = [r for r in records if "drop(s)" in r and not r.startswith("stability:")]
    n = int(drops[0].split()[0]) if drops else 0
    check("flapping link counts every drop", n >= 2, drops)


async def off_head():
    print("off a head")
    muse.RETRY_DELAY = 0.2
    muse.BLUETOOTHCTL = "no-such-bluetoothctl"
    hold, drop = muse.OFF_HEAD_HOLD, muse.SESSION_DROP
    muse.OFF_HEAD_HOLD, muse.SESSION_DROP = 0.5, 2.0
    try:
        # worn for 3s, then taken off and left lying there switched on
        FakeClient.behaviour = {"preset_delay": 0.05, "s_delay": 0.05,
                                "stream_for": 8.0, "off_after": 3.0}
        m = muse.Muse()
        muse.writer = Sink()
        t0 = time.monotonic()
        await asyncio.wait_for(muse.run_session(dev, m), 20)
        frames = muse.writer.frames
        check("worn while on a head", any(f["worn"] for f in frames))
        check("not worn once taken off", not frames[-1]["worn"], frames[-1])
        check("the link stays up while it lies there switched on",
              time.monotonic() - t0 > 7.5 and m.drops == 1,
              f"{time.monotonic() - t0:.1f}s, drops {m.drops}")
        check("and the visitor is forgotten", m.settle.t0 is None)

        # never put on: nothing is worn and nobody is a visitor
        FakeClient.behaviour = {"preset_delay": 0.05, "s_delay": 0.05,
                                "stream_for": 5.0, "off_after": 0.0}
        m = muse.Muse()
        muse.writer = Sink()
        await muse.run_session(dev, m)
        frames = muse.writer.frames
        check("never worn is not worn", frames and not any(f["worn"] for f in frames))
        check("and starts no visitor", m.settle.t0 is None)
    finally:
        muse.OFF_HEAD_HOLD, muse.SESSION_DROP = hold, drop

    print("reconnects")
    # three connects lost to BlueZ's race, then a link that drops, then a
    # headband that is switched off and never seen again
    scans = []

    async def then_gone(address=None):
        scans.append(1)
        return [dev] if len(scans) == 1 else []
    muse.find_muse = then_gone
    records = []

    class H(__import__("logging").Handler):
        def emit(self, r):
            records.append(r.getMessage())
    h = H()
    muse.log.addHandler(h)
    try:
        FakeClient.races = 3
        FakeClient.behaviour = {"connect": "race", "preset_delay": 0.05,
                                "s_delay": 0.05, "stream_for": 1.0}
        m = muse.Muse()
        muse.writer = Sink()
        try:
            await asyncio.wait_for(muse._run(m), 5)
            kept = False
        except asyncio.TimeoutError:
            kept = True
        st = m.stability
        check("lost races are retried at once, not rescanned",
              st.attempts == 4 and len(st.sessions) == 1, f"attempts {st.attempts}, links {len(st.sessions)}")
        check("a headband that goes away is scanned for, not given up on",
              kept and len(scans) > 3, f"{len(scans)} scans")
        missing = [r for r in records if r.startswith("no Muse in range")]
        check("and said once, not on every scan", len(missing) == 1, missing)
    finally:
        muse.log.removeHandler(h)
        FakeClient.races = 0

    print("bridge brightness")
    a = muse_bridge.args
    a.min_value, a.max_value = 13, 200
    check("off a head is dark", muse_bridge._target({"worn": False, "settle": 0.8}) == 0)
    check("on a head, unsettled, is 5%", muse_bridge._target({"worn": True, "settle": 0.0}) == 13)
    check("on a head, settled, is full", muse_bridge._target({"worn": True, "settle": 1.0}) == 200)
    check("nothing heard yet is dark", muse_bridge._target({}) == 0)
    now = 1000.0
    held = muse_bridge._holding
    check("worn, the lamps are the headband's", held({"worn": True, "_seen": now}, now))
    check("off a head they are left to the mic", not held({"worn": False, "_seen": now}, now))
    check("a lost link holds them for a reconnect",
          held({"worn": True, "_seen": now - 20}, now))
    check("but not for good", not held({"worn": True, "_seen": now - 31}, now))
    check("and nothing heard yet holds nothing", not held({}, now))


HARNESS = """
import os, sys, asyncio
sys.path.insert(0, %r)
import test_muse
from test_muse import muse, FakeClient, dev
muse.LOCK_PATH = os.environ["LOCK"]
muse.LOCK_WAIT = float(os.environ.get("LOCK_WAIT", "12"))
muse.BLUETOOTHCTL = os.environ.get("BTCTL", "no-such-bluetoothctl")
muse.RETRY_DELAY = 0.2
FakeClient.behaviour = {"preset_delay": 0.05, "s_delay": 0.05, "stream_for": 600}
async def one(address=None):
    return [dev]
muse.find_muse = one
disconnect, write = FakeClient.disconnect, FakeClient.write_gatt_char
async def loud_disconnect(self):
    print("FAKE disconnect", file=sys.stderr, flush=True)
    await disconnect(self)
async def loud_write(self, uuid, data, response=False):
    if bytes(data[1:-1]) == b"h":
        print("FAKE halt", file=sys.stderr, flush=True)
    await write(self, uuid, data, response)
FakeClient.disconnect, FakeClient.write_gatt_char = loud_disconnect, loud_write
try:
    asyncio.run(muse.main())
except KeyboardInterrupt:
    pass
"""


def shutdown():
    print("shutdown and exclusivity (real processes, real signals)")
    work = tempfile.mkdtemp()
    harness = os.path.join(work, "harness.py")
    with open(harness, "w") as f:
        f.write(HARNESS % HERE)
    lock = os.path.join(work, "muse.lock")

    def spawn(stdin=None, **env):
        # a background job inherits SIGINT as ignored — reproduce that
        p = subprocess.Popen(
            [sys.executable, "-u", harness], stdin=stdin, stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE, text=True, env=dict(os.environ, LOCK=lock, **env),
            preexec_fn=lambda: signal.signal(signal.SIGINT, signal.SIG_IGN))
        lines = []
        threading.Thread(target=lambda: [lines.append(l.rstrip()) for l in p.stderr],
                         daemon=True).start()
        return p, lines

    def saw(lines, text, timeout):
        end = time.time() + timeout
        while time.time() < end:
            if any(text in l for l in lines):
                return True
            time.sleep(0.05)
        return False

    for sig in (signal.SIGTERM, signal.SIGINT):
        name = signal.Signals(sig).name
        p, lines = spawn()
        streaming = saw(lines, "streaming", 10)
        p.send_signal(sig)
        try:
            code = p.wait(timeout=12)
        except subprocess.TimeoutExpired:
            p.kill()
            code = None
        check(f"{name} stops the session", streaming and code == 0, f"exit {code}")
        check(f"{name} halts the headband", saw(lines, "FAKE halt", 1))
        check(f"{name} disconnects", saw(lines, "FAKE disconnect", 1))
        check(f"{name} prints the summary", saw(lines, "stability over", 1))

    # a second session while one is running is refused, not left to fight
    a, a_lines = spawn()
    saw(a_lines, "streaming", 10)
    b, b_lines = spawn(LOCK_WAIT="1")
    try:
        b_code = b.wait(timeout=8)
    except subprocess.TimeoutExpired:
        b.kill()
        b_code = None
    check("second session is refused", b_code == 1, f"exit {b_code}")
    check("and says why", saw(b_lines, "already running", 1))

    # Stop then Start straight away: the new one waits out the old teardown
    a.send_signal(signal.SIGTERM)
    c, c_lines = spawn(LOCK_WAIT="10")
    check("start right after stop gets the headband", saw(c_lines, "streaming", 15))
    a.wait(timeout=10)
    c.send_signal(signal.SIGTERM)
    c.wait(timeout=10)

    # a link left behind by a killed run is released before scanning
    btlog = os.path.join(work, "bt.log")
    btctl = os.path.join(work, "bluetoothctl")
    with open(btctl, "w") as f:
        f.write('#!/bin/sh\n'
                'if [ "$1" = devices ]; then echo "Device 00:55:DA:B8:36:70 Muse-3670"; fi\n'
                f'if [ "$1" = disconnect ]; then echo "$2" >> {btlog}; fi\n')
    os.chmod(btctl, 0o755)
    d, d_lines = spawn(BTCTL=btctl)
    saw(d_lines, "streaming", 10)
    d.send_signal(signal.SIGTERM)
    d.wait(timeout=10)
    released = open(btlog).read().split() if os.path.exists(btlog) else []
    check("stale link released before scanning", "00:55:DA:B8:36:70" in released, released)
    check("and logged", saw(d_lines, "still holding Muse-3670", 1))

    # the page's Reset baseline: a line down the pipe main.py holds on stdin
    r, r_lines = spawn(stdin=subprocess.PIPE)
    saw(r_lines, "new session", 10)
    r.stdin.write("reset\n")
    r.stdin.flush()
    check("reset on stdin learns the baseline again", saw(r_lines, "baseline reset by hand", 3))
    end = time.time() + 3
    while time.time() < end and sum("new session" in l for l in r_lines) < 2:
        time.sleep(0.05)
    check("and starts a new session", sum("new session" in l for l in r_lines) >= 2)
    check("without touching the link", not any("FAKE disconnect" in l for l in r_lines))
    r.send_signal(signal.SIGTERM)
    r.wait(timeout=10)


def bridge_reconnect():
    print("bridge")
    got = []

    def serve(stop):
        if os.path.exists(SOCK):
            os.unlink(SOCK)
        srv = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        srv.bind(SOCK)
        srv.listen(2)
        srv.settimeout(0.2)
        conns = []
        while not stop.is_set():
            try:
                c, _ = srv.accept()
                c.settimeout(0.1)
                conns.append(c)
            except socket.timeout:
                pass
            for c in conns:
                try:
                    got.extend(l for l in c.recv(4096).decode().splitlines() if l)
                except Exception:
                    pass
        for c in conns:
            c.close()
        srv.close()

    muse_bridge.RETRY_DELAY = 0.2
    stop = threading.Event()
    t = threading.Thread(target=serve, args=(stop,), daemon=True)
    t.start()
    time.sleep(0.3)
    muse_bridge._open_sock()
    muse_bridge._send({"cmd": "aubio_shimmer", "value": 1})
    time.sleep(0.3)
    check("frames reach serial_bridge", any('"value": 1' in g for g in got))

    stop.set()
    t.join()                                   # sparkles restarts under us
    for _ in range(3):
        muse_bridge._send({"cmd": "aubio_shimmer", "value": 2})
        time.sleep(0.1)
    stop2 = threading.Event()
    t2 = threading.Thread(target=serve, args=(stop2,), daemon=True)
    t2.start()
    deadline = time.time() + 3
    while time.time() < deadline and not any('"value": 3' in g for g in got):
        muse_bridge._send({"cmd": "aubio_shimmer", "value": 3})
        time.sleep(0.2)
    stop2.set()
    t2.join()
    check("reconnects after a sparkles restart", any('"value": 3' in g for g in got))


def bridge_holds():
    print("bridge lamps only while worn (real process)")
    p = subprocess.Popen([sys.executable, "-u", os.path.join(HERE, "muse_bridge.py"),
                          "--dry-run", "--status"],
                         stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                         stderr=subprocess.DEVNULL, text=True)
    out = []
    threading.Thread(target=lambda: [out.append((time.time(), json.loads(l))) for l in p.stdout],
                     daemon=True).start()

    def feed(worn, secs):
        t0 = time.time()
        while time.time() - t0 < secs:
            p.stdin.write(json.dumps({"worn": worn, "settle": 0.5, "phase": "live",
                                      "contact": 4 if worn else 0}) + "\n")
            p.stdin.flush()
            time.sleep(0.1)
        return t0

    lying = feed(False, 1.0)
    worn = feed(True, 1.0)
    off = feed(False, 1.5)
    p.stdin.close()
    p.wait(timeout=5)
    shimmer = [t for t, f in out if f.get("cmd") == "aubio_shimmer"]
    told = [t for t, f in out if f.get("event") == "brain_status" and f.get("worn")]
    check("lying around, nothing is sent to the lamps",
          not any(lying <= t < worn for t in shimmer), len(shimmer))
    check("worn, the lamps are driven", any(worn <= t < off for t in shimmer))
    check("and main.py hears of it at once, not on the next tick",
          told and told[0] - worn < 0.4, f"{(told[0] - worn) if told else None}")
    check("taken off, the frames stop and the lamps fade by themselves",
          not any(t > off + 0.3 for t in shimmer))


def music_gate():
    print("music gate")
    # importing builds the module's own bridge; keep it off the real serial log
    os.environ["SPARKLES_SERIAL_LOG"] = os.path.join(tempfile.mkdtemp(), "serial.log")
    import serial_bridge
    b = serial_bridge.SerialBridge.__new__(serial_bridge.SerialBridge)
    b._running, b.music_paused = True, False
    b.colors = json.loads(json.dumps(serial_bridge._DEFAULT_COLORS))
    sent = []
    b.send_line = sent.append

    def client(hello=None):
        ours, theirs = socket.socketpair()
        threading.Thread(target=b._handle_music_client, args=(theirs,), daemon=True).start()
        if hello:
            ours.sendall(hello)
        return ours

    def say(conn, cmd):
        conn.sendall((json.dumps(cmd) + "\n").encode())
        time.sleep(0.1)

    muse_c = client(b'{"hello": "muse"}\n')
    aubio = client()
    b.music_paused = True                          # the headband goes on
    say(aubio, {"cmd": "aubio_midi", "note": 60, "velocity": 90})
    say(muse_c, {"cmd": "aubio_shimmer", "value": 40})
    b.music_paused = False                         # and comes off
    say(aubio, {"cmd": "aubio_midi", "note": 62, "velocity": 90})
    muse_c.close()
    aubio.close()
    check("a headband on a head keeps pitch and keyboard off the lamps",
          not any('"note": 60' in s for s in sent), sent)
    check("while the Muse frames still get through", any('"value": 40' in s for s in sent), sent)
    check("and they are back once it comes off", any('"note": 62' in s for s in sent), sent)
    check("the bridge's hello never reaches the master", not any("hello" in s for s in sent), sent)


if __name__ == "__main__":
    decode()
    settle()
    asyncio.run(commands())
    baselines()
    print("ble lifecycle")
    asyncio.run(lifecycle())
    asyncio.run(stability())
    asyncio.run(off_head())
    shutdown()
    bridge_reconnect()
    bridge_holds()
    music_gate()
    print()
    if failures:
        print(f"{R}{len(failures)} failed{X}: {', '.join(failures)}")
        sys.exit(1)
    print(f"{G}all passed{X}")
