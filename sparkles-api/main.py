"""
Sparkles FastAPI server – mirrors WebServer.cpp over USB serial.

Start:  uvicorn main:app --host 0.0.0.0 --port 8080
Serial: /dev/ttyACM0  (override with SPARKLES_PORT env var)
"""

import asyncio
import json
import logging
import os
import shutil
import subprocess
from contextlib import asynccontextmanager
from typing import AsyncGenerator

from fastapi import FastAPI, Query, Request, HTTPException, UploadFile, File
from fastapi.middleware.cors import CORSMiddleware
from starlette.middleware.base import BaseHTTPMiddleware
from fastapi.responses import JSONResponse, StreamingResponse, FileResponse, RedirectResponse
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel

import auth
import compile as fw_compile
from serial_bridge import bridge

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(name)s %(levelname)s %(message)s")
logger = logging.getLogger("sparkles")

SERIAL_PORT   = os.environ.get("SPARKLES_PORT", "/dev/ttyACM0")
FIRMWARE_PATH = os.path.join(os.path.dirname(__file__), "firmware.bin")
_SETTINGS_PATH = os.environ.get("SPARKLES_SETTINGS", "/home/julian/sparkles/settings.json")

# Meshtastic log relay — the T-Beam (separate radio) rebroadcasts to the T-Deck
MESH_BIN     = os.environ.get("SPARKLES_MESH_BIN", "meshtastic")
MESH_PORT    = os.environ.get("SPARKLES_MESH_PORT", "")          # empty → CLI auto-detect
MESH_CHANNEL = int(os.environ.get("SPARKLES_MESH_CHANNEL", "1")) # sparkles secondary channel
MESH_BATTERY_LOW = float(os.environ.get("SPARKLES_MESH_BATTERY_LOW", "7"))  # relay a log below this %
MESH_HEAP_LOW = int(os.environ.get("SPARKLES_MESH_HEAP_LOW", "20000"))  # relay a log below this many bytes free

# ---------------------------------------------------------------------------
# App settings — persisted to JSON file
# ---------------------------------------------------------------------------
_DEFAULT_SETTINGS = {"resync_mode": "fast"}

def _load_settings() -> dict:
    try:
        with open(_SETTINGS_PATH) as f:
            data = json.load(f)
        return {**_DEFAULT_SETTINGS, **data}
    except (FileNotFoundError, json.JSONDecodeError):
        return dict(_DEFAULT_SETTINGS)

def _save_settings(data: dict):
    os.makedirs(os.path.dirname(_SETTINGS_PATH), exist_ok=True)
    with open(_SETTINGS_PATH, "w") as f:
        json.dump(data, f, indent=2)

# Public paths that never require a token
_PUBLIC_PATHS = {"/api/login", "/login", "/favicon.ico", "/", "/favicon.png", "/karaoke",
                 "/internal/keyboard_event"}
# /keyboard/ is public so the karaoke page can list and play songs without login
_PUBLIC_PREFIXES = ("/_app/", "/login", "/karaoke", "/keyboard/")


async def _serial_status_broadcaster():
    """Periodically push serial_status (including stale flag) via SSE."""
    import time
    while True:
        await asyncio.sleep(10)
        try:
            connected = bridge._serial is not None and bridge._serial.is_open
            now = time.monotonic()
            last = bridge._last_frame_time
            ref = last if last > 0 else bridge._connected_since
            stale = connected and ref > 0 and (now - ref) > 30
            bridge._dispatch({"event": "serial_status", "connected": connected, "stale": stale})
        except Exception:
            pass


@asynccontextmanager
async def lifespan(app: FastAPI):
    auth.bootstrap()
    loop = asyncio.get_event_loop()
    bridge._port = SERIAL_PORT
    try:
        bridge.start(loop)
    except Exception as exc:
        logger.warning("Could not open serial port %s: %s – running in offline mode", SERIAL_PORT, exc)
    asyncio.create_task(_serial_status_broadcaster())
    asyncio.create_task(_battery_low_watcher())
    asyncio.create_task(_heap_low_watcher())
    asyncio.create_task(_mesh_event_watcher())
    yield
    bridge.stop()


app = FastAPI(title="Sparkles API", lifespan=lifespan)
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)


class AuthMiddleware(BaseHTTPMiddleware):
    async def dispatch(self, request: Request, call_next):
        path = request.url.path

        # always allow public paths
        if path in _PUBLIC_PATHS or any(path.startswith(p) for p in _PUBLIC_PREFIXES):
            return await call_next(request)

        user = auth.get_current_user(request)
        accepts_html = "text/html" in request.headers.get("accept", "")

        if user is None:
            if accepts_html:
                return RedirectResponse("/", status_code=302)
            return JSONResponse({"detail": "Not authenticated"}, status_code=401)

        role = user.get("role", "")

        # static page access check
        if accepts_html or path == "/" or "." not in path.split("/")[-1]:
            if not auth.check_page_access(path, role):
                if accepts_html:
                    return RedirectResponse("/login", status_code=302)
                return JSONResponse({"detail": "Forbidden"}, status_code=403)
        else:
            # API / asset access check
            if not auth.check_api_access(path, request.method, role):
                return JSONResponse({"detail": "Forbidden"}, status_code=403)

        return await call_next(request)


