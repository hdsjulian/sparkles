#!/usr/bin/env python3
"""Talk to the Sparkles master directly over USB — no raspberry pi in the way.

The pi's FastAPI layer is a thin wrapper: every endpoint boils down to writing one
line of JSON to the master's serial port and reading its JSON events back. This
does exactly that, straight from the mac.

    ./sparkles-cli.py                     interactive, /help lists everything
    ./sparkles-cli.py sync_fast           one-shot: send, show the reply, exit
    ./sparkles-cli.py chirp_position x=1 y=2
    ./sparkles-cli.py --list              print the command table and exit

The command table is read out of the firmware's own serial handler, so it can
never drift from what the master actually accepts.
"""

import argparse
import json
import os
import re
import sys
import threading
import time

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    sys.exit("pyserial is missing:  pip3 install pyserial")

DEFAULT_PORT = "/dev/cu.usbmodem11201"
BAUD = 115200
FIRMWARE = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                        "src", "Master-Device", "main.cpp")

# The master dumps these every 5 seconds whether you asked or not. Muted unless
# --all, otherwise a hundred boards bury whatever you were actually looking at.
HOUSEKEEPING = {"heap", "animate_status", "num_devices", "update_board"}

C = {
    "reset": "\033[0m",   "bold": "\033[1m",   "dim": "\033[2m",
    "red": "\033[91m",    "green": "\033[92m", "yellow": "\033[93m",
    "blue": "\033[94m",   "magenta": "\033[95m", "cyan": "\033[96m",
}
if not sys.stdout.isatty():
    C = {k: "" for k in C}

# Only for grouping and one-liners in /help — the names themselves come from the
# firmware, so an undocumented command still shows up, just without a blurb.
GROUPS = [
    ("sync",        ["sync", "sync_all", "sync_fast", "reannounce", "timer_test"]),
    ("animation",   ["animate_toggle", "animation_off", "blink", "blink_all", "blink_battery_all",
                     "strobe_all", "candle_all", "breath", "bioluminescence", "hearth", "shimmer",
                     "set_sync_async_params", "darkroom", "set_darkroom_params"]),
    ("calibration", ["calibration_start", "calibration_cancel", "calibration_reset",
                     "calibration_continue", "calibration_end", "calibration_test",
                     "calibration_calibrate", "chirp_position", "dist_cal_start",
                     "dist_cal_continue", "dist_cal_end", "dist_cal_cancel", "dist_cal_abort"]),
    ("sleep",       ["sleep_until", "sleep_now", "sleep_until_cancel", "wake_now",
                     "set_sleep_time", "set_wakeup_time", "set_manual_mode", "sleep_test",
                     "sleep_test_cancel", "sleep_status"]),
    ("devices",     ["get_address_list", "remove_device", "remove_all_devices",
                     "submit_positions", "reset_clients", "get_system_info"]),
    ("firmware",    ["ota_update", "set_ota_url", "reset_system", "update_version"]),
    ("system",      ["set_time", "toggle_test_mode", "toggle_logging", "get_midi_params",
                     "set_midi_params"]),
]

BLURBS = {
    "sync_fast":        "resync every client in parallel batches",
    "sync_all":         "resync every client sequentially (slow)",
    "sync":             "resync one client by index",
    "reannounce":       "ask every awake client to re-register",
    "animate_toggle":   "start/stop the ambient animation loop",
    "animation_off":    "everything dark, ambient stays off",
    "hearth":           "candlelit windows, burns and gutters forever",
    "chirp_position":   "one 10-chirp burst from where the emitter stands",
    "dist_cal_start":   "distance-from-centre calibration (one burst)",
    "sleep_now":        "hold the whole fleet asleep until wake_now",
    "wake_now":         "wake everyone, verified until all report back",
    "sleep_until":      "sleep now, wake at HH:MM",
    "sleep_status":     "what the sleep machinery is currently doing",
    "reset_system":     "wipe the address list and restart the master",
    "reset_clients":    "restart every client, master untouched",
    "remove_all_devices": "empty the address list",
    "get_address_list": "dump every known board",
    "ota_update":       "push firmware to every client over the air",
}


