#!/usr/bin/env python3
"""
SparklesMic Pi 5 setup script.

Run as root on a fresh Raspberry Pi OS Bookworm:
  sudo python3 setup_sparklesmic.py

What it does:
  1. Install system packages (audio, Python, build tools)
  2. Clone / update the repo
  3. Create Python venv + install aubioAlgo dependencies + ESPythoNOW
  4. Patch the ath9k_htc kernel module for the AR9271 dongle
  5. Persist wlan1 in monitor mode on boot (via systemd)
  6. Configure WiFi: connect to WLAN (WaldMoeveBowieCanyon) or
     fall back to AP (SparklesMic / SparklesMicAdmin)
  7. Install the wifi-toggle helper
  8. Install a systemd service that auto-starts aubioAlgo
"""

import os
import subprocess
import sys
import time
from pathlib import Path

# ---------------------------------------------------------------------------
# Config
# ---------------------------------------------------------------------------
REPO_URL    = "https://github.com/hdsjulian/sparkles"
REPO_BRANCH = "feature/raspi-espnow"
REPO_DIR    = Path.home() / "SparklesCurrent"

PITCHDETECTOR_DIR = REPO_DIR / "pitchdetector"
VENV_DIR          = PITCHDETECTOR_DIR / ".venv"

KNOWN_SSID     = "WLIAN"
KNOWN_PASS     = "WaldMoeveBowieCanyon"
AP_SSID        = "SparklesMic"
AP_PASS        = "SparklesMicAdmin"
AP_IP          = "10.42.0.1"

ESPNOW_IFACE   = "wlan1"
ESPNOW_CHANNEL = 1

USER = os.environ.get("SUDO_USER") or os.environ.get("USER") or "raspi"
HOME = Path(f"/home/{USER}")


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
def run(cmd, check=True, **kwargs):
    print(f"  $ {cmd}")
    return subprocess.run(cmd, shell=True, check=check, **kwargs)


def step(title):
    print(f"\n{'='*60}\n  {title}\n{'='*60}")


def write_file(path, content, mode=0o644):
    Path(path).write_text(content)
    os.chmod(path, mode)
    print(f"  wrote {path}")


def check_root():
    if os.geteuid() != 0:
        print("Run as root:  sudo python3 setup_sparklesmic.py")
        sys.exit(1)


# ---------------------------------------------------------------------------
# 1. System packages
# ---------------------------------------------------------------------------
def install_system_packages():
    step("Installing system packages")
    packages = [
        # build essentials (needed for driver patch + pip packages with C extensions)
        "build-essential", "git", "python3-pip", "python3-venv", "python3-dev",
        # audio
        "portaudio19-dev", "libasound2-dev", "libsndfile1-dev", "python3-pyaudio",
        # aubio system lib (speeds up pip install)
        "libaubio-dev", "aubio-tools",
        # WiFi driver build deps
        "flex", "bison", "bc", "libssl-dev", "libelf-dev",
        # networking
        "network-manager",
        # misc
        "curl", "wget",
    ]
    run("apt-get update -y")
    run(f"apt-get install -y {' '.join(packages)}")


# ---------------------------------------------------------------------------
# 2. Clone / update repo
# ---------------------------------------------------------------------------
def setup_repo():
    step("Cloning / updating repository")
    if REPO_DIR.exists():
        print(f"  {REPO_DIR} exists — pulling")
        run(f"git -C {REPO_DIR} fetch origin")
        run(f"git -C {REPO_DIR} checkout {REPO_BRANCH}")
        run(f"git -C {REPO_DIR} pull origin {REPO_BRANCH}")
    else:
        run(f"sudo -u {USER} git clone --branch {REPO_BRANCH} {REPO_URL} {REPO_DIR}")
    run(f"chown -R {USER}:{USER} {REPO_DIR}")