app.add_middleware(AuthMiddleware)

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _assert_connected():
    if not bridge.is_connected:
        raise HTTPException(503, detail="Master not connected — serial bridge is down")


def _send(payload: dict):
    _assert_connected()
    bridge.send(payload)
    _announce_animation(payload)


def _ok(msg: str = "OK"):
    return JSONResponse({"status": True, "msg": msg})


async def _request(cmd: dict, event: str, timeout: float = 4.0):
    _assert_connected()
    result = await bridge.request(cmd, event, timeout)
    if result is None:
        raise HTTPException(504, detail=f"No response from device (timeout waiting for '{event}')")
    return result


# ---------------------------------------------------------------------------
# Auth  /api/login  /api/logout  /api/me
# ---------------------------------------------------------------------------

class LoginRequest(BaseModel):
    username: str
    password: str


@app.post("/api/login")
async def login(body: LoginRequest):
    role = auth.authenticate(body.username, body.password)
    if role is None:
        raise HTTPException(401, detail="Invalid credentials")
    token = auth.create_token(body.username, role)
    response = JSONResponse({"status": True, "role": role, "username": body.username})
    response.set_cookie(
        auth.COOKIE_NAME, token,
        httponly=True, samesite="lax", max_age=60 * 60 * 24 * 7,
    )
    return response


@app.post("/api/logout")
async def logout():
    response = JSONResponse({"status": True})
    response.delete_cookie(auth.COOKIE_NAME)
    return response


@app.get("/api/me")
async def me(request: Request):
    user = auth.get_current_user(request)
    if user is None:
        raise HTTPException(401, detail="Not authenticated")
    cfg = auth.load_config()
    page_rules = cfg.get("access", {}).get("pages", {})
    allowed_pages = [
        p for p in page_rules
        if p != "*" and auth.check_page_access(p, user["role"])
    ]
    return {"username": user["sub"], "role": user["role"], "allowedPages": allowed_pages}


# ---------------------------------------------------------------------------
# SSE  /events
# ---------------------------------------------------------------------------

@app.get("/events")
async def sse_events(request: Request) -> StreamingResponse:
    queue = bridge.subscribe()

    async def generator() -> AsyncGenerator[str, None]:
        try:
            while True:
                if await request.is_disconnected():
                    break
                try:
                    frame = await asyncio.wait_for(queue.get(), timeout=15)
                    event_name = frame.get("event", "message")
                    data = json.dumps(frame)
                    yield f"event: {event_name}\ndata: {data}\n\n"
                except asyncio.TimeoutError:
                    yield ": keep-alive\n\n"
        finally:
            bridge.unsubscribe(queue)

    return StreamingResponse(generator(), media_type="text/event-stream",
                             headers={"Cache-Control": "no-cache", "X-Accel-Buffering": "no"})


# ---------------------------------------------------------------------------
# Serial log SSE  /serialLog
# ---------------------------------------------------------------------------

@app.get("/serialLog")
async def serial_log(request: Request) -> StreamingResponse:
    snapshot, queue = bridge.subscribe_log()

    async def generator() -> AsyncGenerator[str, None]:
        # send buffered lines first
        for line in snapshot:
            yield f"event: serial_log\ndata: {json.dumps(line)}\n\n"
        try:
            while True:
                if await request.is_disconnected():
                    break
                try:
                    line = await asyncio.wait_for(queue.get(), timeout=15)
                    yield f"event: serial_log\ndata: {json.dumps(line)}\n\n"
                except asyncio.TimeoutError:
                    yield ": keep-alive\n\n"
        finally:
            bridge.unsubscribe_log(queue)

    return StreamingResponse(generator(), media_type="text/event-stream",
                             headers={"Cache-Control": "no-cache", "X-Accel-Buffering": "no"})


# ---------------------------------------------------------------------------
# Meshtastic log relay → T-Beam → T-Deck (sparkles channel)
# ---------------------------------------------------------------------------

_mesh_lock = asyncio.Lock()  # one meshtastic CLI talking to the radio at a time


