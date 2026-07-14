#!/usr/bin/env bash
# Periodically send an "alive" beacon from the T-Beam on the sparkles channel,
# so you can watch reach/SNR on the T-Deck. Ctrl-C to stop.
#
# usage: ./mesh_heartbeat.sh [port] [interval_seconds]
#   port     defaults to the T-Beam, or $MESH_PORT
#   interval defaults to 60s, or $MESH_INTERVAL

set -u

PORT="${1:-${MESH_PORT:-/dev/cu.wchusbserial5B212289791}}"
INTERVAL="${2:-${MESH_INTERVAL:-60}}"
CHANNAME="sparkles"

command -v meshtastic >/dev/null || { echo "meshtastic CLI not found (try: export PATH=\"\$HOME/.local/bin:\$PATH\")"; exit 1; }
[ -e "$PORT" ] || { echo "port not present: $PORT"; exit 1; }

# find the index of the sparkles channel once, up front
echo "looking up '$CHANNAME' channel on $PORT ..."
CHIDX=$(meshtastic --port "$PORT" --info 2>/dev/null \
  | grep -iE "^ *Index [0-9]+:" \
  | grep -i "\"name\": \"$CHANNAME\"" \
  | grep -oE "Index [0-9]+" | grep -oE "[0-9]+" | head -1)

if [ -z "${CHIDX:-}" ]; then
  echo "couldn't find a channel named '$CHANNAME' on $PORT — check it exists."
  exit 1
fi
echo "sending on channel index $CHIDX every ${INTERVAL}s. Ctrl-C to stop."

n=0
while true; do
  n=$((n + 1))
  ts=$(date "+%H:%M:%S")
  msg="🟢 alive #$n $ts"
  if meshtastic --port "$PORT" --sendtext "$msg" --ch-index "$CHIDX" >/dev/null 2>&1; then
    echo "$ts  sent: $msg"
  else
    echo "$ts  send failed (port busy/unplugged?) — retrying next tick"
  fi
  sleep "$INTERVAL"
done
