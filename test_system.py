#!/usr/bin/env python3
"""
System test for the Sparkles API.

Exercises every API endpoint, auto-verifies what can be checked, and
interactively prompts for physical observations that can't be measured.

Usage:
    python3 test_system.py [--host http://localhost] [--auto]

    --auto   Skip all manual prompts (mark them as skipped, not failed).
"""

import argparse
import sys
import time
import datetime
import requests

# ── ANSI colours ──────────────────────────────────────────────────────────────
R  = "\033[31m"
G  = "\033[32m"
Y  = "\033[33m"
B  = "\033[34m"
C  = "\033[36m"
W  = "\033[1m"
X  = "\033[0m"

def ok(msg):    print(f"  {G}✓{X} {msg}")
def fail(msg):  print(f"  {R}✗{X} {msg}")
def skip(msg):  print(f"  {Y}–{X} {msg}")
def info(msg):  print(f"  {C}·{X} {msg}")
def header(msg): print(f"\n{W}{B}{'─'*60}{X}\n{W} {msg}{X}\n{'─'*60}")


# ── Result tracking ───────────────────────────────────────────────────────────
results = []  # (name, status, detail)   status: pass|fail|skip


def record(name, status, detail=""):
    results.append((name, status, detail))
    if status == "pass":   ok(f"{name}{f'  {detail}' if detail else ''}")
    elif status == "fail": fail(f"{name}  {R}{detail}{X}")
    elif status == "skip": skip(f"{name}  {detail}")


# ── HTTP helpers ──────────────────────────────────────────────────────────────
def get(session, host, path, params=None, *, name, expect=200, expect_fields=(), timeout=8):
    url = host + path
    try:
        r = session.get(url, params=params, timeout=timeout)
    except requests.exceptions.Timeout:
        record(name, "fail", f"timeout after {timeout}s")
        return None
    except Exception as e:
        record(name, "fail", str(e))
        return None

    if r.status_code != expect:
        record(name, "fail", f"HTTP {r.status_code} (expected {expect})")
        return None

    try:
        body = r.json()
    except Exception:
        body = r.text

    for field in expect_fields:
        if not isinstance(body, dict) or field not in body:
            record(name, "fail", f"missing field '{field}' in response")
            return None

    return body


def post(session, host, path, json=None, *, name, expect=200, timeout=8):
    url = host + path
    try:
        r = session.post(url, json=json, timeout=timeout)
    except requests.exceptions.Timeout:
        record(name, "fail", f"timeout after {timeout}s")
        return None
    except Exception as e:
        record(name, "fail", str(e))
        return None

    if r.status_code != expect:
        record(name, "fail", f"HTTP {r.status_code} (expected {expect})")
        return None

    try:
        return r.json()
    except Exception:
        return r.text


# ── Manual prompt ─────────────────────────────────────────────────────────────
def prompt(name, question, auto_skip):
    if auto_skip:
        record(name, "skip", "(--auto)")
        return
    print(f"\n  {Y}?{X} {W}{question}{X}")
    print(f"    [{G}y{X}] yes / [{R}n{X}] no / [{Y}s{X}] skip  ", end="", flush=True)
    try:
        answer = input().strip().lower()
    except (EOFError, KeyboardInterrupt):
        answer = "s"
    if answer == "y":
        record(name, "pass")
    elif answer == "n":
        record(name, "fail", "user observed failure")
    else:
        record(name, "skip", "skipped by user")


