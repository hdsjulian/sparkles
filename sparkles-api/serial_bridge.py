"""
serial_bridge.py – direct serial connection to the master ESP32.

Runs a background reader thread that parses newline-delimited JSON frames
and fans them out to registered async queues.
Pi → ESP32: call send(dict) to queue a command frame.

The master port is SPARKLES_MASTER_PORT if set, otherwise the single device
matching SPARKLES_PORT_GLOB (the Espressif by-id path).

Optionally forwards selected events to a T-Beam running Meshtastic via
its SerialModule (SPARKLES_TBEAM_PORT env var, e.g. /dev/ttyUSB0).
If the port is absent or fails to open, forwarding is silently skipped.
"""

import asyncio
import collections
import glob
import json
import logging
import os
import queue
import socket
import subprocess
import threading
import time
from collections import defaultdict

import serial

logger = logging.getLogger("serial_bridge")

_MASTER_PORT = os.environ.get("SPARKLES_MASTER_PORT")
# single ESP32, so we just grab the one Espressif serial device by its stable by-id path
_PORT_GLOB = os.environ.get("SPARKLES_PORT_GLOB",
                            "/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_*")
_DEFAULT_PORT = "/dev/ttyACM0"
_BAUD = 115200
# music clients (aubioAlgo, keyboard_midi) inject their commands here; the bridge
# owns the single master serial port, so it forwards them onto the write queue
_MUSIC_SOCK = os.environ.get("SPARKLES_MUSIC_SOCK", "/tmp/music.sock")
# a music gap longer than this means piano/mic activity is resuming after idle
_IDLE_RESUME_SECONDS = int(os.environ.get("SPARKLES_IDLE_RESUME", "300"))
_TBEAM_PORT = os.environ.get("SPARKLES_TBEAM_PORT", "")
_TBEAM_BAUD = int(os.environ.get("SPARKLES_TBEAM_BAUD", "38400"))

_BATTERY_CRITICAL = int(os.environ.get("SPARKLES_BATTERY_CRITICAL", "15"))
_HEALTH_INTERVAL  = int(os.environ.get("SPARKLES_HEALTH_INTERVAL", "300"))  # seconds
_LOG_BUFFER_SIZE  = 2000
_SERIAL_LOG_PATH  = os.environ.get("SPARKLES_SERIAL_LOG", "/home/julian/sparkles/serial.log")
# persisted lamp colors, stamped into every music message (hue 0-360, saturation 0-255)
_COLORS_PATH      = os.environ.get("SPARKLES_COLORS", "/home/julian/sparkles/sparkles-api/colors.json")
_DEFAULT_COLORS   = {"midi": {"hue": 25, "saturation": 200}, "shimmer": {"hue": 31, "saturation": 255}}
# persisted sleep/wakeup schedule, re-pushed on every serial connect (master RAM loses it on reboot)
_SCHEDULE_PATH    = os.environ.get("SPARKLES_SCHEDULE", "/home/julian/sparkles/sparkles-api/schedule.json")
# persisted sleep-test event log — a long test (hours) must survive a page reload,
# a closed browser, or even a sparkles service restart; the UI replays this list
# through the same reducers it uses for live SSE, so history and live view agree
_SLEEP_TEST_EVENTS_PATH = os.environ.get("SPARKLES_SLEEP_TEST_EVENTS",
                                          "/home/julian/sparkles/sparkles-api/sleep_test_events.json")
