"""
keyboard_midi.py — USB MIDI keyboard bridge + MIDI file player.

Responsibilities:
  1. Forward live keyboard input to serial_mux (note on/off, sustain pedal)
  2. Play MIDI files back through the keyboard on command from FastAPI
  3. Report playback status (finished / stopped by user) back to FastAPI

Communication:
  - serial_mux socket (/tmp/sparkles.sock): send note/sustain events
  - command socket (/tmp/keyboard_cmd.sock): receive play/stop from FastAPI
  - HTTP POST to FastAPI (/internal/keyboard_event): report status back
"""

import argparse
import json
import logging
import os
import socket
import sys
import threading
import time

import mido

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s %(levelname)s %(message)s",
)
log = logging.getLogger("keyboard_midi")

RETRY_DELAY = 3.0
SUSTAIN_CC  = 64

parser = argparse.ArgumentParser()
parser.add_argument("--sock",        default=os.environ.get("SPARKLES_SOCK", "/tmp/sparkles.sock"),
                    help="Unix socket path to serial_mux")
parser.add_argument("--cmd-sock",    default=os.environ.get("SPARKLES_KEYBOARD_SOCK", "/tmp/keyboard_cmd.sock"),
                    help="Unix socket path for FastAPI commands")
parser.add_argument("--api",         default="http://localhost:8080",
                    help="FastAPI base URL for status callbacks")
parser.add_argument("--songs-dir",   default=os.environ.get("SPARKLES_SONGS_DIR", "/home/julian/sparkles/songs"),
                    help="Directory containing MIDI song files")
parser.add_argument("--midi-port",   default=None,
                    help="MIDI port name (default: first available)")
parser.add_argument("--list-ports",  action="store_true",
                    help="List available MIDI ports and exit")
parser.add_argument("--nosend",      action="store_true",
                    help="Disable socket output (dry run)")
args = parser.parse_args()

if args.list_ports:
    print("Input ports:")
    for p in mido.get_input_names():
        print(f"  {p}")
    print("Output ports:")
    for p in mido.get_output_names():
        print(f"  {p}")
    sys.exit(0)

# ---------------------------------------------------------------------------
# Mux socket (serial_mux — forward live events)
# ---------------------------------------------------------------------------
_mux_sock: socket.socket | None = None
_mux_lock = threading.Lock()


def _open_mux():
    global _mux_sock
    while True:
        try:
            s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            s.connect(args.sock)
            with _mux_lock:
                _mux_sock = s
            log.info("Connected to serial mux at %s", args.sock)
            return
        except Exception as e:
            log.error("Mux connect failed: %s — retrying in %.0fs", e, RETRY_DELAY)
            time.sleep(RETRY_DELAY)


def _mux_send(cmd_dict: dict):
    if args.nosend:
        return
    with _mux_lock:
        s = _mux_sock
    if s is None:
        return
    try:
        s.sendall((json.dumps(cmd_dict) + "\n").encode())
    except Exception as e:
        log.error("Mux send failed: %s", e)


# ---------------------------------------------------------------------------
# FastAPI status callback
# ---------------------------------------------------------------------------