async def _mesh_send(text: str, channel_index: int) -> str:
    """Send a text message out via the T-Beam radio using the meshtastic CLI."""
    if shutil.which(MESH_BIN) is None and not os.path.isfile(MESH_BIN):
        raise HTTPException(503, detail=f"meshtastic CLI not found ('{MESH_BIN}') — install it on the API host")

    args = [MESH_BIN]
    if MESH_PORT:
        args += ["--port", MESH_PORT]
    args += ["--sendtext", text, "--ch-index", str(channel_index)]

    async with _mesh_lock:
        proc = await asyncio.create_subprocess_exec(
            *args, stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.STDOUT)
        try:
            out, _ = await asyncio.wait_for(proc.communicate(), timeout=30)
        except asyncio.TimeoutError:
            proc.kill()
            raise HTTPException(504, detail="meshtastic send timed out (radio busy or unplugged?)")

    output = out.decode(errors="replace")
    if proc.returncode != 0 or "Sending text" not in output:
        raise HTTPException(502, detail=f"meshtastic send failed: {output.strip()[-300:]}")
    return text


class LogMessage(BaseModel):
    message: str
    level: str | None = None         # optional tag, e.g. "INFO" → prefixes the text
    channelIndex: int | None = None  # override the default sparkles channel


@app.post("/log")
async def send_log(body: LogMessage):
    """Relay an arbitrary log message over Meshtastic to the T-Deck."""
    text = body.message if not body.level else f"[{body.level}] {body.message}"
    text = text.encode()[:200].decode(errors="ignore")  # meshtastic text payload limit
    if not text:
        raise HTTPException(422, detail="message is empty")
    ch = body.channelIndex if body.channelIndex is not None else MESH_CHANNEL
    sent = await _mesh_send(text, ch)
    logger.info("mesh log → ch%d: %s", ch, sent)
    return _ok(sent)


# animations worth announcing on the T-Deck — excludes background shimmer (per
# request) and MIDI (streamed continuously, never sent as a discrete command)
_ANIMATION_CMDS = {
    "animate_toggle", "blink", "blink_all", "blink_battery_all",
    "strobe_all", "bioluminescence", "breath", "candle_all",
}


def _announce_animation(payload: dict):
    """Fire-and-forget a T-Deck log when an animation command is triggered."""
    cmd = payload.get("cmd", "")
    if cmd not in _ANIMATION_CMDS:
        return

    async def _task():
        try:
            await _mesh_send(f"🎬 animation: {cmd}", MESH_CHANNEL)
        except Exception as exc:
            logger.warning("animation relay failed for %s: %s", cmd, exc)

    try:
        asyncio.get_running_loop().create_task(_task())
    except RuntimeError:
        pass  # not called from the event loop — skip


async def _battery_low_watcher():
    """Relay a log to the T-Deck when a board's battery dips below MESH_BATTERY_LOW.

    Edge-triggered: fires once when a board crosses the threshold, re-arms when
    it recovers — so a steady stream of update_board frames won't spam the radio.
    """
    queue = bridge.subscribe()
    low: set[int] = set()
    try:
        while True:
            frame = await queue.get()
            if frame.get("event") != "update_board":
                continue
            cid = frame.get("id")
            bat = frame.get("batteryPercentage")
            if cid is None or not isinstance(bat, (int, float)):
                continue
            if bat < MESH_BATTERY_LOW:
                if cid not in low:
                    low.add(cid)
                    try:
                        await _mesh_send(f"⚠️ board {cid} battery low ({bat:.0f}%)", MESH_CHANNEL)
                    except HTTPException as exc:
                        logger.warning("battery-low relay failed for board %s: %s", cid, exc.detail)
            else:
                low.discard(cid)  # recovered → re-arm for the next dip
    finally:
        bridge.unsubscribe(queue)


async def _heap_low_watcher():
    """Relay a T-Deck log when the master's free heap sinks below MESH_HEAP_LOW —
    advance warning that a leak is heading for a hang, so it can be restarted
    at a convenient moment instead of dying mid-evening.

    Edge-triggered like the battery watcher: fires once on the crossing,
    re-arms when the heap recovers.
    """
    queue = bridge.subscribe()
    low = False
    try:
        while True:
            frame = await queue.get()
            if frame.get("event") != "heap":
                continue
            free = frame.get("free")
            if not isinstance(free, int):
                continue
            if free < MESH_HEAP_LOW:
                if not low:
                    low = True
                    try:
                        await _mesh_send(f"⚠️ master heap low ({free // 1024} KB free)", MESH_CHANNEL)
                    except Exception as exc:
                        logger.warning("heap-low relay failed: %s", exc)
            else:
                low = False  # recovered → re-arm for the next dip
    finally:
        bridge.unsubscribe(queue)


async def _mesh_event_watcher():
    """Relay notable device events to the T-Deck.

    installation_active comes from serial_bridge on the music resume edge (gap >
    SPARKLES_IDLE_RESUME); sleep_phase start/end comes from the master firmware.
    """
    queue = bridge.subscribe()
    try:
        while True:
            frame = await queue.get()
            event = frame.get("event")
            if event == "installation_active":
                text = "🎶 people are using the installation"
            elif event == "sleep_phase":
                text = "😴 lamps going to sleep" if frame.get("status") == "start" else "🌅 lamps waking up"
            else:
                continue
            try:
                await _mesh_send(text, MESH_CHANNEL)
            except Exception as exc:
                logger.warning("mesh event relay failed (%s): %s", event, exc)
    finally:
        bridge.unsubscribe(queue)


