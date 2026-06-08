#!/usr/bin/env python3
"""
Run on the Raspberry Pi to pull latest code, rebuild the frontend, and restart the service.
    python3 deploy.py
"""

import subprocess
import os

REPO_DIR = os.path.expanduser("~/sparkles")
UI_DIR   = os.path.join(REPO_DIR, "sparkles-ui")


SUDO_PASS = "julian"


def run(cmd, cwd=None):
    print(f"\n$ {cmd}")
    subprocess.run(cmd, shell=True, cwd=cwd, check=True)


def sudo(cmd, cwd=None):
    print(f"\n$ sudo {cmd}")
    subprocess.run(f"echo {SUDO_PASS} | sudo -S {cmd}", shell=True, cwd=cwd, check=True)


run("git update-index --skip-worktree sparkles-api/auth_config.yaml", cwd=REPO_DIR)

print("\n$ git pull origin feature/serial-bridge")
result = subprocess.run(
    "git pull origin feature/serial-bridge",
    shell=True, cwd=REPO_DIR, check=True, capture_output=True, text=True
)
print(result.stdout)

ui_changed = any("sparkles-ui/" in line for line in result.stdout.splitlines())

if ui_changed:
    print("[sparkles-ui files changed — rebuilding frontend]")
    run("npm run build", cwd=UI_DIR)
else:
    print("[no sparkles-ui changes — skipping frontend build]")

sudo("systemctl restart sparkles")

print("\nDone. Service restarted.")