def discover_commands(path=FIRMWARE):
    """Read the command table out of the firmware's serial handler.

    Every branch looks like strcmp(cmd, "name") == 0, and the arguments it reads
    are doc["key"] lookups inside that branch — so both come straight from source
    rather than a list here that would quietly go stale.
    """
    try:
        with open(path, encoding="utf-8", errors="ignore") as f:
            src = f.read()
    except OSError:
        return {}
    hits = [(m.start(), m.group(1)) for m in re.finditer(r'strcmp\(cmd,\s*"([^"]+)"\)', src)]
    commands = {}
    for i, (pos, name) in enumerate(hits):
        end = hits[i + 1][0] if i + 1 < len(hits) else len(src)
        body = src[pos:end]
        params = []
        for pm in re.finditer(r'doc\["([^"]+)"\]', body):
            key = pm.group(1)
            if key != "cmd" and key not in params:
                params.append(key)
        commands[name] = params
    return commands


def coerce(value):
    """key=value arrives as text; the master wants real JSON types."""
    low = value.lower()
    if low in ("true", "false"):
        return low == "true"
    if low in ("null", "none"):
        return None
    try:
        return int(value)
    except ValueError:
        pass
    try:
        return float(value)
    except ValueError:
        pass
    return value


def find_port(preferred):
    if os.path.exists(preferred):
        return preferred
    candidates = [p.device for p in list_ports.comports() if "usbmodem" in p.device or "usbserial" in p.device]
    cu = [c for c in candidates if "/cu." in c]
    return (cu or candidates or [None])[0]


class Master:
    """Line-oriented JSON link to the master."""

    def __init__(self, port, show_all=False):
        self.show_all = show_all
        self.port = port
        self.ser = serial.Serial(port, BAUD, timeout=0.2)
        self.last_line = 0.0
        self._stop = threading.Event()
        self._thread = threading.Thread(target=self._reader, daemon=True)
        self._thread.start()

    def _reader(self):
        buf = b""
        while not self._stop.is_set():
            try:
                data = self.ser.read(4096)
            except Exception as exc:
                print(f"{C['red']}serial read failed: {exc}{C['reset']}")
                return
            if not data:
                continue
            buf += data
            while b"\n" in buf:
                raw, buf = buf.split(b"\n", 1)
                self._show(raw.decode("utf-8", errors="replace").strip())

    def _show(self, line):
        if not line:
            return
        self.last_line = time.time()
        try:
            frame = json.loads(line)
        except json.JSONDecodeError:
            print(f"{C['dim']}{line}{C['reset']}")   # ESP_LOG chatter and boot banners
            return
        event = frame.get("event", "?")
        if not self.show_all and event in HOUSEKEEPING:
            return
        colour = C["cyan"]
        if "error" in event or frame.get("status") == "failed":
            colour = C["red"]
        elif event in ("boot", "sleep_phase", "sleep_until_start", "sleep_until_done"):
            colour = C["yellow"]
        elif event.endswith("_done") or frame.get("success") is True:
            colour = C["green"]
        rest = " ".join(f"{C['dim']}{k}={C['reset']}{v}" for k, v in frame.items() if k != "event")
        print(f"{colour}{event}{C['reset']} {rest}".rstrip())

    def send(self, payload):
        line = json.dumps(payload)
        print(f"{C['magenta']}→ {line}{C['reset']}")
        self.ser.write((line + "\n").encode())
        self.ser.flush()

    def wait_quiet(self, seconds, quiet_for=1.2):
        """Listen for a while, but cut out early once the master stops talking."""
        deadline = time.time() + seconds
        self.last_line = time.time()
        while time.time() < deadline:
            if time.time() - self.last_line > quiet_for:
                return
            time.sleep(0.05)

    def close(self):
        self._stop.set()
        time.sleep(0.25)
        try:
            self.ser.close()
        except Exception:
            pass