# ---------------------------------------------------------------------------
# Animation
# ---------------------------------------------------------------------------

@app.get("/commandSetLogLevel")
async def command_set_log_level(level: int = 0):
    _send({"cmd": "set_log_level", "level": level})
    return _ok()

@app.get("/commandAnimate")
async def command_animate():
    settings = _load_settings()
    if settings.get("resync_mode", "fast") != "off":
        asyncio.create_task(_presync_then_send({"cmd": "animate_toggle"}, delay_s=5))
    else:
        _send({"cmd": "animate_toggle"})
    return _ok()


@app.get("/commandAnimationOff")
async def command_animation_off():
    _send({"cmd": "animation_off"})
    return _ok()


@app.get("/getAnimateStatus")
async def get_animate_status():
    return await _request({"cmd": "get_animate_status"}, "animate_status")


@app.get("/commandBlink")
async def command_blink(boardId: int = Query(...)):
    _send({"cmd": "blink", "boardId": boardId})
    return _ok()


async def _presync_then_send(payload: dict, delay_s: float):
    """Send sync first, wait delay_s, then send the animation payload."""
    settings = _load_settings()
    mode = settings.get("resync_mode", "fast")
    if mode == "fast":
        _send({"cmd": "sync_fast"})
        await asyncio.sleep(delay_s)
    elif mode == "slow":
        _send({"cmd": "sync_all"})
        await asyncio.sleep(delay_s * 5)
    _send(payload)


@app.get("/commandBlinkAll")
async def command_blink_all():
    _send({"cmd": "blink_all"})
    return _ok()


@app.get("/commandBatteryBlinkAll")
async def command_battery_blink_all():
    _send({"cmd": "blink_battery_all"})
    return _ok()


@app.get("/commandStrobeAll")
async def command_strobe_all(
    frequency: int = Query(default=10),
    duration: int = Query(default=3000),
    hue: int = Query(default=0),
    saturation: int = Query(default=0),
    brightness: int = Query(default=255),
):
    payload = {"cmd": "strobe_all", "frequency": frequency, "duration": duration,
               "hue": hue, "saturation": saturation, "brightness": brightness}
    _send(payload)
    return _ok()


# ---------------------------------------------------------------------------
# Timer test
# ---------------------------------------------------------------------------

@app.get("/commandTimerTest")
async def command_timer_test():
    _send({"cmd": "timer_test"})
    return _ok()


@app.get("/sleepTest")
async def sleep_test(
    sleepSeconds: int = Query(default=15, ge=5, le=3600),
    phaseSeconds: int = Query(default=60, ge=10, le=86400),
):
    _send({"cmd": "test_sleep_cycle",
           "sleep_duration_s": sleepSeconds,
           "phase_duration_s": phaseSeconds})
    return _ok()


@app.get("/sleepTestCancel")
async def sleep_test_cancel():
    _send({"cmd": "test_sleep_cancel"})
    return _ok()


@app.get("/sleepTestReport")
async def sleep_test_report():
    # events for the current/most recent run, in order — survives page reloads,
    # closed browsers, and a sparkles service restart (persisted to disk)
    return {"events": bridge.sleep_test_events}


# ---------------------------------------------------------------------------
# Sync
# ---------------------------------------------------------------------------

@app.get("/commandSync")
async def command_sync(index: int = Query(...)):
    _send({"cmd": "sync", "index": index})
    return _ok()


@app.get("/commandSyncAll")
async def command_sync_all():
    _send({"cmd": "sync_all"})
    return _ok()


@app.get("/commandSyncFast")
async def command_sync_fast():
    _send({"cmd": "sync_fast"})
    return _ok()


@app.get("/commandTestSleepCycle")
async def command_test_sleep_cycle(
    sleep_duration_s: int = Query(default=15),
    phase_duration_s: int = Query(default=60),
) -> StreamingResponse:
    """Run a compressed sleep cycle test, streaming JSON events as SSE."""
    _send({"cmd": "test_sleep_cycle",
           "sleep_duration_s": sleep_duration_s,
           "phase_duration_s": phase_duration_s})

    _SLEEP_TEST_EVENTS = {
        "sleep_test_start", "sleep_test_resync_start", "sleep_test_resync_done",
        "sleep_test_broadcast_start", "sleep_test_cycle_heartbeat",
        "sleep_test_unexpected_wakeup", "sleep_test_broadcast_end",
        "sleep_test_waiting_for_wakeup", "sleep_test_client_back", "sleep_test_done",
    }

    queue = bridge.subscribe()

    async def generator() -> AsyncGenerator[str, None]:
        timeout_s = phase_duration_s + sleep_duration_s + 120
        deadline = asyncio.get_event_loop().time() + timeout_s
        try:
            while True:
                remaining = deadline - asyncio.get_event_loop().time()
                if remaining <= 0:
                    yield f"event: sleep_test_timeout\ndata: {{}}\n\n"
                    break
                try:
                    frame = await asyncio.wait_for(queue.get(), timeout=min(remaining, 15))
                    if frame.get("event") not in _SLEEP_TEST_EVENTS:
                        continue
                    yield f"event: {frame['event']}\ndata: {json.dumps(frame)}\n\n"
                    if frame.get("event") == "sleep_test_done":
                        break
                except asyncio.TimeoutError:
                    yield ": keep-alive\n\n"
        finally:
            bridge.unsubscribe(queue)

    return StreamingResponse(generator(), media_type="text/event-stream",
                             headers={"Cache-Control": "no-cache", "X-Accel-Buffering": "no"})


