#!/bin/bash
# Create virtualenv and install Python dependencies
set -e

echo "=== [2/5] Python Dependencies ==="

VENV=/home/julian/myenv
REPO=/home/julian/sparkles

# Create venv if it doesn't exist
if [ ! -d "$VENV" ]; then
    python3 -m venv "$VENV"
    echo "Created virtualenv at $VENV"
fi

source "$VENV/bin/activate"

pip install --upgrade pip

# API + services dependencies
pip install -r "$REPO/sparkles-api/requirements.txt"

# Pitch detector dependencies
pip install -r "$REPO/pitchdetector/requirements.txt"

echo "=== Python dependencies installed ==="
