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

# auth.py rewrites auth_config.yaml at startup (password_plain -> password_hash,
# and yaml.dump reformats the rest), so the pi's copy always differs from git and
# any commit touching that file aborts the pull. Set it aside, take the incoming
# version, then put the live users back — access rules come from git, credentials
# stay on the pi.
AUTH="$REPO/sparkles-api/auth_config.yaml"
AUTH_BACKUP="$REPO/sparkles-api/auth_config.local.yaml"
if [ -f "$AUTH" ] && ! git -C "$REPO" diff --quiet -- sparkles-api/auth_config.yaml; then
    echo "=== Setting aside local auth_config.yaml ==="
    cp "$AUTH" "$AUTH_BACKUP"
    git -C "$REPO" checkout -- sparkles-api/auth_config.yaml
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

if [ -f "$AUTH_BACKUP" ]; then
    echo "=== Restoring local users into auth_config.yaml ==="
    python3 - "$AUTH" "$AUTH_BACKUP" <<'PY'
import sys, yaml
merged_path, local_path = sys.argv[1], sys.argv[2]
with open(merged_path) as f:
    merged = yaml.safe_load(f) or {}
with open(local_path) as f:
    local = yaml.safe_load(f) or {}
if local.get("users"):
    merged["users"] = local["users"]          # live hashes win, rules come from git
    with open(merged_path, "w") as f:
        yaml.dump(merged, f, allow_unicode=True, sort_keys=False)
    print("kept %d local user(s)" % len(local["users"]))
else:
    print("no users in the local copy, leaving the pulled file alone")
PY
    # retire it: the merged file now holds the live users, and a backup left in
    # place would re-apply these same users over every future change
    mv "$AUTH_BACKUP" "$AUTH_BACKUP.applied"
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