# ---------------------------------------------------------------------------
# 3. Python venv + packages
# ---------------------------------------------------------------------------
def setup_venv():
    step("Setting up Python venv")
    run(f"sudo -u {USER} python3 -m venv {VENV_DIR}")
    pip = VENV_DIR / "bin" / "pip"
    run(f"{pip} install --upgrade pip")

    requirements = PITCHDETECTOR_DIR / "requirements.txt"
    run(f"{pip} install -r {requirements}")

    # ESPythoNOW + Scapy for ESP-NOW over wlan1
    run(f"{pip} install 'git+https://github.com/ChuckMash/ESPythoNOW.git' scapy")


# ---------------------------------------------------------------------------
# 4. Kernel module patch for AR9271 dongle
# ---------------------------------------------------------------------------
def patch_wifi_driver():
    step("Patching ath9k_htc kernel module for AR9271 dongle")
    patch_script = REPO_DIR / "raspi" / "patch_wifi_driver.py"
    if not patch_script.exists():
        print(f"  WARNING: {patch_script} not found — skipping driver patch")
        return
    run(f"python3 {patch_script}")


# ---------------------------------------------------------------------------
# 5. Persist wlan1 in monitor mode on boot
# ---------------------------------------------------------------------------
def setup_monitor_mode_service():
    step(f"Setting up {ESPNOW_IFACE} monitor mode systemd service")

    # Tell NetworkManager to ignore wlan1 entirely
    nm_conf = "/etc/NetworkManager/conf.d/99-espnow-unmanaged.conf"
    write_file(nm_conf, f"""[keyfile]
unmanaged-devices=interface-name:{ESPNOW_IFACE}
""")

    service = f"""[Unit]
Description=Put {ESPNOW_IFACE} into monitor mode for ESP-NOW
After=sys-subsystem-net-devices-{ESPNOW_IFACE}.device
BindsTo=sys-subsystem-net-devices-{ESPNOW_IFACE}.device

[Service]
Type=oneshot
RemainAfterExit=yes
ExecStart=/bin/bash -c "\\
    ip link set {ESPNOW_IFACE} down && \\
    iw {ESPNOW_IFACE} set type monitor && \\
    ip link set {ESPNOW_IFACE} up && \\
    iw {ESPNOW_IFACE} set channel {ESPNOW_CHANNEL}"
ExecStop=/bin/bash -c "\\
    ip link set {ESPNOW_IFACE} down && \\
    iw {ESPNOW_IFACE} set type managed && \\
    ip link set {ESPNOW_IFACE} up"

[Install]
WantedBy=multi-user.target
"""
    path = f"/etc/systemd/system/espnow-monitor.service"
    write_file(path, service)
    run("systemctl daemon-reload")
    run("systemctl enable espnow-monitor.service")
    print(f"  espnow-monitor.service enabled — wlan1 will be in monitor mode on boot")


