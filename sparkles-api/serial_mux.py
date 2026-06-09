"""
serial_mux.py — serial multiplexer daemon

Owns /dev/ttyACM0 and exposes a Unix socket at /tmp/sparkles.sock.
Multiple clients (serial_bridge, aubioAlgo, keyboard script) connect to
the socket and share the serial port with no contention:
  - Client → mux: JSON lines are queued and written to serial in order
  - Serial → clients: every incoming line is broadcast to all clients
"""

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
SERIAL_PORT  = os.environ.get("SPARKLES_PORT", "/dev/ttyACM0")
SERIAL_BAUD  = int(os.environ.get("SPARKLES_BAUD", "115200"))
LOG_PATH     = os.environ.get("SPARKLES_SERIAL_LOG", "/home/julian/sparkles/logs/serial.log")
RECONNECT_DELAY = 3.0

_write_queue: queue.Queue = queue.Queue(maxsize=256)
_clients: list[tuple[socket.socket, threading.Lock]] = []
_clients_lock = threading.Lock()

# ---------------------------------------------------------------------------
# Serial log file
# ---------------------------------------------------------------------------
_log_file = None
try:
    os.makedirs(os.path.dirname(LOG_PATH), exist_ok=True)
    _log_file = open(LOG_PATH, "a", buffering=1)
except Exception as exc:
    log.warning("Could not open serial log %s: %s", LOG_PATH, exc)

def _log_line(direction: str, line: str):
    """Write a TX/RX line to the log file."""
    if _log_file:
        import time as _t
        _log_file.write(f"{_t.strftime('%H:%M:%S')} {direction} {line}\n")


# ---------------------------------------------------------------------------
# Client broadcast helpers
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
    with _clients_lock:
        _clients.append((conn, threading.Lock()))


def _remove_client(conn: socket.socket):
    with _clients_lock:
        _clients[:] = [(s, l) for s, l in _clients if s is not conn]


# ---------------------------------------------------------------------------
# Per-client handler — reads lines from socket, enqueues for serial write
# ---------------------------------------------------------------------------

def _handle_client(conn: socket.socket, addr: str):
    log.info("Client connected: %s", addr)
    _add_client(conn)
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
                    _write_queue.put_nowait(line + "\n")
                    log.debug("MUX TX ← client: %s", line)
                    _log_line("TX", line)
                except queue.Full:
                    log.warning("Write queue full, dropping: %s", line)
    except Exception as exc:
        log.debug("Client error: %s", exc)
    finally:
        _remove_client(conn)
        conn.close()
        log.info("Client disconnected: %s", addr)


# ---------------------------------------------------------------------------
# Serial worker — owns the port, reads and writes
# ---------------------------------------------------------------------------

def _serial_worker():
    _was_connected = False
    _waiting_logged = False
    while True:
        ser = None
        try:
            ser = serial.Serial(SERIAL_PORT, SERIAL_BAUD, timeout=1)
            _was_connected = True
            _waiting_logged = False
            log.info("Master ESP32 connected on %s", SERIAL_PORT)
            _broadcast(json.dumps({"event": "serial_status", "connected": True}))

            # drain boot noise
            deadline = time.monotonic() + 2.0
            while time.monotonic() < deadline:
                ser.readline()
            ser.reset_input_buffer()

            while True:
                # drain write queue first
                while True:
                    try:
                        line_out = _write_queue.get_nowait()
                        ser.write(line_out.encode())
                        log.debug("MUX TX → serial: %s", line_out.strip())
                    except queue.Empty:
                        break

                # read one line from serial
                raw = ser.readline()
                if not raw:
                    continue
                line = raw.decode("utf-8", errors="replace").strip()
                if not line:
                    continue
                log.debug("MUX RX ← serial: %s", line)
                _log_line("RX", line)
                _broadcast(line)

        except serial.SerialException as exc:
            if _was_connected:
                log.warning("Master ESP32 disconnected — waiting for reconnect")
                _broadcast(json.dumps({"event": "serial_status", "connected": False}))
                _was_connected = False
            elif not _waiting_logged:
                log.info("Waiting for master ESP32 on %s ...", SERIAL_PORT)
                _waiting_logged = True
        except Exception as exc:
            log.exception("Serial worker error: %s", exc)
        finally:
            if ser and ser.is_open:
                ser.close()
        time.sleep(RECONNECT_DELAY)


# ---------------------------------------------------------------------------
# Unix socket server
# ---------------------------------------------------------------------------

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
        threading.Thread(
            target=_handle_client,
            args=(conn, conn.fileno()),
            daemon=True,
        ).start()


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    threading.Thread(target=_serial_worker, daemon=True, name="serial-worker").start()
    _socket_server()  # blocks
