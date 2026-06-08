"""
serial_bridge.py – connects to serial_mux.py via Unix socket.

Runs a background reader thread that parses newline-delimited JSON frames
from the mux and fans them out to registered async queues.
Pi → ESP32: call send(dict) to queue a command frame.

Optionally forwards selected events to a T-Beam running Meshtastic via
its SerialModule (SPARKLES_TBEAM_PORT env var, e.g. /dev/ttyUSB0).
If the port is absent or fails to open, forwarding is silently skipped.
"""

import asyncio
import collections
import json
import logging
import os
import queue
import socket
import threading
from collections import defaultdict

import serial

logger = logging.getLogger("serial_bridge")

_SOCKET_PATH = os.environ.get("SPARKLES_SOCK", "/tmp/sparkles.sock")
_DEFAULT_PORT = "/dev/ttyACM0"
_BAUD = 115200


class _MuxSocket:
    """Wraps a Unix socket connection to serial_mux with a serial-like interface."""

    def __init__(self, path: str):
        self._path = path
        self._sock: socket.socket | None = None
        self._buf = b""
        self.is_open = False

    def open(self):
        self._sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self._sock.settimeout(1.0)
        self._sock.connect(self._path)
        self._buf = b""
        self.is_open = True

    def close(self):
        self.is_open = False
        if self._sock:
            try:
                self._sock.close()
            except Exception:
                pass
            self._sock = None

    def write(self, data: bytes):
        if self._sock:
            self._sock.sendall(data)

    def readline(self) -> bytes:
        """Return one newline-terminated line, blocking up to timeout."""
        while b"\n" not in self._buf:
            try:
                chunk = self._sock.recv(4096)
            except socket.timeout:
                return b""
            if not chunk:
                raise serial.SerialException("Mux socket closed")
            self._buf += chunk
        line, self._buf = self._buf.split(b"\n", 1)
        return line + b"\n"

    def reset_input_buffer(self):
        self._buf = b""
_TBEAM_PORT = os.environ.get("SPARKLES_TBEAM_PORT", "")
_TBEAM_BAUD = int(os.environ.get("SPARKLES_TBEAM_BAUD", "38400"))

_BATTERY_CRITICAL = int(os.environ.get("SPARKLES_BATTERY_CRITICAL", "15"))
_HEALTH_INTERVAL  = int(os.environ.get("SPARKLES_HEALTH_INTERVAL", "300"))  # seconds
_LOG_BUFFER_SIZE  = 2000
_SERIAL_LOG_PATH  = os.environ.get("SPARKLES_SERIAL_LOG", "/home/julian/sparkles/serial.log")


class HealthMonitor:
    """Tracks device state and decides what to forward to the T-Beam."""

    def __init__(self, send_fn):
        self._send = send_fn          # callable(dict) — writes to T-Beam
        self._lock = threading.Lock()
        self._clients: dict[int, dict] = {}   # id → {bat, status}
        self._animating = False
        self._num_devices = 0
        self._start_time = __import__("time").monotonic()
        self._last_health = 0.0
        self._alerted_bat: set[int] = set()   # ids already alerted for low battery

    def ingest(self, frame: dict):
        import time
        event = frame.get("event", "")
        now = time.monotonic()

        with self._lock:
            if event == "update_board":
                cid = frame.get("id")
                if cid is None:
                    return
                prev = self._clients.get(cid, {})
                self._clients[cid] = {
                    "bat":    frame.get("batteryPercentage", prev.get("bat", 100)),
                    "status": frame.get("status", prev.get("status", "inactive")),
                }
                # alert: client lost
                if prev.get("status") == "active" and frame.get("status") == "inactive":
                    self._send({"t": "alert", "event": "client_lost",
                                "id": cid, "bat": self._clients[cid]["bat"]})
                # alert: client recovered
                if prev.get("status") == "inactive" and frame.get("status") == "active":
                    self._send({"t": "alert", "event": "client_back", "id": cid})
                # alert: battery critical (once per session)
                bat = self._clients[cid]["bat"]
                if bat <= _BATTERY_CRITICAL and cid not in self._alerted_bat:
                    self._alerted_bat.add(cid)
                    self._send({"t": "alert", "event": "bat_critical",
                                "id": cid, "bat": bat})

            elif event == "animate_status":
                self._animating = frame.get("status") is True or frame.get("status") == "true"

            elif event == "num_devices":
                prev_n = self._num_devices
                self._num_devices = frame.get("numDevices", 0)
                # alert: master rebooted (devices dropped to 0)
                if prev_n > 0 and self._num_devices == 0:
                    self._send({"t": "alert", "event": "master_reboot"})

            # periodic health summary
            if now - self._last_health >= _HEALTH_INTERVAL:
                self._last_health = now
                self._send_health(now)

    def _send_health(self, now: float):
        active = [c for c in self._clients.values() if c["status"] == "active"]
        bats = [c["bat"] for c in active] or [0]
        self._send({
            "t":        "health",
            "active":   len(active),
            "total":    len(self._clients),
            "minBat":   min(bats),
            "avgBat":   round(sum(bats) / len(bats)),
            "animating": self._animating,
            "uptime":   int(now - self._start_time),
        })


