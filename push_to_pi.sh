#!/bin/bash
# Run ON THE MAC. Bundles the current branch, copies it to the Pi over the local
# link (no internet needed either side), then runs update.sh on the Pi — which
# detects there's no internet and pulls from the bundle instead of GitHub.
#
# Use this field-side when the Pi has no internet. When the Pi IS online, just
# run `bash ~/sparkles/update.sh` on it directly.
#
# usage: bash push_to_pi.sh [pi-host]
#   pi-host defaults to julian@sparkles.local (or $PI_HOST); on the Sparkles
#   hotspot use julian@10.42.0.1

set -e

PI="${1:-${PI_HOST:-julian@sparkles.local}}"
REPO="$(cd "$(dirname "$0")" && pwd)"
BRANCH="$(git -C "$REPO" rev-parse --abbrev-ref HEAD)"
BUNDLE=/tmp/sparkles-update.bundle

echo "=== Bundling $BRANCH ==="
git -C "$REPO" bundle create "$BUNDLE" "$BRANCH"

echo "=== Copying bundle to $PI ==="
scp "$BUNDLE" "$PI:/tmp/sparkles-update.bundle"

echo "=== Running update.sh on $PI (offline mode) ==="
# -t so sudo/npm output streams back live
ssh -t "$PI" 'bash ~/sparkles/update.sh'

echo "=== Done. Master firmware (if changed) still needs a separate flash:"
echo "    ssh $PI '/home/julian/myenv/bin/pio run -e Master_Pi -t upload'"