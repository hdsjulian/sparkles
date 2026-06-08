#!/bin/bash
# Sparkles Pi 5 — full setup script
# Run once on a fresh Raspberry Pi OS installation.
# Usage: sudo bash setup.sh
set -e

REPO=/home/julian/sparkles
SETUP_DIR="$REPO/setup"

echo "========================================"
echo "  Sparkles Pi 5 Setup"
echo "========================================"
echo ""

if [ "$EUID" -ne 0 ]; then
    echo "Please run as root: sudo bash setup.sh"
    exit 1
fi

bash "$SETUP_DIR/01_system_deps.sh"
echo ""
bash "$SETUP_DIR/02_python_deps.sh"
echo ""
bash "$SETUP_DIR/03_folders.sh"
echo ""
bash "$SETUP_DIR/04_services.sh"
echo ""
bash "$SETUP_DIR/05_kiosk.sh"
echo ""

echo "========================================"
echo "  Setup complete!"
echo ""
echo "  Next steps:"
echo "  - Add MIDI songs to /home/julian/sparkles/songs/"
echo "  - Reboot to start kiosk: sudo reboot"
echo "========================================"
