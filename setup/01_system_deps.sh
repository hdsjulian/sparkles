#!/bin/bash
# Install system-level dependencies
set -e

echo "=== [1/5] System Dependencies ==="

sudo apt update
sudo apt install -y \
    python3-pip \
    python3-venv \
    python3-dev \
    portaudio19-dev \
    libasound2-dev \
    librtmidi-dev \
    nginx \
    chromium \
    openbox \
    xorg \
    xinit \
    unclutter \
    i2c-tools \
    git \
    curl

echo "=== System dependencies installed ==="
