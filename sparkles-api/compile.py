"""
compile.py — PlatformIO compile helpers for Sparkles.

Streams build output line-by-line via an async generator.
"""

import asyncio
import hashlib
import os
import re
import shutil
from pathlib import Path

REPO_DIR     = Path(__file__).parent.parent.resolve()
API_DIR      = Path(__file__).parent.resolve()
PIO_BIN      = Path(os.environ.get("PIO_BIN", str(API_DIR / ".venv" / "bin" / "pio")))
DEFINES_PATH = REPO_DIR / "lib" / "MyDefines" / "src" / "MyDefines.h"
CLIENT_ENV   = "Client_Device"
MASTER_ENV   = "Master_Pi"
CLIENT_BIN   = REPO_DIR / ".pio" / "build" / CLIENT_ENV / "firmware.bin"
MASTER_BIN   = REPO_DIR / ".pio" / "build" / MASTER_ENV / "firmware.bin"
FIRMWARE_OUT = API_DIR / "firmware.bin"
MASTER_HASH_FILE = API_DIR / ".master_src_hash"

_MASTER_SRC_DIRS = ["src/Master-Device", "lib"]
_MASTER_SRC_EXTS = {".cpp", ".h", ".c", ".ini"}


def _hash_master_sources() -> str:
    """SHA-256 of all master source files, stable across runs."""
    h = hashlib.sha256()
    for d in _MASTER_SRC_DIRS:
        base = REPO_DIR / d
        for path in sorted(base.rglob("*")):
            if path.is_file() and path.suffix in _MASTER_SRC_EXTS:
                h.update(path.relative_to(REPO_DIR).as_posix().encode())
                h.update(path.read_bytes())
    return h.hexdigest()


def master_sources_changed() -> bool:
    """True if source files differ from the last successful upload."""
    current = _hash_master_sources()
    if MASTER_HASH_FILE.exists():
        return MASTER_HASH_FILE.read_text().strip() != current
    return True


def record_master_upload():
    """Store current source hash after a successful upload."""
    MASTER_HASH_FILE.write_text(_hash_master_sources())


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
    """Async generator — yields output lines from a subprocess, with keep-alives."""
    proc = await asyncio.create_subprocess_exec(
        *cmd,
        cwd=str(cwd),
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.STDOUT,
    )
    assert proc.stdout
    while True:
        try:
            raw = await asyncio.wait_for(proc.stdout.readline(), timeout=10)
        except asyncio.TimeoutError:
            yield ": keep-alive"
            continue
        if not raw:
            break
        yield raw.decode(errors="replace").rstrip()
    await proc.wait()
    yield f"[exit code {proc.returncode}]"
    if proc.returncode != 0:
        raise RuntimeError(f"Process exited with code {proc.returncode}")


async def git_pull():
    """Pull latest code from remote with a 15s timeout. Skips silently on timeout or network error."""
    try:
        proc = await asyncio.create_subprocess_exec(
            "git", "pull",
            cwd=str(REPO_DIR),
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.STDOUT,
        )
        try:
            stdout, _ = await asyncio.wait_for(proc.communicate(), timeout=15)
            for line in stdout.decode(errors="replace").splitlines():
                yield line
            yield f"[git pull exit code {proc.returncode}]"
        except asyncio.TimeoutError:
            proc.kill()
            yield "[git pull timed out after 15s — continuing with local code]"
    except Exception as exc:
        yield f"[git pull failed: {exc} — continuing with local code]"


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
    """Compile (if sources changed) and flash master firmware via USB, then hard-reset.

    The caller (main.py compile_stream) frees the port with bridge.release_port()
    before calling this and bridge.resume_port() after, so esptool can claim it —
    no service stop/start needed now that serial_bridge owns the port directly.
    """
    if not master_sources_changed() and MASTER_BIN.exists():
        yield "[sources unchanged — skipping compile, uploading existing binary]"
        async for line in _stream_process(
            [str(PIO_BIN), "run", "-e", MASTER_ENV, "--target", "upload",
             "--disable-auto-clean", "-j", "1"],
            cwd=REPO_DIR,
        ):
            yield line
    else:
        async for line in _stream_process(
            [str(PIO_BIN), "run", "-e", MASTER_ENV, "--target", "upload", "-j", "1"],
            cwd=REPO_DIR,
        ):
            yield line
        record_master_upload()

    await asyncio.get_event_loop().run_in_executor(None, _dtr_reset)
    yield "[DTR reset sent — master rebooting]"
