"""
Sparkles FastAPI server – mirrors WebServer.cpp over USB serial.

Start:  uvicorn main:app --host 0.0.0.0 --port 8080
Serial: /dev/ttyACM0  (override with SPARKLES_PORT env var)
"""

import asyncio
import json
import logging
import os
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

# Public paths that never require a token
_PUBLIC_PATHS = {"/api/login", "/login", "/favicon.ico", "/", "/favicon.png"}
_PUBLIC_PREFIXES = ("/_app/", "/login")


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
# Animation
# ---------------------------------------------------------------------------

@app.get("/commandAnimate")
async def command_animate():
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
    _send({"cmd": "strobe_all", "frequency": frequency, "duration": duration,
           "hue": hue, "saturation": saturation, "brightness": brightness})
    return _ok()


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
    _send({"cmd": "set_time", "year": year, "month": month, "day": day,
           "hours": hours, "minutes": minutes, "seconds": seconds})
    return _ok()


@app.get("/setSleepTime")
async def set_sleep_time(
    hours: int = Query(...),
    minutes: int = Query(...),
    seconds: int = Query(...),
):
    _send({"cmd": "set_sleep_time", "hours": hours, "minutes": minutes, "seconds": seconds})
    return _ok()


@app.get("/setWakeupTime")
async def set_wakeup_time(
    hours: int = Query(...),
    minutes: int = Query(...),
    seconds: int = Query(...),
):
    _send({"cmd": "set_wakeup_time", "hours": hours, "minutes": minutes, "seconds": seconds})
    return _ok()


@app.get("/getSystemInfo")
async def get_system_info():
    return await _request({"cmd": "get_system_info"}, "system_info")


# ---------------------------------------------------------------------------
# MIDI params
# ---------------------------------------------------------------------------

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
    target: str = Query(..., regex="^(client|master|both)$"),
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
    return _ok()


@app.post("/keyboard/stop")
async def keyboard_stop():
    _keyboard_cmd({"cmd": "stop"})
    return _ok()


@app.post("/internal/keyboard_event")
async def keyboard_event(request: Request):
    """Receive status callbacks from keyboard_midi.py and fan out via SSE."""
    body = await request.json()
    bridge._dispatch(body)
    return {"ok": True}


_build_dir = os.path.join(os.path.dirname(__file__), "..", "sparkles-ui", "build")
_index_html = os.path.join(_build_dir, "index.html")

if os.path.isdir(_build_dir):
    # Serve static assets normally
    app.mount("/_app", StaticFiles(directory=os.path.join(_build_dir, "_app")), name="assets")

    # SPA catch-all: serve the file if it exists, otherwise index.html
    @app.get("/{full_path:path}")
    async def spa_fallback(full_path: str):
        candidate = os.path.join(_build_dir, full_path)
        if os.path.isfile(candidate):
            return FileResponse(candidate)
        return FileResponse(_index_html)
