#!/bin/bash
# Quick one-shot: join a WiFi network right now (e.g. at the workshop) so
# update.sh can reach the internet to git pull. The Pi talks to the master over
# USB serial and the lamps are their own ESP-NOW mesh, so the Pi's WiFi has zero
# effect on the installation — this only gets the Pi online.
#
# Does NOT disturb the field WiFi/AP fallback (setup/06_wifi.sh): the profile is
# created with autoconnect OFF, so a reboot in the field behaves exactly as before.
#
# usage: bash wifi_connect.sh "<SSID>" "<password>"
#        bash wifi_connect.sh "<SSID>"            # prompts for the password

set -e

SSID="${1:?usage: bash wifi_connect.sh \"<SSID>\" [password]}"
PSK="${2:-}"
IFACE=wlan0

# prime sudo unattended (local appliance, sudo password is 'raspi'); nmcli
# state changes need root over SSH where there's no active polkit session
echo raspi | sudo -S true 2>/dev/null || true

if [ -z "$PSK" ]; then
    read -rsp "Password for $SSID: " PSK
    echo
fi

echo "=== Joining $SSID ==="
# release the radio if the AP-fallback hotspot grabbed it
sudo nmcli connection down sparkles-ap 2>/dev/null || true

sudo nmcli device wifi rescan ifname "$IFACE" 2>/dev/null || true
sleep 3

if sudo nmcli device wifi connect "$SSID" password "$PSK" ifname "$IFACE"; then
    # keep this profile from hijacking boot back in the field
    sudo nmcli connection modify "$SSID" connection.autoconnect no 2>/dev/null || true
    echo "Connected to $SSID"
else
    echo "Failed to join $SSID (wrong password, or not in range)"
    exit 1
fi

echo "=== Checking internet ==="
if ping -c1 -W3 8.8.8.8 >/dev/null 2>&1; then
    echo "Online. Pi IP: $(hostname -I | awk '{print $1}')"
    echo "Now run:  bash ~/sparkles/update.sh"
else
    echo "Joined $SSID but no internet route — check the network."
    exit 1
fi

echo "=== Done ==="
