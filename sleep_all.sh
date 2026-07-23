#!/bin/bash
# Run ON THE PI. Worst-case, no-web, no-auth "put every device to sleep NOW".
# Writes the command straight into serial_bridge's music socket, which forwards
# it verbatim to the master over serial — so it works even if the web UI / auth /
# FastAPI routing is broken, as long as the sparkles service is running and the
# master is connected. No resync, no wake time: the master broadcasts sleep and
# holds it until you wake them (Settings -> Cancel / Wake Up Now, or a reboot).
#
# usage: bash sleep_all.sh          # sleep everyone now
#        bash sleep_all.sh wake     # wake everyone now (cancel the hold)

SOCK="${SPARKLES_MUSIC_SOCK:-/tmp/music.sock}"
CMD='{"cmd":"sleep_now"}'
[ "$1" = "wake" ] && CMD='{"cmd":"wake_now"}'

if [ ! -S "$SOCK" ]; then
    echo "music socket $SOCK not found — is the sparkles service running?"
    exit 1
fi

python3 - "$SOCK" "$CMD" <<'PY'
import socket, sys
sock, cmd = sys.argv[1], sys.argv[2]
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.settimeout(3)
s.connect(sock)
s.sendall((cmd + "\n").encode())
s.close()
print("sent:", cmd)
PY

echo "Done. If nothing happens, the master isn't connected — check: systemctl is-active sparkles ; ls -l /dev/sparkles"
