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

# Fix locale
locale-gen en_US.UTF-8
update-locale LC_ALL=en_US.UTF-8 LANG=en_US.UTF-8

echo "=== System dependencies installed ==="