# ---------------------------------------------------------------------------
# 6. WiFi: known network + AP fallback
# ---------------------------------------------------------------------------
def setup_wifi():
    step("Configuring WiFi profiles (NM)")

    # Remove stale profiles if they exist
    for name in [KNOWN_SSID, "sparklesmic-ap"]:
        run(f"nmcli con delete '{name}'", check=False)

    # Known home/venue WiFi — autoconnect with high priority
    run(
        f"nmcli con add type wifi ifname wlan0 con-name '{KNOWN_SSID}' ssid '{KNOWN_SSID}' "
        f"wifi-sec.key-mgmt wpa-psk wifi-sec.psk '{KNOWN_PASS}' "
        f"connection.autoconnect yes "
        f"connection.autoconnect-priority 10"
    )

    # AP / hotspot profile — lower priority, activated by fallback service
    run(
        f"nmcli con add type wifi ifname wlan0 con-name 'sparklesmic-ap' ssid '{AP_SSID}' "
        f"802-11-wireless.mode ap "
        f"802-11-wireless.band bg "
        f"ipv4.method shared "
        f"ipv4.addresses {AP_IP}/24 "
        f"wifi-sec.key-mgmt wpa-psk "
        f"wifi-sec.psk '{AP_PASS}' "
        f"connection.autoconnect no "
        f"connection.autoconnect-priority 1"
    )

    # Save profile names for wifi-toggle
    write_file("/etc/wifi_profiles", f"""HOME_PROFILE={KNOWN_SSID}
AP_PROFILE=sparklesmic-ap
""")

    # Install wifi-toggle script
    toggle_src = REPO_DIR / "pitchdetector" / "wifi_toggle.sh"
    if toggle_src.exists():
        run(f"cp {toggle_src} /usr/local/bin/wifi-toggle")
        os.chmod("/usr/local/bin/wifi-toggle", 0o755)
        print("  wifi-toggle installed at /usr/local/bin/wifi-toggle")

    # Fallback service: if no IP on wlan0 after 30s, activate AP
    fallback_service = f"""[Unit]
Description=SparklesMic WiFi fallback — switch to AP if no connection
After=NetworkManager.service
Wants=NetworkManager.service

[Service]
Type=oneshot
ExecStart=/bin/bash -c "\\
    echo 'Waiting 30s for WiFi connection...' ; \\
    sleep 30 ; \\
    if ! nmcli -t -f STATE general | grep -q '^connected$'; then \\
        echo 'No connection — activating AP ({AP_SSID})' ; \\
        nmcli con up 'sparklesmic-ap' ; \\
    else \\
        echo 'Connected — staying in client mode' ; \\
    fi"
RemainAfterExit=yes

[Install]
WantedBy=multi-user.target
"""
    write_file("/etc/systemd/system/sparklesmic-wifi-fallback.service", fallback_service)
    run("systemctl daemon-reload")
    run("systemctl enable sparklesmic-wifi-fallback.service")
    print(f"  WiFi fallback service enabled (AP: {AP_SSID} / {AP_PASS} @ {AP_IP})")


# ---------------------------------------------------------------------------
# 7. aubioAlgo systemd service
# ---------------------------------------------------------------------------
def setup_aubio_service():
    step("Installing aubioAlgo systemd service")
    python = VENV_DIR / "bin" / "python"
    script = PITCHDETECTOR_DIR / "aubioAlgo.py"

    service = f"""[Unit]
Description=SparklesMic aubio pitch detector (ESP-NOW output)
After=network.target espnow-monitor.service
Wants=espnow-monitor.service

[Service]
ExecStart={python} {script} --iface {ESPNOW_IFACE}
WorkingDirectory={PITCHDETECTOR_DIR}
User={USER}
Restart=always
RestartSec=5
# aubioAlgo needs raw socket access for Scapy/ESPythoNOW
AmbientCapabilities=CAP_NET_RAW CAP_NET_ADMIN
CapabilityBoundingSet=CAP_NET_RAW CAP_NET_ADMIN
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
"""
    write_file("/etc/systemd/system/sparklesmic-aubio.service", service)
    run("systemctl daemon-reload")
    run("systemctl enable sparklesmic-aubio.service")
    print("  sparklesmic-aubio.service enabled")
    print("  Start now with:  sudo systemctl start sparklesmic-aubio")
    print("  Follow logs:     sudo journalctl -fu sparklesmic-aubio")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def main():
    check_root()
    print(f"""
SparklesMic Pi 5 Setup
  User:       {USER}
  Repo:       {REPO_DIR}
  Known WiFi: {KNOWN_SSID}
  AP fallback:{AP_SSID} / {AP_PASS}
  ESP-NOW:    {ESPNOW_IFACE}  channel {ESPNOW_CHANNEL}
""")

    install_system_packages()
    setup_repo()
    setup_venv()
    patch_wifi_driver()
    setup_monitor_mode_service()
    setup_wifi()
    setup_aubio_service()

    step("Done")
    print(f"""
  Reboot to apply everything:  sudo reboot

  After reboot:
    wifi-toggle status           — check WiFi mode
    wifi-toggle ap               — force AP mode
    wifi-toggle client           — force client mode
    sudo systemctl start sparklesmic-aubio
    sudo journalctl -fu sparklesmic-aubio

  AP: connect to '{AP_SSID}' with '{AP_PASS}', SSH to {AP_IP}
""")


if __name__ == "__main__":
    main()