def print_help(commands):
    print(f"\n{C['bold']}Sparkles CLI{C['reset']} — commands go straight to the master over serial\n")
    print(f"  {C['bold']}name key=value ...{C['reset']}   send a command")
    print(f"  {C['bold']}{{\"cmd\": ...}}{C['reset']}        send raw JSON")
    print(f"  {C['bold']}/help /all /quit{C['reset']}     help, toggle housekeeping events, exit\n")
    listed = set()
    for title, names in GROUPS:
        rows = [(n, commands[n]) for n in names if n in commands]
        if not rows:
            continue
        print(f"{C['bold']}{title}{C['reset']}")
        for name, params in rows:
            listed.add(name)
            blurb = BLURBS.get(name, "")
            if blurb:
                print(f"  {C['green']}{name:<22}{C['reset']} {blurb}")
                if params:
                    print(f"  {' ' * 22} {C['dim']}{' '.join(params)}{C['reset']}")
            else:
                print(f"  {C['green']}{name:<22}{C['reset']} "
                      f"{C['dim']}{' '.join(params)}{C['reset']}".rstrip())
        print()
    other = sorted(set(commands) - listed)
    if other:
        print(f"{C['bold']}other{C['reset']}")
        for name in other:
            params = commands[name]
            print(f"  {C['green']}{name:<22}{C['reset']} "
                  f"{C['dim']}{' '.join(params)}{C['reset']}")
        print()
    print(f"{C['dim']}{len(commands)} commands, read from the firmware source{C['reset']}\n")


def build_payload(parts, commands):
    """'chirp_position x=1 y=2' -> {'cmd': 'chirp_position', 'x': 1, 'y': 2}"""
    name = parts[0]
    payload = {"cmd": name}
    unknown = []
    for token in parts[1:]:
        if "=" not in token:
            unknown.append(token)
            continue
        key, value = token.split("=", 1)
        payload[key] = coerce(value)
    if unknown:
        print(f"{C['yellow']}ignoring {' '.join(unknown)} — arguments are key=value{C['reset']}")
    known = commands.get(name)
    if known is not None:
        for key in payload:
            if key != "cmd" and key not in known:
                print(f"{C['yellow']}note: {name} does not read '{key}'"
                      f"{' (takes ' + ', '.join(known) + ')' if known else ' (takes no arguments)'}"
                      f"{C['reset']}")
    return payload


def repl(link, commands):
    print(f"{C['dim']}connected to {link.port} — /help for commands, /quit to exit{C['reset']}")
    while True:
        try:
            raw = input(f"{C['bold']}sparkles>{C['reset']} ").strip()
        except (EOFError, KeyboardInterrupt):
            print()
            return
        if not raw:
            continue
        if raw in ("/quit", "/exit", "/q"):
            return
        if raw == "/help":
            print_help(commands)
            continue
        if raw == "/all":
            link.show_all = not link.show_all
            print(f"{C['dim']}housekeeping events {'shown' if link.show_all else 'hidden'}{C['reset']}")
            continue
        if raw.startswith("{"):
            try:
                link.send(json.loads(raw))
            except json.JSONDecodeError as exc:
                print(f"{C['red']}not valid JSON: {exc}{C['reset']}")
            continue
        parts = raw.split()
        if parts[0] not in commands:
            print(f"{C['yellow']}unknown command '{parts[0]}' — sending anyway, /help to list{C['reset']}")
        link.send(build_payload(parts, commands))


def main():
    ap = argparse.ArgumentParser(add_help=False, description="Send commands straight to the Sparkles master.")
    ap.add_argument("command", nargs="*", help="command name and key=value arguments")
    ap.add_argument("-p", "--port", default=DEFAULT_PORT, help=f"serial port (default {DEFAULT_PORT})")
    ap.add_argument("-w", "--wait", type=float, default=6.0, help="seconds to listen after a one-shot command")
    ap.add_argument("-a", "--all", action="store_true", help="show housekeeping events too")
    ap.add_argument("-l", "--list", action="store_true", help="print the command table and exit")
    ap.add_argument("-h", "--help", action="store_true", help="show this help")
    args = ap.parse_args()

    commands = discover_commands()
    if not commands:
        print(f"{C['yellow']}could not read {FIRMWARE} — raw JSON still works{C['reset']}")

    if args.help or (args.command and args.command[0] in ("/help", "help")):
        ap.print_help()
        print_help(commands)
        return 0
    if args.list:
        print_help(commands)
        return 0

    port = find_port(args.port)
    if not port:
        return f"no serial port found (looked for {args.port})"
    if port != args.port:
        print(f"{C['yellow']}{args.port} not present, using {port}{C['reset']}")

    try:
        link = Master(port, show_all=args.all)
    except serial.SerialException as exc:
        return (f"could not open {port}: {exc}\n"
                "if the pi's serial bridge or a monitor has the port, stop that first")

    try:
        if args.command:
            link.send(build_payload(args.command, commands))
            link.wait_quiet(args.wait)
        else:
            repl(link, commands)
    finally:
        link.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
