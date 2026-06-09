#!/bin/bash
# Configure WiFi with AP fallback.
# Connects to WLIAN on boot. If not in range, creates Sparkles hotspot.
set -e

echo "=== [6/6] WiFi + AP Fallback ==="

WIFI_CON="sparkles-wifi"
WIFI_SSID="WLIAN"
WIFI_PSK="WaldMoeveBowieCanyon"

AP_CON="sparkles-ap"
AP_SSID="Sparkles"
AP_PSK="SparklesAdmin"

IFACE="wlan0"
SCRIPT=/usr/local/bin/sparkles-wifi-fallback.sh
SERVICE=/etc/systemd/system/sparkles-wifi.service

# ── 1. Known WiFi profile ─────────────────────────────────────────
if nmcli connection show "$WIFI_CON" &>/dev/null; then
    echo ">> WiFi profile '$WIFI_CON' already exists, updating"
    nmcli connection modify "$WIFI_CON" \
        wifi-sec.key-mgmt wpa-psk \
        wifi-sec.psk "$WIFI_PSK"
else
    echo ">> Creating WiFi profile '$WIFI_CON'"
    nmcli connection add \
        type wifi \
        ifname "$IFACE" \
        con-name "$WIFI_CON" \
        ssid "$WIFI_SSID" \
        wifi-sec.key-mgmt wpa-psk \
        wifi-sec.psk "$WIFI_PSK" \
        connection.autoconnect yes \
        connection.autoconnect-priority 10
fi

# ── 2. AP hotspot profile ─────────────────────────────────────────
if nmcli connection show "$AP_CON" &>/dev/null; then
    echo ">> AP profile '$AP_CON' already exists, skipping"
else
    echo ">> Creating AP profile '$AP_CON'"
    nmcli connection add \
        type wifi \
        ifname "$IFACE" \
        con-name "$AP_CON" \
        ssid "$AP_SSID" \
        802-11-wireless.mode ap \
        802-11-wireless.band bg \
        ipv4.method shared \
        wifi-sec.key-mgmt wpa-psk \
        wifi-sec.psk "$AP_PSK" \
        connection.autoconnect no
fi

# ── 3. Fallback script ────────────────────────────────────────────
echo ">> Writing fallback script to $SCRIPT"
cat > "$SCRIPT" << 'EOF'
#!/bin/bash
# Try known WiFi; fall back to AP hotspot if not in range.

WIFI_CON="sparkles-wifi"
WIFI_SSID="WLIAN"
AP_CON="sparkles-ap"
IFACE="wlan0"
LOG="logger -t sparkles-wifi"

$LOG "Checking for $WIFI_SSID..."

# Scan and check if SSID is visible
nmcli device wifi rescan ifname "$IFACE" 2>/dev/null || true
sleep 3

if nmcli device wifi list ifname "$IFACE" 2>/dev/null | grep -q "$WIFI_SSID"; then
    $LOG "$WIFI_SSID found — connecting"
    if nmcli connection up "$WIFI_CON" ifname "$IFACE"; then
        $LOG "Connected to $WIFI_SSID"
        exit 0
    else
        $LOG "Connection to $WIFI_SSID failed — starting AP"
    fi
else
    $LOG "$WIFI_SSID not in range — starting AP"
fi

nmcli connection up "$AP_CON" ifname "$IFACE"
$LOG "AP hotspot started (SSID: Sparkles)"
EOF
chmod +x "$SCRIPT"

# ── 4. Systemd service ────────────────────────────────────────────
echo ">> Writing systemd service $SERVICE"
cat > "$SERVICE" << EOF
[Unit]
Description=Sparkles WiFi Fallback
After=NetworkManager.service
Wants=NetworkManager.service

[Service]
Type=oneshot
ExecStart=$SCRIPT
RemainAfterExit=yes

[Install]
WantedBy=multi-user.target
EOF

systemctl daemon-reload
systemctl enable sparkles-wifi
systemctl start sparkles-wifi

# Ensure avahi is running so sparkles.local resolves on both WiFi and AP
systemctl enable avahi-daemon
systemctl start avahi-daemon

echo ""
echo "  Known WiFi : $WIFI_SSID"
echo "  AP fallback: $AP_SSID / $AP_PSK"
echo "=== WiFi configured ==="
