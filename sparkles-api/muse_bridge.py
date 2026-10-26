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

The mapping is kept dumb on purpose: on a head the lamps shimmer at --min-value
(about 5%) plus settle's share of the way to --max-value. The lamps are only
the headband's while someone wears it. Off a head this bridge sends nothing at
all, the lamps fade out by themselves within half a second, and pitch
detection and the keyboard have them as if the headband were not there. While
it is worn they are held off the lamps instead (main.py tells serial_bridge,
which lets only this bridge's frames through).

Run by hand, lamps live and everything on screen (stdout carries the lamp
stream, both scripts log to stderr, so one terminal shows the whole chain):

    python muse.py --json --preset p21 | python muse_bridge.py --diag

    lamp   0/255  settle 0.00  anchor  contact 4/4  link  62.3s  drops 0
      pps  85.1  bpm  61  bat  58%  | TP9 44  AF7 51  AF8 39  TP10 47

pps is the one to watch: a healthy link delivers ~85 eeg packets/s, and a stall
shows up there before the connection actually dies. Other modes:

    python muse.py --json --ppg | python muse_bridge.py           # quiet
    python muse.py --json | python muse_bridge.py --dry-run       # no lamps

Stopping just stops the frames. animation_off is not sent: it switches the
master's idle animations off until someone starts them again, while silence
brings them back on their own after a few minutes, as when the music stops.
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
HOLD_THROUGH = 3.0    # seconds without frames before the lamps are let go
RECONNECT_HOLD = 30.0 # ...unless the link dropped on a head: held this long

parser = argparse.ArgumentParser(description="Muse settling score → Sparkles lamps")
parser.add_argument("--sock",       default=os.environ.get("SPARKLES_MUSIC_SOCK", "/tmp/music.sock"),
                    help="Unix socket path to serial_bridge's music socket")
parser.add_argument("--fps",        type=float, default=20.0,
                    help="Output frame rate (default 20)")
parser.add_argument("--min-value",  type=int, default=13,
                    help="Brightness on a head at settle 0 (default 13, about 5%%) — "
                         "off a head the lamps are dark")
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
_reconnecting = False

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
            # named, so our frames still reach the lamps while serial_bridge is
            # holding pitch detection and the keyboard off them
            s.sendall(b'{"hello": "muse"}\n')
            with _sock_lock:
                _sock = s
            log.info("Connected to music bridge at %s", args.sock)
            return
        except Exception as e:
            log.error("Music bridge connect failed: %s — retrying in %.0fs", e, RETRY_DELAY)
            time.sleep(RETRY_DELAY)


def _reconnect():
    """Reopen the socket off the frame loop, one attempt at a time.

    serial_bridge lives inside the sparkles service, so every restart — which
    update.sh does on each deploy — closes the socket under us. Without this
    every send failed from then on and the lamps stayed dark until someone
    restarted the bridge by hand."""
    global _reconnecting
    with _sock_lock:
        if _reconnecting:
            return
        _reconnecting = True

    def run():
        global _reconnecting
        try:
            _open_sock()
        finally:
            with _sock_lock:
                _reconnecting = False

    threading.Thread(target=run, daemon=True, name="music-reconnect").start()


def _send(cmd: dict):
    global _sock
    if args.dry_run:
        return
    with _sock_lock:
        s = _sock
    if s is None:
        return
    try:
        s.sendall((json.dumps(cmd) + "\n").encode())
    except Exception as e:
        log.error("Music bridge send failed: %s — reconnecting", e)
        with _sock_lock:
            if _sock is s:
                _sock = None
        try:
            s.close()
        except Exception:
            pass
        _reconnect()


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
        # every frame is whole, so take it whole: merged, a field that drops
        # out (a test's progress once it ends, a lost heart rate) went stale
        with state_lock:
            state.clear()
            state.update(msg)
            state["_seen"] = time.monotonic()
    running = False


def _holding(s: dict, now: float) -> bool:
    """The lamps are the headband's while someone is wearing it, and are let
    go, to fade out for the microphone and keyboard, HOLD_THROUGH seconds
    after the frames stop. Except when they stop with the electrodes on skin:
    people take the headband off before they switch it off, so that is a
    glitch in the link, and the lamps hold while muse.py reconnects. Nothing
    back after RECONNECT_HOLD means a flat battery or a switch after all."""
    seen = s.get("_seen")
    if not s.get("worn") or seen is None:
        return False
    quiet = now - seen
    if quiet < HOLD_THROUGH:
        return True
    return s.get("contact", 0) >= 2 and quiet < RECONNECT_HOLD


def _target(s: dict) -> float:
    """On a head the lamps never drop below min-value, so a visitor who has not
    settled yet can still see the lamps listening."""
    if not s.get("worn"):
        return 0.0
    settle = float(s.get("settle", 0.0) or 0.0)
    return args.min_value + (args.max_value - args.min_value) * max(0.0, min(1.0, settle))


def main():
    global running

    if not args.dry_run:
        _open_sock()
    threading.Thread(target=_reader, daemon=True, name="muse-stdin").start()

    period = 1.0 / args.fps
    value = 0.0
    last = time.monotonic()
    next_at = last
    last_frame = None
    last_sent = 0.0
    last_status = 0.0
    was_holding = False

    while running:
        next_at += period
        time.sleep(max(0.0, next_at - time.monotonic()))
        now = time.monotonic()
        dt = min(now - last, 0.25)
        last = now

        with state_lock:
            s = dict(state)

        holding = _holding(s, now)
        settle = float(s.get("settle", 0.0) or 0.0)
        if holding:
            # settle already moves over minutes; this only takes the stair-step
            # out of the 10 Hz input so the lamps ramp instead of clicking.
            a = 1.0 - math.exp(-dt / args.smooth) if args.smooth > 0 else 1.0
            value += (_target(s) - value) * a
        else:
            value = 0.0             # the next visitor's lamps come up from dark

        frame = {"cmd": "aubio_shimmer", "value": int(round(max(0, min(255, value))))}
        if holding and (frame != last_frame or now - last_sent > RESEND_AFTER):
            _send(frame)
            last_frame = frame
            last_sent = now
            if args.dry_run:
                print(json.dumps(frame), flush=True)

        # at once when the headband goes on or comes off, not on the next
        # tick, since main.py holds the microphone off the lamps by it
        if now - last_status > 1.0 or (args.status and holding != was_holding):
            last_status = now
            was_holding = holding
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
                    "good":    s.get("good", []),
                    # worn here means the lamps are the headband's right now
                    "worn":    holding,
                    "bpm":     s.get("bpm"),
                    "battery": s.get("battery"),
                    # what settle is made of, so the page can show why it moves
                    "evidence": s.get("evidence"),
                    "bias":    s.get("bias"),
                    "scores":  s.get("scores", {}),
                    # a guided test under way, and the last one's result
                    "recording": s.get("recording"),
                    "recorded": s.get("recorded"),
                }), flush=True)
            elif args.diag and (now - s.get("_seen", 0.0)) > 2.0:
                # Say so rather than reprinting the last good frame forever --
                # stale numbers presented as live is worse than no numbers.
                seen = s.get("_seen")
                why = (f"stream stopped {now - seen:.1f}s ago" if seen
                       else "nothing received yet — still connecting")
                print(f"lamp {frame['value']:3d}/255  NO DATA — {why}"
                      + (f"  (last: contact {s.get('contact', 0)}/4, "
                         f"link {s.get('link', 0.0):.1f}s, "
                         f"drops {s.get('drops', 0)})" if seen else ""),
                      file=sys.stderr, flush=True)
            elif args.diag:
                # stderr on purpose: stdout is the lamp stream when piped, and
                # muse.py logs here too, so one terminal shows the whole chain.
                ch = "  ".join(
                    f"{name} {'--' if uv is None else format(uv, '.0f')}"
                    for name, uv in (s.get("channels") or {}).items())
                link = s.get("link", 0.0)
                head = "on" if holding else "off, lamps left to the mic"
                print(f"lamp {frame['value']:3d}/255  settle {settle:.2f}  "
                      f"{s.get('phase', 'waiting'):<7} "
                      f"contact {s.get('contact', 0)}/4  head {head}  "
                      f"link {link:5.1f}s  drops {s.get('drops', 0)}  "
                      f"pps {s.get('pps', 0):5.1f}  loss {100 * (s.get('loss') or 0):3.0f}%  "
                      f"bpm {s.get('bpm') or 0:3.0f}  bat {s.get('battery') or 0:3.0f}%  "
                      f"| {ch}", file=sys.stderr, flush=True)
                sc = s.get("scores") or {}
                if sc:
                    ev = s.get("evidence")
                    print("      evidence " + ("--" if ev is None else f"{ev:.2f}")
                          + f" (need >{s.get('bias', 0.6):.2f})   "
                          + "  ".join(f"{k} {'--' if v is None else format(v, '.2f')}"
                                      for k, v in sc.items()),
                          file=sys.stderr, flush=True)
            else:
                log.info("settle %.2f → value %3d   phase %-8s contact %s/4",
                         settle, frame["value"], s.get("phase", "waiting"),
                         s.get("contact", 0))

    log.info("input ended")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        running = False
        log.info("stopping — the lamps fade out on their own")
        print()
