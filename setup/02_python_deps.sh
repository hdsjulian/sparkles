#!/bin/bash
# Create virtualenv and install Python dependencies
set -e

echo "=== [2/5] Python Dependencies ==="

VENV=/home/julian/myenv
REPO=/home/julian/sparkles
PIP="$VENV/bin/pip"

# Create venv if it doesn't exist
if [ ! -d "$VENV" ]; then
    python3 -m venv "$VENV" --system-site-packages
    echo "Created virtualenv at $VENV"
fi

"$PIP" install --no-user --upgrade pip

# API + services dependencies
"$PIP" install --no-user -r "$REPO/sparkles-api/requirements.txt"

# Pitch detector dependencies
"$PIP" install --no-user -r "$REPO/pitchdetector/requirements.txt"

# PlatformIO (for flashing master ESP32 from the Pi)
if ! "$VENV/bin/pio" --version &>/dev/null; then
    echo ">> Installing PlatformIO"
    "$PIP" install --no-user platformio
    "$VENV/bin/pio" platform install espressif32
else
    echo ">> PlatformIO already installed"
fi

echo "=== Python dependencies installed ==="
