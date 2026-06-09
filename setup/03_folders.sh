#!/bin/bash
# Create required directories
set -e

echo "=== [3/5] Creating Folders ==="

BASE=/home/julian/sparkles

mkdir -p "$BASE/songs"
mkdir -p "$BASE/logs"

# Ensure correct ownership
chown -R julian:julian "$BASE/songs"
chown -R julian:julian "$BASE/logs"

echo "Created:"
echo "  $BASE/songs   — drop .mid files here"
echo "  $BASE/logs    — serial.log and other logs"

# udev rule for stable ESP32 device name (303a:1001 → /dev/sparkles)
echo 'SUBSYSTEM=="tty", ATTRS{idVendor}=="303a", ATTRS{idProduct}=="1001", SYMLINK+="sparkles"' \
    > /etc/udev/rules.d/99-sparkles.rules
udevadm control --reload-rules
udevadm trigger
echo "  udev rule installed → /dev/sparkles"

echo "=== Folders created ==="
