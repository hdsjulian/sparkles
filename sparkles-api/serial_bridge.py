"""
serial_bridge.py – manages the USB serial connection to the ESP32 master.

Runs a background reader thread that parses newline-delimited JSON frames
from the device and fans them out to registered async queues.
Pi → ESP32: call send(dict) to queue a command frame.
"""

import asyncio
import json
import logging
import threading
from collections import defaultdict
from typing import Callable

import serial

logger = logging.getLogger("serial_bridge")

_DEFAULT_PORT = "/dev/ttyACM0"
_BAUD = 115200


class SerialBridge:
    def __init__(self, port: str = _DEFAULT_PORT, baud: int = _BAUD):
        self._port = port
        self._baud = baud
        self._serial: serial.Serial | None = None
        self._thread: threading.Thread | None = None
        self._running = False
        # asyncio queues subscribed to all incoming events
        self._subscribers: list[asyncio.Queue] = []
        self._subscribers_lock = threading.Lock()
        # event listeners keyed by event name (for request/response pairing)
        self._event_listeners: dict[str, list[asyncio.Future]] = defaultdict(list)
        self._loop: asyncio.AbstractEventLoop | None = None

    # ------------------------------------------------------------------
    # Lifecycle
    # ------------------------------------------------------------------

    def start(self, loop: asyncio.AbstractEventLoop):
        self._loop = loop
        import time as _time
        self._serial = serial.Serial(self._port, self._baud, timeout=1)
        _time.sleep(5)  # wait for ESP32 to finish booting after DTR reset
        self._serial.reset_input_buffer()
        self._running = True
        self._thread = threading.Thread(target=self._reader, daemon=True, name="serial-reader")
        self._thread.start()
        logger.info("Serial bridge started on %s @ %d", self._port, self._baud)

    def stop(self):
        self._running = False
        if self._serial and self._serial.is_open:
            self._serial.close()
        logger.info("Serial bridge stopped")

    # ------------------------------------------------------------------
    # Sending
    # ------------------------------------------------------------------

    def send(self, payload: dict):
        if not self._serial or not self._serial.is_open:
            logger.warning("Serial not open, dropping: %s", payload)
            return
        line = json.dumps(payload) + "\n"
        self._serial.write(line.encode())
        logger.debug("TX → %s", line.strip())

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
    # Request/response helpers (wait for a specific event type)
    # ------------------------------------------------------------------

    async def request(self, cmd: dict, response_event: str, timeout: float = 5.0) -> dict | None:
        fut: asyncio.Future = self._loop.create_future()
        self._event_listeners[response_event].append(fut)
        self.send(cmd)
        try:
            return await asyncio.wait_for(asyncio.shield(fut), timeout)
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
        buffer = ""
        while self._running:
            try:
                raw = self._serial.readline()
                if not raw:
                    continue
                line = raw.decode(errors="replace").strip()
                if not line:
                    continue
                logger.debug("RX ← %s", line)
                # accumulate partial lines (readline handles most of this,
                # but guard against incomplete frames)
                try:
                    frame = json.loads(line)
                except json.JSONDecodeError:
                    logger.debug("Non-JSON line: %s", line)
                    continue
                self._dispatch(frame)
            except serial.SerialException as exc:
                logger.error("Serial error: %s", exc)
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
