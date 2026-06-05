#!/usr/bin/env python3
"""
Patch and install the ath9k_htc kernel module to support AR9271 dongles
whose USB endpoint numbers don't match what newer kernels (6.x) expect.

Must be run as root on the Raspberry Pi:
  sudo python3 patch_wifi_driver.py

Steps performed:
  1. Install build dependencies
  2. Clone the matching RPi kernel source
  3. Prepare the build tree
  4. Patch hif_usb.c to remove the endpoint number check
  5. Compile ath9k_htc.ko against the installed kernel headers
  6. Install the patched module
  7. Configure ath9k_htc to load on boot
"""

import os
import re
import subprocess
import sys
from pathlib import Path

BUILD_DIR = Path.home() / "linux"
KERNEL = subprocess.check_output(["uname", "-r"]).decode().strip()
# e.g. "6.12.75+rpt-rpi-v8" → branch "rpi-6.12.y"
_ver = re.match(r"(\d+\.\d+)", KERNEL)
BRANCH = f"rpi-{_ver.group(1)}.y" if _ver else "rpi-6.12.y"
HEADERS_DIR = Path(f"/usr/src/linux-headers-{KERNEL}")
MODULE_SRC = BUILD_DIR / "drivers/net/wireless/ath/ath9k"
MODULE_DEST = Path(f"/lib/modules/{KERNEL}/kernel/drivers/net/wireless/ath/ath9k/ath9k_htc.ko")
HIF_USB = BUILD_DIR / "drivers/net/wireless/ath/ath9k/hif_usb.c"
BOOT_CONF = Path("/etc/modules-load.d/ath9k_htc.conf")


def run(cmd, **kwargs):
    print(f"  $ {cmd}")
    result = subprocess.run(cmd, shell=True, **kwargs)
    if result.returncode != 0:
        print(f"ERROR: command failed (exit {result.returncode})")
        sys.exit(result.returncode)
    return result


def step(title):
    print(f"\n{'='*60}\n  {title}\n{'='*60}")


def already_patched():
    if not MODULE_DEST.exists():
        return False
    out = subprocess.run(
        ["modinfo", str(MODULE_DEST)], capture_output=True, text=True
    )
    return "out-of-tree" in out.stdout or "tainted" in out.stdout


def check_root():
    if os.geteuid() != 0:
        print("Run as root:  sudo python3 patch_wifi_driver.py")
        sys.exit(1)


def install_deps():
    step("Installing build dependencies")
    run("apt-get install -y git build-essential flex bison bc libssl-dev libelf-dev")


def clone_kernel():
    step(f"Cloning RPi kernel source (branch {BRANCH})")
    if BUILD_DIR.exists():
        print(f"  {BUILD_DIR} already exists, skipping clone")
        return
    run(
        f"git clone --depth=1 --branch {BRANCH} "
        f"https://github.com/raspberrypi/linux.git {BUILD_DIR}"
    )


def prepare_tree():
    step("Preparing kernel build tree")
    config = Path(f"/boot/config-{KERNEL}")
    if not config.exists():
        # fall back to /boot/config
        config = Path("/boot/config")
    run(f"cp {config} {BUILD_DIR}/.config")
    run(f"make -C {BUILD_DIR} olddefconfig")
    run(f"make -C {BUILD_DIR} scripts")
    run(f"make -C {BUILD_DIR} prepare")


def patch_hif_usb():
    step("Patching hif_usb.c — removing endpoint number check")
    src = HIF_USB.read_text()

    if "// patched: endpoint check removed" in src:
        print("  Already patched, skipping")
        return

    # The block we want to neuter looks like:
    #   if (usb_find_common_endpoints(...) < 0 ||
    #       usb_endpoint_num(bulk_in) != USB_WLAN_RX_PIPE || ...) {
    #           dev_err(..., "Device endpoint numbers are not the expected ones\n");
    #           return -ENODEV;   ← remove this line
    #   }
    patched = re.sub(
        r'(dev_err\([^;]+?"ath9k_htc: Device endpoint numbers are not the expected ones\\n"\);\s*\n)'
        r'(\s*return\s*-ENODEV;\s*\n)',
        r'\1'  # keep the dev_err log, drop the return
        r'        // patched: endpoint check removed\n',
        src,
    )

    if patched == src:
        print("  Pattern not found — checking manually...")
        # Fallback: find the line and remove it
        lines = src.splitlines(keepends=True)
        new_lines = []
        for line in lines:
            if "return -ENODEV;" in line and "endpoint" not in line:
                # only skip the return that follows the endpoint error message
                # look back to see if recent context had the endpoint message
                context = "".join(new_lines[-5:])
                if "endpoint numbers" in context:
                    new_lines.append("        // patched: endpoint check removed\n")
                    print(f"  Removed: {line.rstrip()}")
                    continue
            new_lines.append(line)
        patched = "".join(new_lines)

    HIF_USB.write_text(patched)
    print("  Patch applied")

    # Verify
    if "return -ENODEV" in HIF_USB.read_text():
        # Check if there's still a -ENODEV near the endpoint error message
        lines = HIF_USB.read_text().splitlines()
        for i, line in enumerate(lines):
            if "endpoint numbers are not the expected" in line:
                nearby = lines[i:i+5]
                if any("return -ENODEV" in l for l in nearby):
                    print("WARNING: patch may not have worked — check hif_usb.c manually")


def build_module():
    step("Compiling ath9k_htc module")
    run(
        f"make -j$(nproc) -C {HEADERS_DIR} "
        f"M={MODULE_SRC} modules"
    )


def install_module():
    step("Installing patched module")
    src = MODULE_SRC / "ath9k_htc.ko"
    if not src.exists():
        print(f"ERROR: {src} not found — build may have failed")
        sys.exit(1)
    run(f"cp {src} {MODULE_DEST}")
    run("depmod -a")
    print(f"  Installed to {MODULE_DEST}")


def configure_autoload():
    step("Configuring module to load on boot")
    BOOT_CONF.write_text("ath9k_htc\n")
    print(f"  Written {BOOT_CONF}")


def reload_module():
    step("Reloading module")
    subprocess.run("modprobe -r ath9k_htc", shell=True)  # ignore error if not loaded
    run("modprobe ath9k_htc")
    result = subprocess.run("ip link show wlan1", shell=True, capture_output=True)
    if result.returncode == 0:
        print("\n  wlan1 is UP")
        print("\n  To enable monitor mode:")
        print("    sudo ip link set wlan1 down")
        print("    sudo iw wlan1 set type monitor")
        print("    sudo ip link set wlan1 up")
        print("    sudo iw wlan1 set channel 1")
    else:
        print("\n  wlan1 not detected yet — try unplugging and replugging the dongle")


def main():
    check_root()
    print(f"Kernel: {KERNEL}")
    print(f"Branch: {BRANCH}")
    print(f"Headers: {HEADERS_DIR}")

    if not HEADERS_DIR.exists():
        print(f"ERROR: kernel headers not found at {HEADERS_DIR}")
        print(f"  Run: sudo apt install linux-headers-{KERNEL}")
        sys.exit(1)

    install_deps()
    clone_kernel()
    prepare_tree()
    patch_hif_usb()
    build_module()
    install_module()
    configure_autoload()
    reload_module()

    print("\n✓ Done — AR9271 dongle should now work as wlan1")


if __name__ == "__main__":
    main()