# ── Main ──────────────────────────────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="http://localhost")
    parser.add_argument("--auto", action="store_true", help="skip all manual prompts")
    args = parser.parse_args()

    host = args.host.rstrip("/")
    auto = args.auto

    session = requests.Session()
    ctx = {}  # shared state between tests (e.g. boardId, midi params)

    print(f"\n{W}Sparkles system test{X}  →  {host}")
    if auto:
        print(f"{Y}--auto mode: manual prompts will be skipped{X}")

    # ── 1. Auth ───────────────────────────────────────────────────────────────
    header("1. Authentication")

    body = post(session, host, "/api/login",
                json={"username": "admin", "password": "admin"},
                name="POST /api/login (admin)")
    if body is None:
        fail("Cannot authenticate — aborting")
        sys.exit(1)
    record("POST /api/login (admin)", "pass", f"token issued")

    me = get(session, host, "/api/me",
             name="GET /api/me", expect_fields=["username", "role"])
    if me:
        record("GET /api/me", "pass", f"user={me.get('username')} role={me.get('role')}")

    # ── 2. Serial / health ────────────────────────────────────────────────────
    header("2. Serial bridge & health")

    status = get(session, host, "/serial-status",
                 name="GET /serial-status",
                 expect_fields=["connected", "stale"])
    if status:
        connected = status.get("connected")
        stale = status.get("stale")
        label = "connected" if connected else "DISCONNECTED"
        if stale:
            label += ", STALE"
        record("GET /serial-status", "pass", label)
        ctx["serial_ok"] = connected and not stale

    version = get(session, host, "/current-version",
                  name="GET /current-version")
    if version:
        record("GET /current-version", "pass", str(version))
        ctx["version"] = version

    # ── 3. Device info ────────────────────────────────────────────────────────
    header("3. Device info")

    sysinfo = get(session, host, "/getSystemInfo",
                  name="GET /getSystemInfo", timeout=6)
    if sysinfo:
        record("GET /getSystemInfo", "pass", f"keys: {list(sysinfo.keys())[:5]}")

    addresses = get(session, host, "/getAddressList",
                    name="GET /getAddressList", timeout=6)
    if addresses:
        devs = addresses if isinstance(addresses, list) else addresses.get("addresses", [])
        ctx["devices"] = devs
        ids = [d.get("boardId") or d.get("id") for d in devs]
        record("GET /getAddressList", "pass", f"{len(devs)} device(s): ids={ids}")
    else:
        ctx["devices"] = []

    animate = get(session, host, "/getAnimateStatus",
                  name="GET /getAnimateStatus", timeout=6)
    if animate:
        record("GET /getAnimateStatus", "pass", str(animate))

    # ── 4. Blink commands ─────────────────────────────────────────────────────
    header("4. Blink commands")

    body = get(session, host, "/commandBlinkAll", name="GET /commandBlinkAll")
    if body is not None:
        record("GET /commandBlinkAll", "pass")
    prompt("BlinkAll — physical", "Did ALL devices blink?", auto)

    if ctx["devices"]:
        bid = ctx["devices"][0].get("boardId") or ctx["devices"][0].get("id")
        body = get(session, host, "/commandBlink",
                   params={"boardId": bid},
                   name=f"GET /commandBlink?boardId={bid}")
        if body is not None:
            record(f"GET /commandBlink?boardId={bid}", "pass")
        prompt(f"Blink #{bid} — physical", f"Did device #{bid} blink?", auto)
    else:
        skip("No devices — skipping single-device blink test")
        record("GET /commandBlink (single)", "skip", "no devices available")

    body = get(session, host, "/commandBatteryBlinkAll",
               name="GET /commandBatteryBlinkAll")
    if body is not None:
        record("GET /commandBatteryBlinkAll", "pass")
    prompt("BatteryBlinkAll — physical", "Did devices show battery level blink?", auto)

    # ── 5. Strobe ─────────────────────────────────────────────────────────────
    header("5. Strobe")

    body = get(session, host, "/commandStrobeAll",
               params={"frequency": 10, "duration": 2000,
                       "hue": 0, "saturation": 0, "brightness": 200},
               name="GET /commandStrobeAll")
    if body is not None:
        record("GET /commandStrobeAll", "pass")
    prompt("StrobeAll — physical", "Did all devices strobe white for ~2 seconds?", auto)

    # ── 6. Animation toggle ───────────────────────────────────────────────────
    header("6. Animation")

    body = get(session, host, "/commandAnimate", name="GET /commandAnimate")
    if body is not None:
        record("GET /commandAnimate", "pass")
    prompt("commandAnimate — physical", "Did animation start/toggle on devices?", auto)

    body = get(session, host, "/commandAnimationOff", name="GET /commandAnimationOff")
    if body is not None:
        record("GET /commandAnimationOff", "pass")
    prompt("commandAnimationOff — physical", "Did animation stop on all devices?", auto)

    # ── 7. Sync ───────────────────────────────────────────────────────────────
    header("7. Sync")

    body = get(session, host, "/commandSyncAll", name="GET /commandSyncAll")
    if body is not None:
        record("GET /commandSyncAll", "pass")

    if ctx["devices"]:
        bid = ctx["devices"][0].get("boardId") or ctx["devices"][0].get("id")
        body = get(session, host, "/commandSync",
                   params={"index": bid},
                   name=f"GET /commandSync?index={bid}")
        if body is not None:
            record(f"GET /commandSync?index={bid}", "pass")

    # ── 8. MIDI params ────────────────────────────────────────────────────────
    header("8. MIDI parameters")

    midi = get(session, host, "/getMidiParams",
               name="GET /getMidiParams", timeout=6)
    if midi:
        ctx["midi"] = midi
        record("GET /getMidiParams", "pass", f"keys: {list(midi.keys())[:5]}")

    if ctx.get("midi"):
        m = ctx["midi"]
        body = get(session, host, "/setMidiParams", params={
            "minVal":          m.get("minVal", 0),
            "maxVal":          m.get("maxVal", 255),
            "minSat":          m.get("minSat", 0),
            "maxSat":          m.get("maxSat", 255),
            "midiHue":         m.get("midiHue", 0),
            "midiSaturation":  m.get("midiSaturation", 255),
            "rangeMin":        m.get("rangeMin", 0),
            "rangeMax":        m.get("rangeMax", 127),
            "minDb":           m.get("minDb", -60.0),
            "maxDb":           m.get("maxDb", 0.0),
            "mode":            m.get("mode", 0),
            "distance":        m.get("distance", 100),
            "distanceSwitch":  m.get("distanceSwitch", False),
            "distanceMode":    m.get("distanceMode", 0),
        }, name="GET /setMidiParams (echo current)")
        if body is not None:
            record("GET /setMidiParams (echo current)", "pass")
    else:
        record("GET /setMidiParams", "skip", "no midi params available")

    # ── 9. Darkroom params ────────────────────────────────────────────────────
    header("9. Darkroom parameters")

    dark = get(session, host, "/getDarkroomParams",
               name="GET /getDarkroomParams", timeout=6)
    if dark:
        ctx["dark"] = dark
        record("GET /getDarkroomParams", "pass", f"keys: {list(dark.keys())[:5]}")

    if ctx.get("dark"):
        d = ctx["dark"]
        body = get(session, host, "/setDarkroomParams", params={
            "strobeMin":          d.get("strobeMin", 50),
            "strobeMax":          d.get("strobeMax", 200),
            "redlightMin":        d.get("redlightMin", 10),
            "redlightMax":        d.get("redlightMax", 100),
            "candlelightMin":     d.get("candlelightMin", 10),
            "candlelightMax":     d.get("candlelightMax", 80),
            "redLightEnabled":    d.get("redLightEnabled", True),
            "candleLightEnabled": d.get("candleLightEnabled", True),
        }, name="GET /setDarkroomParams (echo current)")
        if body is not None:
            record("GET /setDarkroomParams (echo current)", "pass")
    else:
        record("GET /setDarkroomParams", "skip", "no darkroom params available")

    # ── 10. Time ──────────────────────────────────────────────────────────────
    header("10. Time")

    now = datetime.datetime.utcnow()
    body = get(session, host, "/setTime", params={
        "year": now.year, "month": now.month, "day": now.day,
        "hours": now.hour, "minutes": now.minute, "seconds": now.second,
    }, name="GET /setTime (current UTC)")
    if body is not None:
        record("GET /setTime (current UTC)", "pass")

    body = get(session, host, "/setSleepTime",
               params={"hours": 23, "minutes": 0, "seconds": 0},
               name="GET /setSleepTime")
    if body is not None:
        record("GET /setSleepTime", "pass")

    body = get(session, host, "/setWakeupTime",
               params={"hours": 8, "minutes": 0, "seconds": 0},
               name="GET /setWakeupTime")
    if body is not None:
        record("GET /setWakeupTime", "pass")

    # ── 11. Logging / test mode toggles ──────────────────────────────────────
    header("11. Toggles")

    body = get(session, host, "/toggleLogging", name="GET /toggleLogging")
    if body is not None:
        record("GET /toggleLogging (on)", "pass")
        # toggle back
        get(session, host, "/toggleLogging", name="_toggleLogging_off")

    body = get(session, host, "/toggleTestMode", name="GET /toggleTestMode")
    if body is not None:
        record("GET /toggleTestMode (on)", "pass")
        get(session, host, "/toggleTestMode", name="_toggleTestMode_off")

    # ── 12. Reannounce ────────────────────────────────────────────────────────
    header("12. Reannounce")

    body = get(session, host, "/reannounce", name="GET /reannounce")
    if body is not None:
        record("GET /reannounce", "pass")
    prompt("Reannounce — physical",
           "Did devices re-announce? (check serial log for address list update)", auto)

    # ── 13. Calibration flow ──────────────────────────────────────────────────
    header("13. Calibration flow (start → cancel)")

    body = get(session, host, "/commandStartCalibration",
               name="GET /commandStartCalibration")
    if body is not None:
        record("GET /commandStartCalibration", "pass")
    prompt("Calibration started — physical",
           "Did devices enter calibration mode (slow white pulse)?", auto)

    time.sleep(1)

    body = get(session, host, "/commandCancelCalibration",
               name="GET /commandCancelCalibration")
    if body is not None:
        record("GET /commandCancelCalibration", "pass")
    prompt("Calibration cancelled — physical",
           "Did devices exit calibration mode?", auto)

    body = get(session, host, "/commandResetCalibration",
               name="GET /commandResetCalibration")
    if body is not None:
        record("GET /commandResetCalibration", "pass")

    # ── 14. Distance calibration flow ─────────────────────────────────────────
    header("14. Distance calibration flow (start → cancel)")

    body = get(session, host, "/commandStartDistanceCalibration",
               name="GET /commandStartDistanceCalibration")
    if body is not None:
        record("GET /commandStartDistanceCalibration", "pass")
    prompt("Distance calibration started — physical",
           "Did devices enter distance calibration mode?", auto)

    time.sleep(1)

    body = get(session, host, "/commandCancelDistanceCalibration",
               name="GET /commandCancelDistanceCalibration")
    if body is not None:
        record("GET /commandCancelDistanceCalibration", "pass")
    prompt("Distance calibration cancelled — physical",
           "Did devices exit distance calibration?", auto)

    # ── 15. Firmware ──────────────────────────────────────────────────────────
    header("15. Firmware")

    r = session.get(host + "/firmware.bin", timeout=5, stream=True)
    if r.status_code == 200:
        size = int(r.headers.get("content-length", 0))
        record("GET /firmware.bin", "pass",
               f"{size // 1024} KB" if size else "no content-length")
    elif r.status_code == 404:
        record("GET /firmware.bin", "skip", "no firmware uploaded yet")
    else:
        record("GET /firmware.bin", "fail", f"HTTP {r.status_code}")

    # ── 16. Auth — user role ──────────────────────────────────────────────────
    header("16. Role-based access (user account)")

    user_session = requests.Session()
    body = post(user_session, host, "/api/login",
                json={"username": "user", "password": "user"},
                name="_user_login")
    if body:
        record("POST /api/login (user)", "pass")
        # user should be allowed on dashboard endpoints
        r2 = user_session.get(host + "/serial-status", timeout=5)
        if r2.status_code == 200:
            record("user role: GET /serial-status allowed", "pass")
        else:
            record("user role: GET /serial-status allowed", "fail", f"HTTP {r2.status_code}")

        # user should be blocked from admin endpoints
        r3 = user_session.get(host + "/compile-stream", timeout=5)
        if r3.status_code in (401, 403):
            record("user role: GET /compile-stream blocked", "pass", f"HTTP {r3.status_code}")
        else:
            record("user role: GET /compile-stream blocked", "fail",
                   f"expected 401/403, got {r3.status_code}")
    else:
        record("POST /api/login (user)", "skip", "no user account configured")

    # ── 17. Logout ────────────────────────────────────────────────────────────
    header("17. Logout")

    body = post(session, host, "/api/logout", name="POST /api/logout")
    if body is not None:
        record("POST /api/logout", "pass")

    r = session.get(host + "/api/me", timeout=5)
    if r.status_code in (401, 403):
        record("Session cleared after logout", "pass")
    else:
        record("Session cleared after logout", "fail",
               f"expected 401, got {r.status_code}")

    # ── Summary ───────────────────────────────────────────────────────────────
    print(f"\n{'═'*60}")
    print(f"{W} Test summary{X}")
    print(f"{'═'*60}")

    passed  = [r for r in results if r[1] == "pass"]
    failed  = [r for r in results if r[1] == "fail"]
    skipped = [r for r in results if r[1] == "skip"]

    if failed:
        print(f"\n{R}{W} FAILED ({len(failed)}):{X}")
        for name, _, detail in failed:
            print(f"  {R}✗{X} {name}  {detail}")

    print(f"\n  {G}Passed:  {len(passed)}{X}")
    print(f"  {R}Failed:  {len(failed)}{X}")
    print(f"  {Y}Skipped: {len(skipped)}{X}")
    print()

    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
