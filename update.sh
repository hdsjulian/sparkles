#!/bin/bash
# Pull latest code and restart all Sparkles services.
# Run on the Pi as julian (NOT sudo): bash ~/sparkles/update.sh
# git/npm stay julian-owned; only the service restart uses sudo.

set -e

REPO=/home/julian/sparkles
BRANCH=$(git -C "$REPO" rev-parse --abbrev-ref HEAD)
BUNDLE=/tmp/sparkles-update.bundle

# Online if we can reach the git remote within a few seconds. GIT_TERMINAL_PROMPT=0
# so an unreachable remote fails fast instead of blocking on a credential prompt.
if GIT_TERMINAL_PROMPT=0 timeout 6 git -C "$REPO" ls-remote origin HEAD &>/dev/null; then
    ONLINE=1
else
    ONLINE=0
fi

echo "=== Pulling $BRANCH ==="
if [ "$ONLINE" = "1" ]; then
    git -C "$REPO" pull
elif [ -f "$BUNDLE" ]; then
    echo "No internet — pulling from bundle $BUNDLE (push_to_pi.sh)"
    git -C "$REPO" pull "$BUNDLE" "$BRANCH"
else
    echo "No internet and no bundle at $BUNDLE — building/restarting with the code already on disk"
fi

echo "=== Rebuilding frontend ==="
cd "$REPO/sparkles-ui"
# npm install needs the network; skip it offline (node_modules is already present)
if [ "$ONLINE" = "1" ]; then
    /usr/bin/npm install --legacy-peer-deps --silent
fi
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
