"""
serial_mux.py — serial multiplexer daemon (dual device)

Owns the ESP32 serial ports and exposes a Unix socket at /tmp/sparkles.sock.
Two devices share the air: a "master" (sync, calibration, management) and a
"music" device (the 30 Hz aubio/MIDI broadcast). The mux figures out which
physical port is which by asking each one: it sends {"cmd":"identify"} and the
device answers {"event":"identity","role":"master"|"music"}. Role is declared
by firmware, so boards and USB ports can be swapped freely.

Clients (serial_bridge, aubioAlgo, keyboard script) connect to the socket and
are unchanged: they send JSON lines, the mux routes each to the right device by
its "cmd", and every line read from either device is broadcast back to all
clients.
"""

import glob
import json
import logging
import os
import queue
import socket
import threading
import time

import serial

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s %(levelname)s %(name)s %(message)s",
)
log = logging.getLogger("serial_mux")

SOCKET_PATH  = os.environ.get("SPARKLES_SOCK", "/tmp/sparkles.sock")
SERIAL_BAUD  = int(os.environ.get("SPARKLES_BAUD", "115200"))
PORT_GLOB    = os.environ.get("SPARKLES_PORT_GLOB", "/dev/ttyACM*")
LOG_PATH     = os.environ.get("SPARKLES_SERIAL_LOG", "/home/julian/sparkles/logs/serial.log")
RAW_LOG_PATH = os.environ.get("SPARKLES_RAW_LOG", "/home/julian/sparkles/logs/serial_raw.log")
DISCOVER_INTERVAL = 3.0
IDENTIFY_TIMEOUT  = 3.0

# commands that belong to the music device, everything else goes to the master
MUSIC_CMDS = {"aubio_shimmer", "aubio_midi", "keyboard_midi"}

# explicit per-role overrides skip identification when set
ROLE_ENV = {
    "master": os.environ.get("SPARKLES_MASTER_PORT"),
    "music":  os.environ.get("SPARKLES_MUSIC_PORT"),
}

_clients: list = []
_clients_lock = threading.Lock()
_last_music = 0.0


# ---------------------------------------------------------------------------
# Serial log files
# ---------------------------------------------------------------------------
_log_file = None
_raw_log_file = None
try:
    os.makedirs(os.path.dirname(LOG_PATH), exist_ok=True)
    _log_file = open(LOG_PATH, "a", buffering=1)
    _raw_log_file = open(RAW_LOG_PATH, "a", buffering=1)
except Exception as exc:
    log.warning("Could not open serial log: %s", exc)


def _log_line(direction: str, role: str, line: str):
    if _log_file:
        _log_file.write(f"{time.strftime('%H:%M:%S')} {direction} [{role}] {line}\n")


def _log_raw(role: str, line: str):
    if _raw_log_file:
        ts = time.strftime('%H:%M:%S.') + f"{int(time.time() * 1000) % 1000:03d}"
        _raw_log_file.write(f"{ts} [{role}] {line}\n")


# ---------------------------------------------------------------------------
# Client broadcast
# ---------------------------------------------------------------------------

def _broadcast(line: str):
    encoded = (line + "\n").encode()
    with _clients_lock:
        dead = []
        for conn, lock in _clients:
            try:
                with lock:
                    conn.sendall(encoded)
            except Exception:
                dead.append((conn, lock))
        for d in dead:
            _clients.remove(d)


def _add_client(conn: socket.socket):
    conn.settimeout(0.5)
    with _clients_lock:
        _clients.append((conn, threading.Lock()))


def _remove_client(conn: socket.socket):
    with _clients_lock:
        _clients[:] = [(s, l) for s, l in _clients if s is not conn]


# ---------------------------------------------------------------------------
# Device port — one per role, owns its serial handle and write queue
# ---------------------------------------------------------------------------

class Port:
    def __init__(self, role: str):
        self.role = role
        self.path = None
        self.ser = None
        self.connected = False
        self.write_queue: queue.Queue = queue.Queue(maxsize=256)

    def enqueue(self, line: str):
        try:
            self.write_queue.put_nowait(line)
        except queue.Full:
            log.warning("%s write queue full, dropping: %s", self.role, line.strip())


ports = {"master": Port("master"), "music": Port("music")}
_assigned_paths: set = set()
_assigned_lock = threading.Lock()
_last_drop_warn = {"master": 0.0, "music": 0.0}


def route(line_no_nl: str):
    """Send a client line to the correct device by its cmd."""
    role = "master"
    try:
        obj = json.loads(line_no_nl)
        if obj.get("cmd") in MUSIC_CMDS:
            role = "music"
    except Exception:
        pass  # non-JSON goes to the master

    p = ports[role]
    if not p.connected:
        now = time.time()
        if now - _last_drop_warn[role] > 2.0:
            log.warning("%s device not connected, dropping %s", role, line_no_nl[:60])
            _last_drop_warn[role] = now
        return
    p.enqueue(line_no_nl + "\n")
    _log_line("TX", role, line_no_nl)
    if role == "music":
        global _last_music
        _last_music = time.time()


# ---------------------------------------------------------------------------
# Port read/write threads
# ---------------------------------------------------------------------------

