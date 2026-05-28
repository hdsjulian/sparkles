#!/usr/bin/env python3
"""
Run on the Raspberry Pi to pull latest code, rebuild the frontend, and restart the service.
    python3 deploy.py
"""

import subprocess
import os

REPO_DIR = os.path.expanduser("~/sparkles")
UI_DIR   = os.path.join(REPO_DIR, "sparkles-ui")


SUDO_PASS = "raspi"


def run(cmd, cwd=None):
    print(f"\n$ {cmd}")
    subprocess.run(cmd, shell=True, cwd=cwd, check=True)


def sudo(cmd, cwd=None):
    print(f"\n$ sudo {cmd}")
    subprocess.run(f"echo {SUDO_PASS} | sudo -S {cmd}", shell=True, cwd=cwd, check=True)


run("git update-index --skip-worktree sparkles-api/auth_config.yaml", cwd=REPO_DIR)
run("git pull origin feature/serial-bridge", cwd=REPO_DIR)
run("npm run build", cwd=UI_DIR)
sudo("systemctl restart sparkles")

print("\nDone. Service restarted.")