@app.get("/appSettings")
async def get_app_settings():
    return _load_settings()


class AppSettingsUpdate(BaseModel):
    resync_mode: str | None = None  # "fast" | "slow" | "off"


@app.post("/appSettings")
async def set_app_settings(body: AppSettingsUpdate):
    settings = _load_settings()
    if body.resync_mode is not None:
        if body.resync_mode not in ("fast", "slow", "off"):
            raise HTTPException(status_code=422, detail="resync_mode must be fast, slow, or off")
        settings["resync_mode"] = body.resync_mode
    _save_settings(settings)
    return settings


# ---------------------------------------------------------------------------
# Board positions
# ---------------------------------------------------------------------------

@app.get("/submitPositions")
async def submit_positions(
    boardId: int = Query(...),
    xpos: float = Query(...),
    ypos: float = Query(...),
):
    _send({"cmd": "submit_positions", "boardId": boardId, "xpos": xpos, "ypos": ypos})
    return _ok()


# ---------------------------------------------------------------------------
# Address list
# ---------------------------------------------------------------------------

@app.get("/getAddressList")
async def get_address_list(id: int | None = Query(default=None)):
    cmd = {"cmd": "get_address_list"}
    if id is not None:
        cmd["id"] = id
    return await _request(cmd, "address_list")


@app.get("/removeDevice")
async def remove_device(index: int = Query(...)):
    # fire-and-forget: removal shifts indices, so the result comes back as a
    # fresh update_board/address_list dump over SSE, not a single response
    _send({"cmd": "remove_device", "index": index})
    return _ok()


@app.get("/removeAllDevices")
async def remove_all_devices():
    _send({"cmd": "remove_all_devices"})
    return _ok()


# ---------------------------------------------------------------------------
# Time
# ---------------------------------------------------------------------------

@app.get("/setTime")
async def set_time(
    year: int = Query(...),
    month: int = Query(...),
    day: int = Query(...),
    hours: int = Query(...),
    minutes: int = Query(...),
    seconds: int = Query(...),
):
    # the browser is the only real time source here (no internet, no rtc):
    # set the pi's own clock too and mark it trusted for the rest of this boot
    stamp = f"{year:04d}-{month:02d}-{day:02d} {hours:02d}:{minutes:02d}:{seconds:02d}"
    try:
        subprocess.run(["sudo", "date", "-s", stamp], check=True, timeout=5, capture_output=True)
        subprocess.run(["sudo", "fake-hwclock", "save"], timeout=5, capture_output=True)
    except Exception as exc:
        logger.warning("Could not set pi clock: %s", exc)
    bridge.mark_clock_trusted()
    _send({"cmd": "set_time", "year": year, "month": month, "day": day,
           "hours": hours, "minutes": minutes, "seconds": seconds})
    return _ok()


@app.get("/setSleepTime")
async def set_sleep_time(
    hours: int = Query(...),
    minutes: int = Query(...),
    seconds: int = Query(...),
):
    bridge.set_schedule(sleep={"hours": hours, "minutes": minutes, "seconds": seconds})
    _send({"cmd": "set_sleep_time", "hours": hours, "minutes": minutes, "seconds": seconds})
    return _ok()


@app.get("/setWakeupTime")
async def set_wakeup_time(
    hours: int = Query(...),
    minutes: int = Query(...),
    seconds: int = Query(...),
):
    bridge.set_schedule(wakeup={"hours": hours, "minutes": minutes, "seconds": seconds})
    _send({"cmd": "set_wakeup_time", "hours": hours, "minutes": minutes, "seconds": seconds})
    return _ok()


@app.get("/getSystemInfo")
async def get_system_info():
    return await _request({"cmd": "get_system_info"}, "system_info")


# ---------------------------------------------------------------------------
# MIDI params
# ---------------------------------------------------------------------------

@app.get("/colors")
async def get_colors():
    return bridge.colors


