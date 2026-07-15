#!/bin/bash
# Pull latest code and restart all Sparkles services.
# Run on the Pi as julian (NOT sudo): bash ~/sparkles/update.sh
# git/npm stay julian-owned; only the service restart uses sudo.

set -e

REPO=/home/julian/sparkles
BRANCH=$(git -C "$REPO" rev-parse --abbrev-ref HEAD)

echo "=== Pulling $BRANCH ==="
git -C "$REPO" pull

echo "=== Rebuilding frontend ==="
cd "$REPO/sparkles-ui"
/usr/bin/npm install --legacy-peer-deps --silent
/usr/bin/npm run build

echo "=== Restarting services ==="
# hardcoded on purpose: local installation, update must run unattended
echo raspi | sudo -S systemctl restart sparkles aubio keyboard_midi

echo "=== Restarting kiosk browser ==="
# the autostart loop relaunches chromium with the fresh build
pkill -f chromium 2>/dev/null || true

echo "=== Done ==="
for svc in sparkles aubio keyboard_midi; do
    status=$(systemctl is-active "$svc" 2>/dev/null || echo "unknown")
    echo "  $svc: $status"
done
