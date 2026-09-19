"""
muse_bridge.py — Muse settling score → lamp brightness. Test rig.

Reads `muse.py --json` on stdin and drives every lamp's brightness from the
`settle` score, over the existing music socket:

    music socket → serial_bridge.send_line → master → BACKGROUND_SHIMMER broadcast

This is the deliberately dumb version for bringing the thing up on one or two
lamps. It ignores position and distance entirely — one broadcast, every board
renders the same brightness, so whatever is powered on lights up. Colour is
left alone too: serial_bridge stamps the configured shimmer hue onto every
aubio_shimmer frame, and since we only touch `value` that works in our favour
and nothing in the running system needs changing.

The whole mapping is one line — brightness = settle — on purpose. If a lamp is
dark you know the score is low, not that some animation curve ate it.

Run by hand, lamps live and everything on screen (stdout carries the lamp
stream, both scripts log to stderr, so one terminal shows the whole chain):

    python muse.py --json --preset p21 | python muse_bridge.py --diag

    lamp   0/255  settle 0.00  anchor  contact 4/4  link  62.3s  drops 0
      pps  85.1  bpm  61  bat  58%  | TP9 44  AF7 51  AF8 39  TP10 47

pps is the one to watch: a healthy link delivers ~85 eeg packets/s, and a stall
shows up there before the connection actually dies. Other modes:

    python muse.py --json --ppg | python muse_bridge.py           # quiet
    python muse.py --json | python muse_bridge.py --dry-run       # no lamps

Ctrl-c sends animation_off so the idle animation loop takes the lamps back —
BACKGROUND_SHIMMER is endless, and the loop will otherwise wait it out forever.
"""

import argparse
import json
import logging
import math
import os
import socket
import sys
import threading
import time

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s %(levelname)s %(message)s",
)
log = logging.getLogger("muse_bridge")

RETRY_DELAY = 3.0
RESEND_AFTER = 0.25   # force a frame this often even when nothing has changed

parser = argparse.ArgumentParser(description="Muse settling score → Sparkles lamps")
parser.add_argument("--sock",       default=os.environ.get("SPARKLES_MUSIC_SOCK", "/tmp/music.sock"),
                    help="Unix socket path to serial_bridge's music socket")
parser.add_argument("--fps",        type=float, default=20.0,
                    help="Output frame rate (default 20)")
parser.add_argument("--min-value",  type=int, default=0,
                    help="Brightness at settle 0 (default 0, lamps fully dark)")
parser.add_argument("--max-value",  type=int, default=200,
                    help="Brightness at settle 1 (default 200)")
parser.add_argument("--smooth",     type=float, default=0.6,
                    help="Seconds to catch up to a change in settle")
parser.add_argument("--diag",       action="store_true",
                    help="Print a full diagnostic line per second on STDERR, while "
                         "still driving the lamps — for running this by hand")
parser.add_argument("--status",     action="store_true",
                    help="Print one status JSON line per second on stdout (FastAPI reads these)")
parser.add_argument("--dry-run",    action="store_true",
                    help="Print every frame instead of sending it")
args = parser.parse_args()

_sock: socket.socket | None = None
_sock_lock = threading.Lock()

state = {}
state_lock = threading.Lock()
running = True


def _open_sock():
    """Block until the bridge is up, same as keyboard_midi does."""
    global _sock
    while running:
        try:
            s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            s.connect(args.sock)
            with _sock_lock:
                _sock = s
            log.info("Connected to music bridge at %s", args.sock)
            return
        except Exception as e:
            log.error("Music bridge connect failed: %s — retrying in %.0fs", e, RETRY_DELAY)
            time.sleep(RETRY_DELAY)


def _send(cmd: dict):
    if args.dry_run:
        return
    with _sock_lock:
        s = _sock
    if s is None:
        return
    try:
        s.sendall((json.dumps(cmd) + "\n").encode())
    except Exception as e:
        log.error("Music bridge send failed: %s", e)