@app.get("/setColors")
async def set_colors(
    midiHue: int | None = Query(default=None, ge=0, le=360),
    midiSaturation: int | None = Query(default=None, ge=0, le=255),
    shimmerHue: int | None = Query(default=None, ge=0, le=360),
    shimmerSaturation: int | None = Query(default=None, ge=0, le=255),
):
    midi = {k: v for k, v in (("hue", midiHue), ("saturation", midiSaturation)) if v is not None}
    shimmer = {k: v for k, v in (("hue", shimmerHue), ("saturation", shimmerSaturation)) if v is not None}
    bridge.set_colors(midi=midi or None, shimmer=shimmer or None)
    return _ok()


@app.get("/getMidiParams")
@app.get("/commandGetMidiParams")
async def get_midi_params():
    return await _request({"cmd": "get_midi_params"}, "midi_params")


@app.get("/setMidiParams")
async def set_midi_params(
    minVal: int = Query(...),
    maxVal: int = Query(...),
    minSat: int = Query(...),
    maxSat: int = Query(...),
    midiHue: int = Query(...),
    midiSaturation: int = Query(...),
    rangeMin: int = Query(...),
    rangeMax: int = Query(...),
    minDb: float = Query(...),
    maxDb: float = Query(...),
    mode: int = Query(...),
    distance: int = Query(default=100),
    distanceSwitch: bool = Query(default=False),
    distanceMode: int = Query(default=0),
):
    _send({
        "cmd": "set_midi_params",
        "minVal": minVal, "maxVal": maxVal,
        "minSat": minSat, "maxSat": maxSat,
        "midiHue": midiHue, "midiSaturation": midiSaturation,
        "rangeMin": rangeMin, "rangeMax": rangeMax,
        "minDb": minDb, "maxDb": maxDb,
        "mode": mode,
        "distance": distance, "distanceSwitch": distanceSwitch, "distanceMode": distanceMode,
    })
    return _ok()


# ---------------------------------------------------------------------------
# Darkroom params
# ---------------------------------------------------------------------------

@app.get("/getDarkroomParams")
async def get_darkroom_params():
    return await _request({"cmd": "get_darkroom_params"}, "darkroom_params")


@app.get("/setDarkroomParams")
async def set_darkroom_params(
    strobeMin: int = Query(...),
    strobeMax: int = Query(...),
    redlightMin: int = Query(...),
    redlightMax: int = Query(...),
    candlelightMin: int = Query(...),
    candlelightMax: int = Query(...),
    redLightEnabled: bool = Query(...),
    candleLightEnabled: bool = Query(...),
):
    _send({
        "cmd": "set_darkroom_params",
        "strobeMin": strobeMin, "strobeMax": strobeMax,
        "redlightMin": redlightMin, "redlightMax": redlightMax,
        "candlelightMin": candlelightMin, "candlelightMax": candlelightMax,
        "redLightEnabled": redLightEnabled, "candleLightEnabled": candleLightEnabled,
    })
    return _ok()


# ---------------------------------------------------------------------------
# Calibration
# ---------------------------------------------------------------------------

@app.get("/commandStartCalibration")
async def command_start_calibration():
    _send({"cmd": "calibration_start"})
    return _ok()


@app.get("/commandCancelCalibration")
async def command_cancel_calibration():
    _send({"cmd": "calibration_cancel"})
    return _ok()


@app.get("/commandResetCalibration")
async def command_reset_calibration():
    _send({"cmd": "calibration_reset"})
    return _ok()


@app.get("/commandContinueCalibration")
async def command_continue_calibration(x: float = Query(...), y: float = Query(...)):
    _send({"cmd": "calibration_continue", "x": x, "y": y})
    return _ok()


@app.get("/commandEndCalibration")
async def command_end_calibration():
    _send({"cmd": "calibration_end"})
    return _ok()


@app.get("/commandTestCalibration")
async def command_test_calibration():
    _send({"cmd": "calibration_test"})
    return _ok()


@app.get("/commandCalibrate")
async def command_calibrate(boardId: int = Query(...)):
    _send({"cmd": "calibration_calibrate", "boardId": boardId})
    return _ok()


# ---------------------------------------------------------------------------
# Distance calibration
# ---------------------------------------------------------------------------

@app.get("/commandStartDistanceCalibration")
async def command_start_distance_calibration():
    _send({"cmd": "dist_cal_start"})
    return _ok()


@app.get("/commandContinueDistanceCalibration")
async def command_continue_distance_calibration():
    _send({"cmd": "dist_cal_continue"})
    return _ok()


@app.get("/commandEndDistanceCalibration")
async def command_end_distance_calibration():
    _send({"cmd": "dist_cal_end"})
    return _ok()


@app.get("/commandCancelDistanceCalibration")
async def command_cancel_distance_calibration():
    _send({"cmd": "dist_cal_cancel"})
    return _ok()


@app.get("/commandAbortDistanceCalibration")
async def command_abort_distance_calibration():
    _send({"cmd": "dist_cal_abort"})
    return _ok()