# marker that a human set the clock via browser during this pi boot — only then is
# the pi's clock worth pushing (no internet, no rtc: a fresh boot has a stale clock)
_CLOCK_TRUST_PATH = os.environ.get("SPARKLES_CLOCK_TRUST", "/home/julian/sparkles/sparkles-api/clock_trust.json")


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
        # on-disk serial log (RX + management TX; music TX bypasses via send_line)
        try:
            os.makedirs(os.path.dirname(_SERIAL_LOG_PATH), exist_ok=True)
            self._log_file = open(_SERIAL_LOG_PATH, "a", buffering=1)
        except Exception as exc:
            logger.warning("Could not open serial log %s: %s", _SERIAL_LOG_PATH, exc)
            self._log_file = None
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
        self._last_music_time: float = 0.0  # for the installation_active resume edge
        # set to True to keep reader thread from reconnecting (e.g. during firmware flash)
        self._pause_reconnect: bool = False
        # outbound write queue — serial writes happen on the reader thread, never the event loop.
        # sized for the 30 Hz music stream plus keyboard bursts now sharing this queue
        self._send_queue: queue.Queue = queue.Queue(maxsize=256)
        # lamp colors and sleep schedule, raspi is the source of truth (persisted across reboots)
        self.colors = self._load_colors()
        self.schedule = self._load_schedule()
        self.clock_trusted = self._load_clock_trust()
        self._clock_negotiated = True  # armed (set False) on each serial connect
        self.sleep_test_events = self._load_sleep_test_events()

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
        threading.Thread(target=self._writer, daemon=True, name="serial-writer").start()
        threading.Thread(target=self._music_socket_server, daemon=True, name="music-socket").start()
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
    # Music socket — aubio/keyboard inject music commands here, we own the port
    # ------------------------------------------------------------------

    def _handle_music_client(self, conn: socket.socket):
        buf = b""
        try:
            while self._running:
                data = conn.recv(4096)
                if not data:
                    break
                buf += data
                while b"\n" in buf:
                    raw, buf = buf.split(b"\n", 1)
                    line = raw.strip().decode("utf-8", errors="ignore")
                    if line:
                        self.send_line(self._stamp_music_colors(line))
        except Exception:
            pass
        finally:
            conn.close()

    # ------------------------------------------------------------------
    # Lamp colors — persisted here so master reboots can't lose them
    # ------------------------------------------------------------------

    @staticmethod
    def _load_colors() -> dict:
        try:
            with open(_COLORS_PATH) as f:
                stored = json.load(f)
            return {k: {**_DEFAULT_COLORS[k], **stored.get(k, {})} for k in _DEFAULT_COLORS}
        except Exception:
            return json.loads(json.dumps(_DEFAULT_COLORS))

    def set_colors(self, midi: dict | None = None, shimmer: dict | None = None):
        if midi:
            self.colors["midi"].update(midi)
        if shimmer:
            self.colors["shimmer"].update(shimmer)
        try:
            with open(_COLORS_PATH, "w") as f:
                json.dump(self.colors, f, indent=2)
        except Exception as exc:
            logger.warning("Could not save colors to %s: %s", _COLORS_PATH, exc)

    @staticmethod
    def _load_schedule() -> dict:
        try:
            with open(_SCHEDULE_PATH) as f:
                return json.load(f)
        except Exception:
            return {}

    def set_schedule(self, sleep: dict | None = None, wakeup: dict | None = None):
        if sleep is not None:
            self.schedule["sleep"] = sleep
        if wakeup is not None:
            self.schedule["wakeup"] = wakeup
        try:
            with open(_SCHEDULE_PATH, "w") as f:
                json.dump(self.schedule, f, indent=2)
        except Exception as exc:
            logger.warning("Could not save schedule to %s: %s", _SCHEDULE_PATH, exc)

    @staticmethod
    def _load_sleep_test_events() -> list:
        try:
            with open(_SLEEP_TEST_EVENTS_PATH) as f:
                return json.load(f)
        except Exception:
            return []

    def _save_sleep_test_events(self):
        try:
            with open(_SLEEP_TEST_EVENTS_PATH, "w") as f:
                json.dump(self.sleep_test_events, f)
        except Exception as exc:
            logger.warning("Could not save sleep test events to %s: %s", _SLEEP_TEST_EVENTS_PATH, exc)

    def _record_sleep_test_event(self, event_name: str, frame: dict):
        # a fresh run supersedes whatever the last one left behind
        if event_name == "sleep_test_start":
            self.sleep_test_events = []
        self.sleep_test_events.append({"event": event_name, "data": frame, "ts": time.time()})
        self._save_sleep_test_events()

    @staticmethod
    def _boot_id() -> str:
        try:
            with open("/proc/sys/kernel/random/boot_id") as f:
                return f.read().strip()
        except Exception:
            return ""

    def _load_clock_trust(self) -> bool:
        try:
            with open(_CLOCK_TRUST_PATH) as f:
                saved = json.load(f)
            boot = self._boot_id()
            return bool(boot) and saved.get("boot_id") == boot
        except Exception:
            return False

    def mark_clock_trusted(self):
        self.clock_trusted = True
        self._clock_negotiated = True  # human sync outranks any pending negotiation
        try:
            with open(_CLOCK_TRUST_PATH, "w") as f:
                json.dump({"boot_id": self._boot_id(), "synced_at": time.time()}, f)
        except Exception as exc:
            logger.warning("Could not save clock trust marker: %s", exc)

    def _push_pi_clock(self):
        now = time.localtime()
        self.send({"cmd": "set_time", "year": now.tm_year, "month": now.tm_mon, "day": now.tm_mday,
                   "hours": now.tm_hour, "minutes": now.tm_min, "seconds": now.tm_sec})

    def _send_clock_and_schedule(self):
        """The master keeps clock and sleep schedule in RAM + NVS — re-push on every
        connect so a mid-night reboot rejoins the sleep phase. Clock rules:
        human-synced pi clock wins; otherwise negotiate — whoever has a clock
        donates it to the side that lost theirs (see _negotiate_clock)."""
        if self.clock_trusted:
            self._push_pi_clock()
        else:
            # ask what time the master thinks it is, negotiation continues in _dispatch
            self._clock_negotiated = False
            self.send({"cmd": "get_system_info"})
        if self.schedule.get("sleep"):
            self.send({"cmd": "set_sleep_time", **self.schedule["sleep"]})
        if self.schedule.get("wakeup"):
            self.send({"cmd": "set_wakeup_time", **self.schedule["wakeup"]})

    def _negotiate_clock(self, frame: dict):
        """Neither clock is human-synced: master survived a pi reboot -> adopt its
        clock; master lost its clock entirely -> our stale clock still beats 1970."""
        epoch = int(frame.get("epoch") or 0)
        if epoch > 1_700_000_000:
            # master epoch is wall time pretending to be utc, keep that convention
            wall = time.strftime("%Y-%m-%d %H:%M:%S", time.gmtime(epoch))
            try:
                subprocess.run(["sudo", "date", "-s", wall], check=True, timeout=5, capture_output=True)
                subprocess.run(["sudo", "fake-hwclock", "save"], timeout=5, capture_output=True)
                logger.info("Adopted master clock: %s", wall)
            except Exception as exc:
                logger.warning("Could not adopt master clock: %s", exc)
        else:
            logger.info("Master clock unset — pushing pi clock (not human-synced, better than nothing)")
            self._push_pi_clock()

    def _stamp_music_colors(self, line: str) -> str:
        """Inject the configured hue/saturation so clients always render the current color."""
        try:
            msg = json.loads(line)
            cmd = msg.get("cmd")
            if cmd == "aubio_shimmer":
                # config is the base color, aubio's pitch position (scale 0..1) rides
                # on top: higher pitch pulls slightly redder and towards white — same
                # dynamics as the original hardcoded 22->8 / 255->0 mapping
                c = self.colors["shimmer"]
                scale = float(msg.pop("scale", 0.0) or 0.0)
                base = int(c["hue"]) * 255 // 360
                msg["hue"] = max(0, base - int(14 * scale))
                msg["saturation"] = int(int(c["saturation"]) * (1.0 - scale) + 0.5)
            elif cmd in ("keyboard_midi", "aubio_midi"):
                c = self.colors["midi"]
                msg["hue"] = int(c["hue"]) * 255 // 360  # degrees -> FastLED 0-255
                msg["saturation"] = int(c["saturation"])
            else:
                return line
            return json.dumps(msg)
        except Exception:
            return line

    def _music_socket_server(self):
        if os.path.exists(_MUSIC_SOCK):
            os.unlink(_MUSIC_SOCK)
        server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        server.bind(_MUSIC_SOCK)
        os.chmod(_MUSIC_SOCK, 0o660)
        server.listen(8)
        logger.info("Music socket listening on %s", _MUSIC_SOCK)
        while self._running:
            try:
                conn, _ = server.accept()
            except Exception:
                break
            threading.Thread(target=self._handle_music_client, args=(conn,), daemon=True).start()

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
            self._append_log(line.strip(), "TX")
        except queue.Full:
            logger.warning("Send queue full, dropping: %s", payload)

    def send_line(self, line: str):
        """Enqueue a pre-serialized JSON line from a music client. Hot path — skips
        the disk/SSE log to avoid churn at 30 Hz plus keyboard bursts."""
        if not line.endswith("\n"):
            line += "\n"
        try:
            self._send_queue.put_nowait(line)
        except queue.Full:
            return  # music is a continuous stream; a dropped frame is harmless
        # resume edge: first music after a long idle → installation_active event
        # (the old serial_mux emitted this; the mesh relay in main.py listens for it)
        now = time.monotonic()
        last = self._last_music_time
        self._last_music_time = now
        if last == 0.0 or now - last > _IDLE_RESUME_SECONDS:
            idle = int(now - last) if last else None
            self._dispatch({"event": "installation_active", "idleSeconds": idle})

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

    def _append_log(self, line: str, direction: str = "RX"):
        # ts is the Pi clock so log timing survives master reboots
        entry = {"dir": direction, "line": line, "ts": time.time()}
        with self._subscribers_lock:
            self._log_buffer.append(entry)
            subs = list(self._log_subscribers)
        if self._log_file:
            self._log_file.write(f"{time.strftime('%H:%M:%S')} {direction} {line}\n")
        for q in subs:
            try:
                self._loop.call_soon_threadsafe(q.put_nowait, entry)
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

    @staticmethod
    def _get_master_port() -> str | None:
        if _MASTER_PORT:
            return _MASTER_PORT
        # single ESP32 now, so the Espressif by-id glob matches exactly one device
        matches = sorted(glob.glob(_PORT_GLOB))
        return matches[0] if matches else None

    def _writer(self):
        """Drain the send queue the moment something is enqueued. Runs on its own
        thread so TX never waits for readline() to time out — critical for the
        30 Hz music stream. One reader + one writer thread is pyserial-safe."""
        while self._running:
            try:
                line_out = self._send_queue.get(timeout=0.5)
            except queue.Empty:
                continue
            ser = self._serial
            if ser is None or not ser.is_open:
                continue  # not connected (or port released for flashing) — drop
            try:
                ser.write(line_out.encode())
            except Exception:
                pass  # reader thread owns reconnect handling

    def _reader(self):
        import time as _time

        RECONNECT_DELAY = 3.0

        while self._running:
            # --- wait if paused for external tool (e.g. firmware flash) ---
            while self._running and self._pause_reconnect:
                _time.sleep(0.5)

            if not self._running:
                break

            # --- open master serial port ---
            port = self._get_master_port()
            if not port:
                logger.warning("Master port unknown, retrying in %.0fs", RECONNECT_DELAY)
                self._emit_serial_status(False)
                _time.sleep(RECONNECT_DELAY)
                continue
            try:
                ser = serial.Serial(port, self._baud, timeout=1, write_timeout=1)
                self._serial = ser
                logger.info("Connected to master device on %s", port)
            except Exception as exc:
                logger.warning("Serial open failed (%s), retrying in %.0fs", exc, RECONNECT_DELAY)
                self._emit_serial_status(False)
                _time.sleep(RECONNECT_DELAY)
                continue

            # Drain boot noise for up to 2s. A genuine master reboot spews
            # ROM bootloader garbage here — but when only the pi's process
            # restarted (the master kept running the whole time), this window
            # sees real, valid JSON from the very first line. Discarding it
            # unconditionally silently ate the master's live traffic on every
            # reconnect, including one-shot lifecycle events (e.g. a sleep
            # test's own start/resync lines) that never come again. Dispatch
            # anything that actually parses; only true garbage gets dropped.
            deadline = _time.monotonic() + 2.0
            while self._running and _time.monotonic() < deadline:
                try:
                    raw = self._serial.readline()
                except Exception:
                    break
                if not raw:
                    continue
                line = raw.decode(errors="replace").strip()
                if not line:
                    continue
                try:
                    frame = json.loads(line)
                except json.JSONDecodeError:
                    continue  # actual boot noise
                self._append_log(line)
                self._dispatch(frame)
            if not self._running:
                break
            self._serial.reset_input_buffer()
            self._connected_since = _time.monotonic()
            self._last_frame_time = 0.0  # reset so stale clock starts from connect
            self._emit_serial_status(True)
            self._send_ota_url()
            self._send_clock_and_schedule()

            # --- read loop (TX happens on the dedicated writer thread) ---
            while self._running:
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
                except Exception as exc:
                    # pyserial can raise raw OSError/TypeError when the port is
                    # closed under a blocked readline (flash, usb re-enumeration) —
                    # treat everything as a disconnect or the thread spins forever
                    logger.error("Serial disconnected: %s — reconnecting in %.0fs", exc, RECONNECT_DELAY)
                    self._emit_serial_status(False)
                    try:
                        self._serial.close()
                    except Exception:
                        pass
                    _time.sleep(RECONNECT_DELAY)
                    break

    def _dispatch(self, frame: dict):
        if self._loop is None:
            return
        event_name = frame.get("event", "")

        # one-shot clock negotiation per connect (see _send_clock_and_schedule)
        if event_name == "system_info" and not self._clock_negotiated:
            self._clock_negotiated = True
            self._negotiate_clock(frame)

        if event_name.startswith("sleep_test_"):
            self._record_sleep_test_event(event_name, frame)

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
