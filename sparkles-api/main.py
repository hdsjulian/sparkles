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

from fastapi import FastAPI, Query, Request, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse, StreamingResponse
from fastapi.staticfiles import StaticFiles

from serial_bridge import bridge

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(name)s %(levelname)s %(message)s")
logger = logging.getLogger("sparkles")

SERIAL_PORT = os.environ.get("SPARKLES_PORT", "/dev/ttyACM0")


@asynccontextmanager
async def lifespan(app: FastAPI):
    loop = asyncio.get_event_loop()
    bridge._port = SERIAL_PORT
    try:
        bridge.start(loop)
    except Exception as exc:
        logger.warning("Could not open serial port %s: %s – running in offline mode", SERIAL_PORT, exc)
    yield
    bridge.stop()


app = FastAPI(title="Sparkles API", lifespan=lifespan)
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _send(payload: dict):
    bridge.send(payload)


def _ok(msg: str = "OK"):
    return JSONResponse({"status": True, "msg": msg})


async def _request(cmd: dict, event: str, timeout: float = 10.0):
    result = await bridge.request(cmd, event, timeout)
    if result is None:
        raise HTTPException(504, detail=f"No response from device (timeout waiting for '{event}')")
    return result


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
async def toggle_test_mode():
    result = await _request({"cmd": "toggle_test_mode"}, "test_mode")
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


@app.get("/commandMessage")
async def command_message(boardId: int = Query(...)):
    _send({"cmd": "command_message", "boardId": boardId})
    return _ok()


_build_dir = os.path.join(os.path.dirname(__file__), "..", "sparkles-ui", "build")
if os.path.isdir(_build_dir):
    app.mount("/", StaticFiles(directory=_build_dir, html=True), name="static")