# ---------------------------------------------------------------------------
# Admin / system
# ---------------------------------------------------------------------------

@app.get("/commandOTAUpdate")
async def command_ota_update():
    _send({"cmd": "ota_update"})
    return _ok()


@app.get("/toggleTestMode")
async def toggle_test_mode(spacing: float = Query(default=1.0)):
    result = await _request({"cmd": "toggle_test_mode", "spacing": spacing}, "test_mode")
    return result


@app.get("/toggleLogging")
async def toggle_logging():
    result = await _request({"cmd": "toggle_logging"}, "logging")
    return result


@app.get("/reannounce")
async def reannounce():
    _send({"cmd": "reannounce"})
    return _ok()


@app.get("/resetSystem")
async def reset_system():
    _send({"cmd": "reset_system"})
    return _ok()


@app.get("/factoryReset")
async def factory_reset():
    _send({"cmd": "factory_reset"})
    return _ok()


@app.post("/setMaintenanceMode")
async def set_maintenance_mode(active: bool = Query(...)):
    _send({"cmd": "set_maintenance_mode", "active": active})
    return _ok()


@app.get("/commandShimmer")
async def command_shimmer(boardId: int = Query(default=-1)):
    _send({"cmd": "shimmer", "boardId": boardId})
    return _ok()


@app.get("/commandBioluminescence")
async def command_bioluminescence(
    minInterval: int = Query(default=2000),
    maxInterval: int = Query(default=8000),
    fadeDuration: int = Query(default=1500),
    repetitions: int = Query(default=0),
    hue: int = Query(default=140),
    hueVariance: int = Query(default=20),
    saturation: int = Query(default=220),
    brightness: int = Query(default=80),
):
    _send({"cmd": "bioluminescence", "minInterval": minInterval, "maxInterval": maxInterval,
           "fadeDuration": fadeDuration, "repetitions": repetitions, "hue": hue,
           "hueVariance": hueVariance, "saturation": saturation, "brightness": brightness})
    return _ok()


@app.get("/commandBreath")
async def command_breath(
    cycleDuration: int = Query(default=4000),
    spreadDelay: int = Query(default=2000),
    repetitions: int = Query(default=0),
    hue: int = Query(default=96),
    saturation: int = Query(default=180),
    brightness: int = Query(default=200),
):
    _send({"cmd": "breath", "cycleDuration": cycleDuration, "spreadDelay": spreadDelay,
           "repetitions": repetitions, "hue": hue, "saturation": saturation, "brightness": brightness})
    return _ok()


@app.get("/commandCandleAll")
async def command_candle_all(
    hue: int = Query(default=20),
    saturation: int = Query(default=210),
    brightness: int = Query(default=180),
):
    _send({"cmd": "candle_all", "hue": hue, "saturation": saturation, "brightness": brightness})
    return _ok()


@app.get("/commandMessage")
async def command_message(boardId: int = Query(...)):
    _send({"cmd": "command_message", "boardId": boardId})
    return _ok()


# ---------------------------------------------------------------------------
# Firmware — OTA serve, upload, compile
# ---------------------------------------------------------------------------

@app.get("/serial-status")
async def serial_status():
    import time
    connected = bridge._serial is not None and bridge._serial.is_open
    now = time.monotonic()
    last = bridge._last_frame_time
    ref = last if last > 0 else bridge._connected_since  # fallback: time since port opened
    stale = connected and ref > 0 and (now - ref) > 30
    return {"connected": connected, "stale": stale, "lastFrameAge": round(now - last) if last > 0 else None}


@app.get("/current-version")
async def current_version():
    try:
        return {"version": fw_compile.read_version()}
    except Exception as e:
        raise HTTPException(500, detail=str(e))


@app.get("/compile-stream")
async def compile_stream(
    request: Request,
    target: str = Query(..., pattern="^(client|master|both)$"),
    incrementVersion: bool = Query(default=False),
) -> StreamingResponse:
    """SSE stream of PlatformIO compile output."""

    async def generator() -> AsyncGenerator[str, None]:
        def emit(line: str) -> str:
            return f"event: compile_log\ndata: {json.dumps(line)}\n\n"

        try:
            yield emit("[pulling latest code…]")
            async for line in fw_compile.git_pull():
                yield emit(line)

            if incrementVersion and target in ("master", "both"):
                old, new = fw_compile.increment_version()
                yield emit(f"[version bumped {old} → {new}]")

            if target in ("client", "both"):
                yield emit("[compiling client…]")
                async for line in fw_compile.compile_client():
                    if await request.is_disconnected():
                        return
                    yield emit(line)
                yield emit("[client done]")

            if target in ("master", "both"):
                yield emit("[compiling & flashing master…]")
                bridge.release_port()
                await asyncio.sleep(0.5)
                try:
                    async for line in fw_compile.compile_master():
                        if await request.is_disconnected():
                            return
                        yield emit(line)
                finally:
                    bridge.resume_port()
                yield emit("[master done]")

            yield f"event: compile_done\ndata: {json.dumps({'success': True})}\n\n"

        except Exception as exc:
            yield emit(f"[ERROR] {exc}")
            yield f"event: compile_done\ndata: {json.dumps({'success': False, 'error': str(exc)})}\n\n"

    return StreamingResponse(
        generator(),
        media_type="text/event-stream",
        headers={"Cache-Control": "no-cache", "X-Accel-Buffering": "no"},
    )


