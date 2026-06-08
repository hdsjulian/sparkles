#!/bin/bash
# Run once on the Pi to configure both WiFi profiles.
set -e

echo "=== WiFi Toggle Setup ==="
echo ""
read -p "Home WiFi SSID: " HOME_SSID
read -s -p "Home WiFi password: " HOME_PASS
echo ""
read -p "AP SSID (what this Pi will broadcast): " AP_SSID
read -s -p "AP password (min 8 chars): " AP_PASS
echo ""

# Connect to home WiFi
echo ">> Setting up home WiFi profile..."
nmcli con delete "$HOME_SSID" 2>/dev/null || true
nmcli con add type wifi ifname wlan0 con-name "$HOME_SSID" ssid "$HOME_SSID" \
    wifi-sec.key-mgmt wpa-psk wifi-sec.psk "$HOME_PASS" \
    connection.autoconnect no

# Create AP profile
echo ">> Setting up AP profile..."
nmcli con delete "hotspot" 2>/dev/null || true
nmcli con add type wifi ifname wlan0 con-name "hotspot" ssid "$AP_SSID" \
    802-11-wireless.mode ap \
    802-11-wireless.band bg \
    ipv4.method shared \
    ipv4.addresses 10.42.0.1/24 \
    wifi-sec.key-mgmt wpa-psk \
    wifi-sec.psk "$AP_PASS" \
    connection.autoconnect no

# Save profile names for the toggle script
sudo bash -c "cat > /etc/wifi_profiles <<EOF
HOME_PROFILE=$HOME_SSID
AP_PROFILE=hotspot
EOF"

# Install toggle script
sudo cp "$(dirname "$0")/wifi_toggle.sh" /usr/local/bin/wifi-toggle
sudo chmod +x /usr/local/bin/wifi-toggle

echo ""
echo "=== Done ==="
echo "Run 'wifi-toggle' to switch between client and AP mode."
echo "Run 'wifi-toggle status' to see current mode."
