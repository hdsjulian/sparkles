#!/bin/bash
# Run ON THE MAC. Bundles the current branch, copies it to the Pi over the local
# link (no internet needed either side), then runs update.sh on the Pi — which
# detects there's no internet and pulls from the bundle instead of GitHub.
#
# --firmware also builds the client and master here, flashes the master on the
# Pi from that build, and then has the Pi update every board over its own wifi,
# one by one and never more than five away at once. Ctrl-C during that stops
# the asking; boards already away finish on their own.
#
# Use this field-side when the Pi has no internet. When the Pi IS online, just
# run `bash ~/sparkles/update.sh` on it directly.
#
# usage: bash push_to_pi.sh [--firmware] [pi-host]
#   pi-host defaults to julian@sparkles.local (or $PI_HOST); on the Sparkles
#   hotspot use julian@10.42.0.1

set -e

FIRMWARE=0
if [ "$1" = "--firmware" ]; then
    FIRMWARE=1
    shift
fi

PI="${1:-${PI_HOST:-julian@sparkles.local}}"
# Hardcoded on purpose, same as the sudo password in update.sh: local
# installation, and being prompted twice a deploy is the actual cost here.
# Override with PI_PASS. `ssh-copy-id "$PI"` once would retire this entirely.
PI_PASS="${PI_PASS:-raspi}"
# the web ui's admin, which starts the board updates; override with ADMIN_PASS
ADMIN_PASS="${ADMIN_PASS:-admin}"

if command -v sshpass >/dev/null 2>&1; then
    SSH=(sshpass -p "$PI_PASS" ssh)
    SCP=(sshpass -p "$PI_PASS" scp)
else
    echo "note: sshpass not installed — you will be prompted for the password"
    echo "      brew install hudochenkov/sshpass/sshpass   (or run ssh-copy-id $PI once)"
    SSH=(ssh)
    SCP=(scp)
fi
REPO="$(cd "$(dirname "$0")" && pwd)"
BRANCH="$(git -C "$REPO" rev-parse --abbrev-ref HEAD)"
BUNDLE=/tmp/sparkles-update.bundle
FW=/tmp/sparkles-firmware

if [ "$FIRMWARE" = "1" ]; then
    if [ -n "$(git -C "$REPO" status --porcelain -- src lib platformio.ini)" ]; then
        echo "note: src/, lib/ or platformio.ini have uncommitted changes. They go into the"
        echo "      firmware, but the Pi's own code only gets what is committed."
    fi
    echo "=== Building client and master firmware ==="
    pio run -d "$REPO" -e Client_Device -e Master_Pi
    rm -rf "$FW"
    mkdir -p "$FW/master"
    cp "$REPO/.pio/build/Client_Device/firmware.bin" "$FW/client.bin"
    cp "$REPO/.pio/build/Master_Pi/"{bootloader,partitions,firmware}.bin "$FW/master/"
    cp ~/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin "$FW/master/"
fi

echo "=== Bundling $BRANCH ==="
git -C "$REPO" bundle create "$BUNDLE" "$BRANCH"

echo "=== Copying bundle to $PI ==="
"${SCP[@]}" "$BUNDLE" "$PI:/tmp/sparkles-update.bundle"
if [ "$FIRMWARE" = "1" ]; then
    "${SCP[@]}" -r "$FW" "$PI:/tmp/"
fi

FLASH=""
FLASHED=""
if [ "$FIRMWARE" = "1" ]; then
    # The client binary goes where the API serves it from. The bridge holds the
    # master's port, so the service stops for the flash and update.sh starts it
    # again, flashed or not; only then does a failed flash end this script,
    # before any board has been asked. esptool is called directly with what
    # `pio run -v -e Master_Pi -t upload` uses: pio can't flash a binary it did
    # not build, with -t nobuild this platform leaves the offsets out.
    FLASH="cp $FW/client.bin sparkles-api/firmware.bin && \
      echo '$PI_PASS' | sudo -S systemctl stop sparkles && \
      flashed=0 && \
      if (cd $FW/master && /home/julian/myenv/bin/python3 ~/.platformio/packages/tool-esptoolpy/esptool.py \
            --chip esp32s3 --port /dev/sparkles --baud 921600 --before default_reset --after hard_reset \
            write_flash -z --flash_mode dio --flash_freq 80m --flash_size detect \
            0x0000 bootloader.bin 0x8000 partitions.bin 0xe000 boot_app0.bin 0x10000 firmware.bin) && \
         (cd sparkles-api && /home/julian/myenv/bin/python3 -c 'import compile; compile._dtr_reset()'); \
      then flashed=1; else echo '=== MASTER FLASH FAILED, no board will be touched ==='; fi && "
    FLASHED=' && [ $flashed = 1 ]'
fi

# Pull the bundle into the Pi's repo FIRST, from here — this updates update.sh
# itself to the offline-capable version before we run it (bootstrap: the Pi's
# on-disk update.sh may predate the offline logic). Then run it; its own bundle
# pull is a harmless already-up-to-date no-op. -t streams sudo/npm output back.
echo "=== Pulling bundle + running update.sh on $PI ==="
# auth_config.yaml is rewritten by auth.py at startup, so the pi's copy is always
# dirty and this pull would abort on it. Set it aside first; update.sh (the fresh
# one this pull installs) merges the live users back out of the copy.
"${SSH[@]}" -t "$PI" "set -e; cd ~/sparkles && \
  if [ -f sparkles-api/auth_config.yaml ] && ! git diff --quiet -- sparkles-api/auth_config.yaml; then \
    cp sparkles-api/auth_config.yaml sparkles-api/auth_config.local.yaml && \
    git checkout -- sparkles-api/auth_config.yaml; \
  fi && \
  git pull /tmp/sparkles-update.bundle '$BRANCH' && $FLASH bash ~/sparkles/update.sh$FLASHED"

if [ "$FIRMWARE" != "1" ]; then
    echo "=== Done. Firmware untouched; push_to_pi.sh --firmware flashes the master and updates the boards"
    exit 0
fi

# The API restarted and the master rebooted, so wait for both: logged in, the
# port open and a first frame from the master, which means its loop is running.
# Straight to the Pi over http, the laptop is on its wifi anyway.
HOST="${PI#*@}"
JAR="$(mktemp)"
echo "=== Waiting for the Pi's API and the master ==="
for i in $(seq 60); do
    code="$(curl -s -o /dev/null -w '%{http_code}' -c "$JAR" -H 'Content-Type: application/json' \
        -d "{\"username\":\"admin\",\"password\":\"$ADMIN_PASS\"}" "http://$HOST/api/login" || true)"
    if [ "$code" = "401" ]; then
        echo "The Pi turned down admin's password, set ADMIN_PASS. No board has been touched."
        exit 1
    fi
    if [ "$code" = "200" ]; then
        status="$(curl -s -b "$JAR" "http://$HOST/serial-status" || true)"
        if echo "$status" | grep -Eq '"connected": ?true' && echo "$status" | grep -Eq '"lastFrameAge": ?[0-9]'; then
            break
        fi
    fi
    if [ "$i" = "60" ]; then
        echo "The master did not come back within two minutes. No board has been touched."
        exit 1
    fi
    sleep 2
done

echo "=== Updating the boards ==="
curl -sN -b "$JAR" "http://$HOST/otaRollout" | python3 -u -c '
import json, sys
for line in sys.stdin:
    if line.startswith("data: "):
        print(json.loads(line[6:]).get("msg", line[6:].strip()))
    elif line.strip() and not line.startswith("event: "):
        print(line.strip())   # an error answer instead of the stream
'
rm -f "$JAR"