# ---------------------------------------------------------------------------
# Firmware OTA
# ---------------------------------------------------------------------------

@app.get("/firmware.bin")
async def get_firmware():
    if not os.path.isfile(FIRMWARE_PATH):
        raise HTTPException(404, detail="No firmware uploaded yet")
    return FileResponse(FIRMWARE_PATH, media_type="application/octet-stream",
                        filename="firmware.bin")


@app.post("/upload-firmware")
async def upload_firmware(file: UploadFile = File(...)):
    if not file.filename.endswith(".bin"):
        raise HTTPException(400, detail="Expected a .bin file")
    data = await file.read()
    with open(FIRMWARE_PATH, "wb") as f:
        f.write(data)
    logger.info("Firmware uploaded: %d bytes → %s", len(data), FIRMWARE_PATH)
    return JSONResponse({"status": True, "size": len(data)})


# ---------------------------------------------------------------------------
# Keyboard / MIDI player
# ---------------------------------------------------------------------------

_SONGS_DIR        = os.environ.get("SPARKLES_SONGS_DIR", "/home/julian/sparkles/songs")
_KEYBOARD_CMD_SOCK = os.environ.get("SPARKLES_KEYBOARD_SOCK", "/tmp/keyboard_cmd.sock")

# current playback state, so the (login-free) karaoke page can poll it and clear
# itself when a song stops — including when a key press on the keyboard stops it
_playback = {"song": None, "playing": False}


def _keyboard_cmd(cmd: dict):
    """Send a command to keyboard_midi.py via its Unix socket."""
    import socket as _socket
    try:
        s = _socket.socket(_socket.AF_UNIX, _socket.SOCK_STREAM)
        s.settimeout(2.0)
        s.connect(_KEYBOARD_CMD_SOCK)
        s.sendall((json.dumps(cmd) + "\n").encode())
        s.close()
    except Exception as exc:
        raise HTTPException(503, detail=f"keyboard_midi not reachable: {exc}")


@app.get("/keyboard/songs")
async def keyboard_songs():
    """List available MIDI files in the songs directory."""
    if not os.path.isdir(_SONGS_DIR):
        return {"songs": []}
    files = sorted(
        f for f in os.listdir(_SONGS_DIR)
        if f.lower().endswith(".mid") or f.lower().endswith(".midi")
    )
    return {"songs": files}


@app.post("/keyboard/play")
async def keyboard_play(song: str = Query(...)):
    _keyboard_cmd({"cmd": "play", "file": song})
    _playback.update(song=song, playing=True)

    async def _announce():
        try:
            await _mesh_send(f"🎤 karaoke: {song}", MESH_CHANNEL)
        except Exception as exc:
            logger.warning("karaoke relay failed: %s", exc)
    asyncio.create_task(_announce())
    return _ok()


@app.post("/keyboard/stop")
async def keyboard_stop():
    _keyboard_cmd({"cmd": "stop"})
    _playback.update(playing=False)
    return _ok()


@app.get("/keyboard/status")
async def keyboard_status():
    """Current playback state — polled by the karaoke page so it clears when a
    song stops (including when a key press on the keyboard stops it)."""
    return _playback


@app.post("/internal/keyboard_event")
async def keyboard_event(request: Request):
    """Receive status callbacks from keyboard_midi.py and fan out via SSE."""
    body = await request.json()
    if body.get("event") == "keyboard_playback" and body.get("status") in ("stopped", "finished", "error"):
        _playback.update(playing=False)
    bridge._dispatch(body)
    return {"ok": True}


_build_dir = os.path.join(os.path.dirname(__file__), "..", "sparkles-ui", "build")
_index_html = os.path.join(_build_dir, "index.html")

if os.path.isdir(_build_dir):
    _build_root = os.path.realpath(_build_dir)

    # Serve static assets normally
    app.mount("/_app", StaticFiles(directory=os.path.join(_build_dir, "_app")), name="assets")

    # SPA catch-all: serve the file if it exists, otherwise index.html
    @app.get("/{full_path:path}")
    async def spa_fallback(full_path: str):
        # realpath + prefix check keeps ../ and absolute paths inside the build dir
        candidate = os.path.realpath(os.path.join(_build_root, full_path))
        if candidate.startswith(_build_root + os.sep) and os.path.isfile(candidate):
            return FileResponse(candidate)
        return FileResponse(_index_html)