def _notify_fastapi(event: dict):
    """POST a keyboard event to FastAPI so it can fan out via SSE."""
    import urllib.request
    import urllib.error
    try:
        body = json.dumps(event).encode()
        req = urllib.request.Request(
            f"{args.api}/internal/keyboard_event",
            data=body,
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        urllib.request.urlopen(req, timeout=2)
    except Exception as e:
        log.warning("FastAPI notify failed: %s", e)


# ---------------------------------------------------------------------------
# Playback state
# ---------------------------------------------------------------------------
_playback_lock  = threading.Lock()
_playback_stop  = threading.Event()   # set to interrupt playback
_playback_thread: threading.Thread | None = None
_current_song: str | None = None


def _play_file(filepath: str, out_port: mido.ports.BaseOutput):
    global _current_song
    log.info("Starting playback: %s", filepath)
    _current_song = os.path.basename(filepath)
    _playback_stop.clear()

    try:
        mid = mido.MidiFile(filepath)
    except Exception as e:
        log.error("Failed to open MIDI file %s: %s", filepath, e)
        _notify_fastapi({"event": "keyboard_playback", "status": "error",
                         "song": os.path.basename(filepath), "detail": str(e)})
        return

    try:
        for msg in mid.play():
            if _playback_stop.is_set():
                # send all-notes-off before stopping
                for ch in range(16):
                    out_port.send(mido.Message("control_change", channel=ch, control=123, value=0))
                log.info("Playback stopped by user: %s", filepath)
                _notify_fastapi({"event": "keyboard_playback", "status": "stopped",
                                 "song": os.path.basename(filepath)})
                return
            if not msg.is_meta:
                out_port.send(msg)
                # also drive the cluster: forward notes to the mux, same path
                # live key presses take to the music device and the LEDs
                if msg.type == "note_on":
                    _mux_send({"cmd": "keyboard_midi", "note": msg.note, "velocity": msg.velocity})
                elif msg.type == "note_off":
                    _mux_send({"cmd": "keyboard_midi", "note": msg.note, "velocity": 0})
    except Exception as e:
        log.error("Playback error: %s", e)
        _notify_fastapi({"event": "keyboard_playback", "status": "error",
                         "song": os.path.basename(filepath), "detail": str(e)})
        return

    log.info("Playback finished: %s", filepath)
    _notify_fastapi({"event": "keyboard_playback", "status": "finished",
                     "song": os.path.basename(filepath)})


def start_playback(filepath: str, out_port: mido.ports.BaseOutput):
    global _playback_thread
    stop_playback()  # stop any current playback first
    _playback_thread = threading.Thread(
        target=_play_file, args=(filepath, out_port), daemon=True
    )
    _playback_thread.start()


def stop_playback():
    _playback_stop.set()
    t = _playback_thread
    if t and t.is_alive():
        t.join(timeout=2.0)


# ---------------------------------------------------------------------------
# MIDI input handler
# ---------------------------------------------------------------------------

def _handle_input(msg: mido.Message):
    log.debug("MIDI IN: %s", msg)

    if msg.type == "note_on":
        # user played a key — interrupt playback if active
        if _playback_thread and _playback_thread.is_alive():
            log.info("User played key during playback — stopping")
            stop_playback()
        _mux_send({"cmd": "keyboard_midi", "note": msg.note, "velocity": msg.velocity})

    elif msg.type == "note_off":
        _mux_send({"cmd": "keyboard_midi", "note": msg.note, "velocity": 0})

    elif msg.type == "control_change" and msg.control == SUSTAIN_CC:
        _mux_send({"cmd": "sustain_pedal", "value": msg.value})


# ---------------------------------------------------------------------------
# Command socket server (FastAPI sends play/stop here)
# ---------------------------------------------------------------------------

def _handle_cmd_client(conn: socket.socket, out_port: mido.ports.BaseOutput):
    buf = b""
    try:
        while True:
            data = conn.recv(4096)
            if not data:
                break
            buf += data
            while b"\n" in buf:
                raw, buf = buf.split(b"\n", 1)
                line = raw.strip().decode("utf-8", errors="ignore")
                if not line:
                    continue
                try:
                    cmd = json.loads(line)
                except json.JSONDecodeError:
                    continue

                action = cmd.get("cmd")
                log.info("Command received: %s", cmd)

                if action == "play":
                    songs_root = os.path.realpath(args.songs_dir)
                    filepath = os.path.realpath(os.path.join(songs_root, cmd.get("file", "")))
                    # realpath + prefix check keeps ../ and absolute paths inside songs_dir
                    if not filepath.startswith(songs_root + os.sep) or not os.path.isfile(filepath):
                        log.warning("Song not found: %s", filepath)
                        resp = {"event": "keyboard_playback", "status": "error",
                                "detail": f"File not found: {cmd.get('file')}"}
                        conn.sendall((json.dumps(resp) + "\n").encode())
                    else:
                        start_playback(filepath, out_port)

                elif action == "stop":
                    stop_playback()

    except Exception as e:
        log.debug("Command client error: %s", e)
    finally:
        conn.close()


def _cmd_server(out_port: mido.ports.BaseOutput):
    path = args.cmd_sock
    if os.path.exists(path):
        os.unlink(path)
    server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    server.bind(path)
    os.chmod(path, 0o660)
    server.listen(4)
    log.info("Command socket listening on %s", path)
    while True:
        conn, _ = server.accept()
        threading.Thread(target=_handle_cmd_client, args=(conn, out_port),
                         daemon=True).start()


# ---------------------------------------------------------------------------
# MIDI port helpers
# ---------------------------------------------------------------------------

def _find_port_name(names: list[str]) -> str | None:
    if args.midi_port:
        return args.midi_port if args.midi_port in names else None
    return names[0] if names else None


def _open_midi_ports():
    while True:
        in_names  = mido.get_input_names()
        out_names = mido.get_output_names()
        name = _find_port_name(in_names)
        if not name:
            log.warning("No MIDI input ports — retrying in %.0fs", RETRY_DELAY)
            time.sleep(RETRY_DELAY)
            continue
        if name not in out_names:
            log.warning("MIDI output port '%s' not available — retrying", name)
            time.sleep(RETRY_DELAY)
            continue
        try:
            return mido.open_input(name), mido.open_output(name)
        except Exception as e:
            log.error("Failed to open MIDI ports: %s — retrying", e)
            time.sleep(RETRY_DELAY)


# ---------------------------------------------------------------------------
# Main loop
# ---------------------------------------------------------------------------

if not args.nosend:
    _open_mux()

while True:
    in_port, out_port = _open_midi_ports()
    log.info("MIDI ports open: %s", in_port.name)

    threading.Thread(target=_cmd_server, args=(out_port,), daemon=True).start()

    try:
        for msg in in_port:
            _handle_input(msg)
    except Exception as e:
        log.error("MIDI input error: %s — reconnecting", e)
    finally:
        try:
            in_port.close()
            out_port.close()
        except Exception:
            pass
    time.sleep(RETRY_DELAY)
