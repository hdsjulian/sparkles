#!/bin/bash
# Quick one-shot: join a WiFi network right now (e.g. at the workshop) so
# update.sh can reach the internet to git pull. The Pi talks to the master over
# USB serial and the lamps are their own ESP-NOW mesh, so the Pi's WiFi has zero
# effect on the installation — this only gets the Pi online.
#
# SAFE OVER THE HOTSPOT: if you're SSH'd in via the 'Sparkles' AP, switching
# networks drops your shell (one radio, can't be AP + client at once). So the
# actual switch runs DETACHED via systemd-run — losing SSH can't interrupt it —
# and if the join fails it brings the hotspot back so you're never locked out.
#
# Does NOT disturb the field WiFi/AP fallback (setup/06_wifi.sh): the profile is
# created with autoconnect OFF, so a reboot in the field behaves as before.
#
# usage: bash wifi_connect.sh "<SSID>" "<password>"
#        bash wifi_connect.sh "<SSID>"            # prompts for the password

set -e
IFACE=wlan0

# ── detached worker (re-entry): does the actual switch, no controlling TTY ──
if [ "$1" = "--run" ]; then
    SSID="$2"; PSK="$3"
    nmcli connection down sparkles-ap 2>/dev/null || true
    sleep 2
    nmcli device wifi rescan 2>/dev/null || true
    sleep 5
    if nmcli device wifi connect "$SSID" password "$PSK" ifname "$IFACE"; then
        nmcli connection modify "$SSID" connection.autoconnect no 2>/dev/null || true
        echo "Connected to $SSID; Pi IP $(hostname -I | awk '{print $1}')"
    else
        echo "Failed to join $SSID — restoring the Sparkles hotspot"
        nmcli connection up sparkles-ap 2>/dev/null || true
    fi
    exit 0
fi

# ── user-facing invocation ──
SSID="${1:?usage: bash wifi_connect.sh \"<SSID>\" [password]}"
PSK="${2:-}"

# prime sudo unattended (local appliance, sudo password is 'raspi')
echo raspi | sudo -S true 2>/dev/null || true

if [ -z "$PSK" ]; then
    read -rsp "Password for $SSID: " PSK
    echo
fi

# launch the switch detached so a dropped SSH session can't kill it mid-connect
sudo systemd-run --collect --unit=sparkles-wifiswitch bash "$(readlink -f "$0")" --run "$SSID" "$PSK"

cat <<EOF
Switching to "$SSID" in the background.
If you're on the 'Sparkles' hotspot your SSH will drop now — that's expected.
Wait ~30s, then:
  worked  -> join "$SSID" on your laptop, then: ssh julian@sparkles.local
  failed  -> the Pi is back on the 'Sparkles' hotspot; reconnect to it
After reconnecting, see what happened:  journalctl -u sparkles-wifiswitch --no-pager
Once online:  bash ~/sparkles/update.sh
EOF