def _reader():
    global running
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            msg = json.loads(line)
        except ValueError:
            continue
        with state_lock:
            state.update(msg)
            state["_seen"] = time.monotonic()
    running = False


def main():
    global running

    if not args.dry_run:
        _open_sock()
    threading.Thread(target=_reader, daemon=True, name="muse-stdin").start()

    period = 1.0 / args.fps
    span = args.max_value - args.min_value
    value = float(args.min_value)
    last = time.monotonic()
    next_at = last
    last_frame = None
    last_sent = 0.0
    last_status = 0.0

    while running:
        next_at += period
        time.sleep(max(0.0, next_at - time.monotonic()))
        now = time.monotonic()
        dt = min(now - last, 0.25)
        last = now

        with state_lock:
            s = dict(state)

        settle = float(s.get("settle", 0.0) or 0.0)
        target = args.min_value + span * max(0.0, min(1.0, settle))
        # settle already moves over minutes; this only takes the stair-step out
        # of the 10 Hz input so the lamps ramp instead of clicking.
        a = 1.0 - math.exp(-dt / args.smooth) if args.smooth > 0 else 1.0
        value += (target - value) * a

        frame = {"cmd": "aubio_shimmer", "value": int(round(max(0, min(255, value))))}
        if frame != last_frame or now - last_sent > RESEND_AFTER:
            _send(frame)
            last_frame = frame
            last_sent = now
            if args.dry_run:
                print(json.dumps(frame), flush=True)

        if now - last_status > 1.0:
            last_status = now
            if args.status:
                print(json.dumps({
                    "event":   "brain_status",
                    # connected is not the same as usable: frames can be
                    # arriving from a headband that is barely touching skin.
                    "connected": (now - s.get("_seen", 0.0)) < 3.0,
                    "channels": s.get("channels", {}),
                    "settle":  round(settle, 4),
                    "value":   frame["value"],
                    "phase":   s.get("phase", "waiting"),
                    "session": s.get("session", 0.0),
                    "contact": s.get("contact", 0),
                    "bpm":     s.get("bpm"),
                    "battery": s.get("battery"),
                }), flush=True)
            elif args.diag and (now - s.get("_seen", 0.0)) > 2.0:
                # Say so rather than reprinting the last good frame forever --
                # stale numbers presented as live is worse than no numbers.
                gap = now - s["_seen"] if s.get("_seen") else 0.0
                print(f"lamp {frame['value']:3d}/255  NO DATA for {gap:4.1f}s  "
                      f"(last: contact {s.get('contact', 0)}/4, "
                      f"link {s.get('link', 0.0):.1f}s, drops {s.get('drops', 0)})",
                      file=sys.stderr, flush=True)
            elif args.diag:
                # stderr on purpose: stdout is the lamp stream when piped, and
                # muse.py logs here too, so one terminal shows the whole chain.
                ch = "  ".join(
                    f"{name} {'--' if uv is None else format(uv, '.0f')}"
                    for name, uv in (s.get("channels") or {}).items())
                link = s.get("link", 0.0)
                print(f"lamp {frame['value']:3d}/255  settle {settle:.2f}  "
                      f"{s.get('phase', 'waiting'):<7} "
                      f"contact {s.get('contact', 0)}/4  "
                      f"link {link:5.1f}s  drops {s.get('drops', 0)}  "
                      f"pps {s.get('pps', 0):5.1f}  "
                      f"bpm {s.get('bpm') or 0:3.0f}  bat {s.get('battery') or 0:3.0f}%  "
                      f"| {ch}", file=sys.stderr, flush=True)
            else:
                log.info("settle %.2f → value %3d   phase %-8s contact %s/4",
                         settle, frame["value"], s.get("phase", "waiting"),
                         s.get("contact", 0))

    log.info("input ended — handing the lamps back to the idle loop")
    _send({"cmd": "animation_off"})


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        running = False
        log.info("stopping — handing the lamps back to the idle loop")
        _send({"cmd": "animation_off"})
        print()
