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
import json
import math
import os
import socket
import sys
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


def control_packets(text):
    """Muse control replies: 20-byte packets, length byte then <=19 chars."""
    out = []
    for i in range(0, len(text), 19):
        chunk = text[i:i + 19].encode()
        out.append(bytearray([len(chunk)]) + chunk + b"\x00" * (19 - len(chunk)))
    return out

def eeg_packet(idx, t0, ch):
    vals = [2048 + int(40 * math.sin(2 * math.pi * 10 * (t0 + k / 256.0) + ch))
            for k in range(12)]
    bits = 0
    for v in vals:
        bits = (bits << 12) | (v & 0xFFF)
    return bytearray(idx.to_bytes(2, "big") + bits.to_bytes(18, "big"))

class FakeClient:
    behaviour = {}
    last = None

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
        n = 0
        t = 0.0
        started = time.monotonic()
        try:
            while time.monotonic() - started < self.b.get("stream_for", 4.0):
                for i, (ch, uuid) in enumerate(muse.EEG_CHANS.items()):
                    idx[ch] = (idx[ch] + 1) & 0xFFFF
                    if skip and ch == "TP9" and n % skip == 0:
                        continue                   # the headband sent it; we lost it
                    self.notify[uuid](None, eeg_packet(idx[ch], t, i))
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


if __name__ == "__main__":
    decode()
    settle()
    print("ble lifecycle")
    asyncio.run(lifecycle())
    asyncio.run(stability())
    bridge_reconnect()
    print()
    if failures:
        print(f"{R}{len(failures)} failed{X}: {', '.join(failures)}")
        sys.exit(1)
    print(f"{G}all passed{X}")