class SerialBridge:
    def __init__(self, port: str = _DEFAULT_PORT, baud: int = _BAUD):
        self._port = port
        self._baud = baud
        self._serial: serial.Serial | None = None
        self._log_file = open(_SERIAL_LOG_PATH, "a", buffering=1) if _SERIAL_LOG_PATH else None
        self._thread: threading.Thread | None = None
        self._running = False
        # asyncio queues subscribed to all incoming events
        self._subscribers: list[asyncio.Queue] = []
        self._subscribers_lock = threading.Lock()
        # event listeners keyed by event name (for request/response pairing)
        self._event_listeners: dict[str, list[asyncio.Future]] = defaultdict(list)
        self._loop: asyncio.AbstractEventLoop | None = None
        # optional T-Beam forwarder
        self._tbeam: serial.Serial | None = self._open_tbeam()
        self._health = HealthMonitor(self._forward_to_tbeam) if self._tbeam else None
        # serial log buffer + subscribers
        self._log_buffer: collections.deque[str] = collections.deque(maxlen=_LOG_BUFFER_SIZE)
        self._log_subscribers: list[asyncio.Queue] = []
        # watchdog: timestamps for stale detection
        self._last_frame_time: float = 0.0
        self._connected_since: float = 0.0  # when port was last opened successfully
        # set to True to keep reader thread from reconnecting (e.g. during firmware flash)
        self._pause_reconnect: bool = False
        # outbound write queue — serial writes happen on the reader thread, never the event loop
        self._send_queue: queue.Queue = queue.Queue(maxsize=64)

    # ------------------------------------------------------------------
    # T-Beam forwarder
    # ------------------------------------------------------------------

    @staticmethod
    def _open_tbeam() -> "serial.Serial | None":
        if not _TBEAM_PORT:
            return None
        try:
            s = serial.Serial(_TBEAM_PORT, _TBEAM_BAUD, timeout=1)
            logger.info("T-Beam forwarder opened on %s @ %d", _TBEAM_PORT, _TBEAM_BAUD)
            return s
        except Exception as exc:
            logger.warning("T-Beam not available on %s: %s", _TBEAM_PORT, exc)
            return None

    def _forward_to_tbeam(self, frame: dict):
        if self._tbeam is None or not self._tbeam.is_open:
            return
        try:
            self._tbeam.write((json.dumps(frame) + "\n").encode())
        except Exception as exc:
            logger.warning("T-Beam write failed: %s", exc)
            self._tbeam = None  # stop trying until restart

    # ------------------------------------------------------------------
    # Lifecycle
    # ------------------------------------------------------------------

    def start(self, loop: asyncio.AbstractEventLoop):
        self._loop = loop
        self._running = True
        self._thread = threading.Thread(target=self._reader, daemon=True, name="serial-reader")
        self._thread.start()
        logger.info("Serial bridge started, will connect to %s @ %d", self._port, self._baud)

    def stop(self):
        self._running = False
        if self._serial and self._serial.is_open:
            self._serial.close()
        if self._tbeam and self._tbeam.is_open:
            self._tbeam.close()
        logger.info("Serial bridge stopped")

    def release_port(self):
        """Close the serial port and pause reconnection so esptool can claim it."""
        self._pause_reconnect = True
        if self._serial and self._serial.is_open:
            self._serial.close()
        logger.info("Serial port released for external use")

    def resume_port(self):
        """Allow the reader thread to reconnect after external tool is done."""
        self._pause_reconnect = False
        logger.info("Serial port reconnect resumed")

    def _emit_serial_status(self, connected: bool):
        self._dispatch({"event": "serial_status", "connected": connected})

    # ------------------------------------------------------------------
    # Sending
    # ------------------------------------------------------------------

    def _send_ota_url(self):
        import socket as _socket
        try:
            s = _socket.socket(_socket.AF_INET, _socket.SOCK_DGRAM)
            s.connect(("8.8.8.8", 80))
            ip = s.getsockname()[0]
            s.close()
            url = f"http://{ip}/firmware.bin"
            self.send({"cmd": "set_ota_url", "url": url})
            logger.info("Sent OTA URL to master: %s", url)
        except Exception as exc:
            logger.warning("Could not detect Pi IP for OTA URL: %s", exc)

    @property
    def is_connected(self) -> bool:
        return self._serial is not None and self._serial.is_open

    def send(self, payload: dict):
        """Enqueue a command for the reader thread to write — never blocks the event loop."""
        try:
            line = json.dumps(payload) + "\n"
            self._send_queue.put_nowait(line)
            logger.debug("TX → %s", line.strip())
            if self._log_file:
                import time as _time
                self._log_file.write(f"{_time.strftime('%H:%M:%S')} TX {line.strip()}\n")
        except queue.Full:
            logger.warning("Send queue full, dropping: %s", payload)

    # ------------------------------------------------------------------
    # Subscriptions (for SSE fan-out)
    # ------------------------------------------------------------------

    def subscribe(self) -> asyncio.Queue:
        q: asyncio.Queue = asyncio.Queue(maxsize=256)
        with self._subscribers_lock:
            self._subscribers.append(q)
        return q

    def unsubscribe(self, q: asyncio.Queue):
        with self._subscribers_lock:
            self._subscribers.remove(q)

    # ------------------------------------------------------------------
    # Serial log buffer
    # ------------------------------------------------------------------

    def subscribe_log(self) -> tuple[list[str], asyncio.Queue]:
        """Return buffered lines so far + a live queue for new lines."""
        q: asyncio.Queue = asyncio.Queue(maxsize=1000)
        with self._subscribers_lock:
            snapshot = list(self._log_buffer)
            self._log_subscribers.append(q)
        return snapshot, q

    def unsubscribe_log(self, q: asyncio.Queue):
        with self._subscribers_lock:
            try:
                self._log_subscribers.remove(q)
            except ValueError:
                pass

    def _append_log(self, line: str):
        with self._subscribers_lock:
            self._log_buffer.append(line)
            subs = list(self._log_subscribers)
        if self._log_file:
            import time as _time
            self._log_file.write(f"{_time.strftime('%H:%M:%S')} RX {line}\n")
        for q in subs:
            try:
                self._loop.call_soon_threadsafe(q.put_nowait, line)
            except (asyncio.QueueFull, AttributeError):
                pass

    # ------------------------------------------------------------------
    # Request/response helpers (wait for a specific event type)
    # ------------------------------------------------------------------

    async def request(self, cmd: dict, response_event: str, timeout: float = 5.0) -> dict | None:
        fut: asyncio.Future = self._loop.create_future()
        self._event_listeners[response_event].append(fut)
        self.send(cmd)
        try:
            return await asyncio.wait_for(fut, timeout)
        except asyncio.TimeoutError:
            logger.warning("Timeout waiting for event '%s'", response_event)
            return None
        finally:
            try:
                self._event_listeners[response_event].remove(fut)
            except ValueError:
                pass

    # ------------------------------------------------------------------
    # Background reader
    # ------------------------------------------------------------------

    def _reader(self):
        import time as _time

        RECONNECT_DELAY = 3.0

        while self._running:
            # --- wait if paused for external tool (e.g. firmware flash) ---
            while self._running and self._pause_reconnect:
                _time.sleep(0.5)

            if not self._running:
                break

            # --- connect to mux socket ---
            try:
                mux = _MuxSocket(_SOCKET_PATH)
                mux.open()
                self._serial = mux
                logger.info("Connected to serial mux at %s", _SOCKET_PATH)
            except Exception as exc:
                logger.warning("Mux connect failed (%s), retrying in %.0fs", exc, RECONNECT_DELAY)
                self._emit_serial_status(False)
                _time.sleep(RECONNECT_DELAY)
                continue

            # Drain boot noise for 2s
            deadline = _time.monotonic() + 2.0
            while self._running and _time.monotonic() < deadline:
                try:
                    self._serial.readline()
                except Exception:
                    break
            if not self._running:
                break
            self._serial.reset_input_buffer()
            self._connected_since = _time.monotonic()
            self._last_frame_time = 0.0  # reset so stale clock starts from connect
            self._emit_serial_status(True)
            self._send_ota_url()

            # --- read loop ---
            while self._running:
                # drain outbound queue before blocking on readline
                while True:
                    try:
                        line_out = self._send_queue.get_nowait()
                        self._serial.write(line_out.encode())
                    except queue.Empty:
                        break
                try:
                    raw = self._serial.readline()
                    if not raw:
                        continue
                    line = raw.decode(errors="replace").strip()
                    if not line:
                        continue
                    logger.debug("RX ← %s", line)
                    self._append_log(line)
                    try:
                        frame = json.loads(line)
                    except json.JSONDecodeError:
                        continue
                    import time as _time
                    self._last_frame_time = _time.monotonic()
                    self._dispatch(frame)
                except serial.SerialException as exc:
                    logger.error("Serial disconnected: %s — reconnecting in %.0fs", exc, RECONNECT_DELAY)
                    self._emit_serial_status(False)
                    try:
                        self._serial.close()
                    except Exception:
                        pass
                    _time.sleep(RECONNECT_DELAY)
                    break
                except Exception as exc:
                    logger.exception("Unexpected reader error: %s", exc)

    def _dispatch(self, frame: dict):
        if self._loop is None:
            return
        event_name = frame.get("event", "")

        # resolve request/response futures
        listeners = self._event_listeners.get(event_name, [])
        for fut in list(listeners):
            if not fut.done():
                self._loop.call_soon_threadsafe(fut.set_result, frame)

        # health monitor decides what to forward to T-Beam
        if self._health:
            self._health.ingest(frame)

        # fan out to SSE subscribers
        with self._subscribers_lock:
            subs = list(self._subscribers)
        for q in subs:
            try:
                self._loop.call_soon_threadsafe(q.put_nowait, frame)
            except asyncio.QueueFull:
                logger.warning("SSE queue full, dropping frame")


# module-level singleton
bridge = SerialBridge()