def _write_thread(p: Port, stop_event: threading.Event):
    while not stop_event.is_set():
        try:
            line_out = p.write_queue.get(timeout=0.05)
            p.ser.write(line_out.encode())
        except queue.Empty:
            pass
        except Exception:
            break


def _run_port(p: Port, ser: serial.Serial, path: str):
    """Own an identified port: spawn its writer, read+broadcast until it dies."""
    p.ser = ser
    p.path = path
    p.connected = True
    log.info("%s device connected on %s", p.role, path)
    _broadcast(json.dumps({"event": "device_status", "role": p.role, "connected": True}))

    stop_event = threading.Event()
    writer = threading.Thread(target=_write_thread, args=(p, stop_event), daemon=True)
    writer.start()
    try:
        while True:
            raw = ser.readline()
            if not raw:
                continue
            line = raw.decode("utf-8", errors="replace").strip()
            if not line:
                continue
            _log_line("RX", p.role, line)
            _log_raw(p.role, line)
            _broadcast(line)
    except Exception:
        pass
    finally:
        stop_event.set()
        p.connected = False
        p.ser = None
        p.path = None
        try:
            ser.close()
        except Exception:
            pass
        with _assigned_lock:
            _assigned_paths.discard(path)
        log.warning("%s device disconnected (%s)", p.role, path)
        _broadcast(json.dumps({"event": "device_status", "role": p.role, "connected": False}))


# ---------------------------------------------------------------------------
# Identification — ask a port who it is
# ---------------------------------------------------------------------------

def _identify(path: str):
    """Open a port, send identify, return (role, open_serial) or (None, None)."""
    try:
        ser = serial.Serial(path, SERIAL_BAUD, timeout=0.5)
    except Exception:
        return None, None
    try:
        time.sleep(0.3)            # let boot noise settle
        ser.reset_input_buffer()
        ser.write(b'{"cmd":"identify"}\n')
        deadline = time.monotonic() + IDENTIFY_TIMEOUT
        while time.monotonic() < deadline:
            raw = ser.readline()
            if not raw:
                continue
            try:
                obj = json.loads(raw.decode("utf-8", errors="replace").strip())
            except Exception:
                continue
            if obj.get("event") == "identity" and obj.get("role") in ports:
                return obj["role"], ser
    except Exception:
        pass
    try:
        ser.close()
    except Exception:
        pass
    return None, None


def _discover_loop():
    """Periodically probe ports, identify unassigned ones, run each role."""
    while True:
        for role, forced in ROLE_ENV.items():
            if forced and not ports[role].connected and os.path.exists(forced):
                with _assigned_lock:
                    if forced in _assigned_paths:
                        continue
                    _assigned_paths.add(forced)
                try:
                    ser = serial.Serial(forced, SERIAL_BAUD, timeout=1)
                except Exception:
                    with _assigned_lock:
                        _assigned_paths.discard(forced)
                    continue
                threading.Thread(target=_run_port, args=(ports[role], ser, forced), daemon=True).start()

        if not all(ROLE_ENV.values()):
            for path in sorted(glob.glob(PORT_GLOB)):
                with _assigned_lock:
                    if path in _assigned_paths:
                        continue
                if all(p.connected for p in ports.values()):
                    break
                role, ser = _identify(path)
                if role and not ports[role].connected:
                    with _assigned_lock:
                        _assigned_paths.add(path)
                    ser.timeout = 1
                    threading.Thread(target=_run_port, args=(ports[role], ser, path), daemon=True).start()
                elif ser is not None:
                    try:
                        ser.close()
                    except Exception:
                        pass
        time.sleep(DISCOVER_INTERVAL)


def _music_heartbeat():
    """While music is flowing, ping the master ~1 Hz so it suppresses its idle
    animation loop. Far cheaper than routing the 30 Hz stream through it."""
    while True:
        time.sleep(1.0)
        if time.time() - _last_music < 3.0 and ports["master"].connected:
            ports["master"].enqueue('{"cmd":"music_active"}\n')


# ---------------------------------------------------------------------------
# Per-client handler
# ---------------------------------------------------------------------------

def _handle_client(conn: socket.socket, addr: str):
    log.info("Client connected: %s", addr)
    _add_client(conn)
    buf = b""
    try:
        while True:
            try:
                data = conn.recv(4096)
            except socket.timeout:
                continue
            if not data:
                break
            buf += data
            while b"\n" in buf:
                raw, buf = buf.split(b"\n", 1)
                line = raw.strip().decode("utf-8", errors="ignore")
                if line:
                    route(line)
    except Exception as exc:
        log.debug("Client error: %s", exc)
    finally:
        _remove_client(conn)
        conn.close()
        log.info("Client disconnected: %s", addr)


def _socket_server():
    if os.path.exists(SOCKET_PATH):
        os.unlink(SOCKET_PATH)

    server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    server.bind(SOCKET_PATH)
    os.chmod(SOCKET_PATH, 0o660)
    server.listen(16)
    log.info("Listening on %s", SOCKET_PATH)

    while True:
        conn, _ = server.accept()
        threading.Thread(target=_handle_client, args=(conn, conn.fileno()), daemon=True).start()


if __name__ == "__main__":
    threading.Thread(target=_discover_loop, daemon=True, name="discover").start()
    threading.Thread(target=_music_heartbeat, daemon=True, name="music-heartbeat").start()
    _socket_server()  # blocks
