#!/bin/bash
# Toggle between home WiFi client and AP mode.

source /etc/wifi_profiles

current() {
    nmcli con show --active | grep -qE "^$AP_PROFILE\s" && echo "ap" || echo "client"
}

status() {
    mode=$(current)
    if [ "$mode" = "ap" ]; then
        ip=$(ip -4 addr show wlan0 | grep -oP '(?<=inet\s)\d+(\.\d+){3}')
        ssid=$(nmcli -t -f active,ssid dev wifi | grep '^yes' | cut -d: -f2)
        echo "Mode: AP (hotspot) | SSID: $ssid | IP: $ip"
    else
        ip=$(ip -4 addr show wlan0 | grep -oP '(?<=inet\s)\d+(\.\d+){3}')
        ssid=$(nmcli -t -f active,ssid dev wifi | grep '^yes' | cut -d: -f2)
        echo "Mode: client | SSID: $ssid | IP: $ip"
    fi
}

to_ap() {
    echo "Switching to AP mode..."
    nmcli con down "$HOME_PROFILE" 2>/dev/null || true
    nmcli con up "$AP_PROFILE"
    echo "AP active. Connect to the hotspot and SSH to 10.42.0.1"
}

to_client() {
    echo "Switching to client mode..."
    nmcli con down "$AP_PROFILE" 2>/dev/null || true
    nmcli con up "$HOME_PROFILE"
    echo "Client mode active."
}

case "${1:-toggle}" in
    status)
        status
        ;;
    ap)
        to_ap
        ;;
    client)
        to_client
        ;;
    toggle)
        if [ "$(current)" = "ap" ]; then
            to_client
        else
            to_ap
        fi
        ;;
    *)
        echo "Usage: wifi-toggle [toggle|ap|client|status]"
        ;;
esac
