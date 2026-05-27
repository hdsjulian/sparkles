#!/usr/bin/env python3
"""
Sparkles Pi setup script.
Run this on the Raspberry Pi:  python3 setup_pi.py
"""

import subprocess
import sys
import os

REPO_URL    = "https://github.com/hdsjulian/sparkles"
REPO_BRANCH = "feature/serial-bridge"
REPO_DIR    = os.path.expanduser("~/sparkles")
API_DIR    = os.path.join(REPO_DIR, "sparkles-api")
UI_DIR     = os.path.join(REPO_DIR, "sparkles-ui")
VENV_DIR   = os.path.join(API_DIR, ".venv")
SERIAL_PORT = "/dev/sparkles"
PORT        = 80


def run(cmd, cwd=None, check=True):
    print(f"\n$ {cmd}")
    result = subprocess.run(cmd, shell=True, cwd=cwd, check=check)
    return result


def step(msg):
    print(f"\n{'='*60}\n  {msg}\n{'='*60}")


# ── 1. System packages ────────────────────────────────────────────
step("Installing system packages")
run("sudo apt update -y")
run("sudo apt install -y python3-pip python3-venv git curl")

# Ensure Node.js >= 18 (apt ships old versions on Raspberry Pi OS)
step("Installing Node.js 20 via NodeSource")
run("curl -fsSL https://deb.nodesource.com/setup_20.x | sudo -E bash -")
run("sudo apt install -y nodejs")


# ── 2. Clone / update repo ────────────────────────────────────────
step("Cloning repository")
if os.path.isdir(REPO_DIR):
    print(f"{REPO_DIR} already exists — pulling latest")
    run(f"git fetch origin", cwd=REPO_DIR)
    run(f"git checkout {REPO_BRANCH}", cwd=REPO_DIR)
    run(f"git pull origin {REPO_BRANCH}", cwd=REPO_DIR)
else:
    run(f"git clone --branch {REPO_BRANCH} {REPO_URL} {REPO_DIR}")


# ── 3. Python venv + FastAPI deps ─────────────────────────────────
step("Setting up Python virtualenv")
run(f"python3 -m venv {VENV_DIR}")
pip = os.path.join(VENV_DIR, "bin", "pip")
run(f"{pip} install --upgrade pip")
run(f"{pip} install fastapi 'uvicorn[standard]' pyserial")


# ── 3b. PlatformIO CLI (for flashing firmware from the Pi) ────────
step("Installing PlatformIO CLI")
run(f"{pip} install platformio")
pio = os.path.join(VENV_DIR, "bin", "pio")
run(f"{pio} platform install espressif32")


# ── 4. Build SvelteKit frontend ───────────────────────────────────
step("Building SvelteKit frontend")
run(f"rm -rf {os.path.join(UI_DIR, 'node_modules')}")
run("npm install --legacy-peer-deps", cwd=UI_DIR)
run("npx svelte-kit sync", cwd=UI_DIR)
run("npm run build", cwd=UI_DIR)



# ── 6. udev rule for stable ESP32 device name ────────────────────
step("Installing udev rule for ESP32 (303a:1001 → /dev/sparkles)")
udev_rule = 'SUBSYSTEM=="tty", ATTRS{idVendor}=="303a", ATTRS{idProduct}=="1001", SYMLINK+="sparkles"\n'
udev_path = "/tmp/99-sparkles.rules"
with open(udev_path, "w") as f:
    f.write(udev_rule)
run(f"sudo cp {udev_path} /etc/udev/rules.d/99-sparkles.rules")
run("sudo udevadm control --reload-rules")
run("sudo udevadm trigger")


# ── 7. Write systemd service ──────────────────────────────────────
step("Installing systemd service")
uvicorn = os.path.join(VENV_DIR, "bin", "uvicorn")
service = f"""[Unit]
Description=Sparkles API + Frontend
After=network.target

[Service]
ExecStart={uvicorn} main:app --host 0.0.0.0 --port {PORT}
WorkingDirectory={API_DIR}
Environment=SPARKLES_PORT={SERIAL_PORT}
# Optional: set SPARKLES_TBEAM_PORT=/dev/ttyUSB0 to forward events to a T-Beam via Meshtastic SerialModule
Restart=always
RestartSec=5
User={os.environ.get('USER', 'pi')}
AmbientCapabilities=CAP_NET_BIND_SERVICE

[Install]
WantedBy=multi-user.target
"""
service_path = "/tmp/sparkles.service"
with open(service_path, "w") as f:
    f.write(service)

run(f"sudo cp {service_path} /etc/systemd/system/sparkles.service")
run("sudo systemctl daemon-reload")
run("sudo systemctl enable sparkles")
run("sudo systemctl restart sparkles")


# ── 8. Done ───────────────────────────────────────────────────────
step("Done")
print(f"""
Sparkles is running at http://localhost:{PORT}

Useful commands:
  sudo systemctl status sparkles    # check status
  sudo journalctl -fu sparkles      # follow logs
  sudo systemctl restart sparkles   # restart
""")
