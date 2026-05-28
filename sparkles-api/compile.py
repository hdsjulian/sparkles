"""
compile.py — PlatformIO compile helpers for Sparkles.

Streams build output line-by-line via an async generator.
"""

import asyncio
import os
import re
import shutil
from pathlib import Path

REPO_DIR     = Path(__file__).parent.parent.resolve()
API_DIR      = Path(__file__).parent.resolve()
PIO_BIN      = API_DIR / ".venv" / "bin" / "pio"
DEFINES_PATH = REPO_DIR / "lib" / "MyDefines" / "src" / "MyDefines.h"
CLIENT_ENV   = "Client_Device"
MASTER_ENV   = "Master_WebserverTest_Pi"
CLIENT_BIN   = REPO_DIR / ".pio" / "build" / CLIENT_ENV / "firmware.bin"
FIRMWARE_OUT = API_DIR / "firmware.bin"


def read_version() -> str:
    text = DEFINES_PATH.read_text()
    m = re.search(r'#define VERSION "(\d+\.\d+\.\d+)"', text)
    if not m:
        raise RuntimeError("VERSION not found in MyDefines.h")
    return m.group(1)


def increment_version() -> tuple[str, str]:
    """Bump patch number in MyDefines.h. Returns (old, new)."""
    text = DEFINES_PATH.read_text()
    m = re.search(r'#define VERSION "(\d+)\.(\d+)\.(\d+)"', text)
    if not m:
        raise RuntimeError("VERSION not found in MyDefines.h")
    major, minor, patch = m.group(1), m.group(2), int(m.group(3))
    old = f"{major}.{minor}.{patch}"
    new = f"{major}.{minor}.{patch + 1}"
    updated = re.sub(
        r'#define VERSION "\d+\.\d+\.\d+"',
        f'#define VERSION "{new}"',
        text,
    )
    DEFINES_PATH.write_text(updated)
    return old, new


async def _stream_process(cmd: list[str], cwd: Path):
    """Async generator — yields output lines from a subprocess."""
    proc = await asyncio.create_subprocess_exec(
        *cmd,
        cwd=str(cwd),
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.STDOUT,
    )
    assert proc.stdout
    async for raw in proc.stdout:
        yield raw.decode(errors="replace").rstrip()
    await proc.wait()
    yield f"[exit code {proc.returncode}]"
    if proc.returncode != 0:
        raise RuntimeError(f"Process exited with code {proc.returncode}")


async def compile_client():
    """Compile client firmware and copy binary to firmware.bin for OTA serving."""
    async for line in _stream_process(
        [str(PIO_BIN), "run", "-e", CLIENT_ENV, "-j", "1"],
        cwd=REPO_DIR,
    ):
        yield line

    if CLIENT_BIN.exists():
        shutil.copy2(CLIENT_BIN, FIRMWARE_OUT)
        yield f"[firmware copied to {FIRMWARE_OUT.name} — {FIRMWARE_OUT.stat().st_size // 1024} KB]"
    else:
        raise RuntimeError("Client binary not found after build")


def _dtr_reset(port: str = "/dev/sparkles"):
    """Toggle DTR to hard-reset the ESP32 after flashing."""
    try:
        import serial as _serial, time as _time
        s = _serial.Serial(port, 115200, timeout=1)
        s.dtr = False
        _time.sleep(0.1)
        s.dtr = True
        s.close()
    except Exception as exc:
        raise RuntimeError(f"DTR reset failed: {exc}")


async def compile_master():
    """Compile and flash master firmware via USB, then hard-reset."""
    async for line in _stream_process(
        [str(PIO_BIN), "run", "-e", MASTER_ENV, "--target", "upload", "-j", "1"],
        cwd=REPO_DIR,
    ):
        yield line
    await asyncio.get_event_loop().run_in_executor(None, _dtr_reset)
    yield "[DTR reset sent — master rebooting]"
