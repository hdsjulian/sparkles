#!/bin/bash
# Install system-level dependencies
set -e

echo "=== [1/5] System Dependencies ==="

sudo apt update
sudo apt install -y \
    python3-pip \
    python3-venv \
    python3-dev \
    python3-aubio \
    python3-numpy \
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
    curl \
    ca-certificates \
    gnupg \
    avahi-daemon \
    avahi-utils

# Node.js 20 via NodeSource
if ! command -v node &>/dev/null || [ "$(node -e 'process.stdout.write(process.version.slice(1).split(".")[0])')" -lt 20 ]; then
    echo ">> Installing Node.js 20"
    curl -fsSL https://deb.nodesource.com/setup_20.x | sudo -E bash -
    sudo apt install -y nodejs
else
    echo ">> Node.js $(node --version) already installed"
fi

# Fix locale — use system default if en_US not available
if locale -a 2>/dev/null | grep -q "en_US.utf8"; then
    update-locale LC_ALL=en_US.UTF-8 LANG=en_US.UTF-8
else
    SYSTEM_LOCALE=$(locale -a 2>/dev/null | grep -i "utf8" | head -1 | sed 's/utf8/UTF-8/')
    update-locale LC_ALL="$SYSTEM_LOCALE" LANG="$SYSTEM_LOCALE"
fi

echo "=== System dependencies installed ==="
