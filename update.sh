#!/bin/bash
# Pull latest code and restart all Sparkles services.
# Run on the Pi: bash ~/sparkles/update.sh

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
sudo systemctl restart serial_mux sparkles aubio keyboard_midi

echo "=== Done ==="
for svc in serial_mux sparkles aubio keyboard_midi; do
    status=$(systemctl is-active "$svc" 2>/dev/null || echo "unknown")
    echo "  $svc: $status"
done
