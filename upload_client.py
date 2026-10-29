import subprocess
import threading
import time
import os
import signal
import argparse
import re
import serial

PORTS = [
    "/dev/tty.usbmodem11101",
    "/dev/tty.usbmodem11201",
    "/dev/tty.usbmodem11301"
]

# ANSI colors per port index
PORT_COLORS = ["\033[92m", "\033[96m", "\033[95m"]  # green, cyan, magenta
RESET = "\033[0m"
BOLD  = "\033[1m"
RED   = "\033[91m"
YELLOW = "\033[93m"

VALID_ENVS = ["Master_Device", "Client_Device", "Clap_Device", "Raspi-Device", "Test_Device", "Log_Device"]

# counted by the MAC esptool prints, so a board plugged in twice counts once
flashed = set()
failed = 0
count_lock = threading.Lock()

# what a client prints at boot, see logBootBattery() in src/Client-Device/main.cpp
BOOT_BATTERY = re.compile(r"boot reading (\d+\.\d+) V, (\d+)%")


def build_firmware(env):
    print(f"Building {env}...")
    result = subprocess.run(["pio", "run", "-e", env], capture_output=True, text=True)
    if result.returncode != 0:
        print("Build failed:\n", result.stdout, result.stderr)
        exit(1)
    print("Build complete.")


def read_boot_battery(port, timeout=20):
    # the board resets after the upload and repeats its boot reading for 20 s;
    # the port can take a moment to come back
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            with serial.Serial(port, 115200, timeout=0.5) as ser:
                while time.time() < deadline:
                    found = BOOT_BATTERY.search(ser.readline().decode(errors="replace"))
                    if found:
                        return float(found.group(1)), int(found.group(2))
        except (serial.SerialException, OSError):
            time.sleep(0.5)
    return None


def battery_note(battery):
    if battery is None:
        return f"{YELLOW}no battery reading within 20 s{RESET}"
    volts, pct = battery
    if volts < 2.5:
        # a cell this flat could not run the board, so this is no cell reading at all
        return f"{RED}{BOLD}⚠ no battery seen ({volts:.2f} V){RESET}"
    if pct < 50:
        return f"{RED}{BOLD}⚠ battery {pct}% ({volts:.2f} V){RESET}"
    if pct < 90:
        return f"{YELLOW}⚠ battery {pct}% ({volts:.2f} V){RESET}"
    return f"battery {pct}% ({volts:.2f} V)"


def monitor_and_upload(port, env, color):
    global failed
    last_connected = False
    while True:
        connected = os.path.exists(port)
        if connected and not last_connected:
            print(f"{color}Device detected on {port}, uploading...{RESET}")
            upload = subprocess.run([
                "pio", "run", "-e", env, "-t", "upload", "--upload-port", port
            ], capture_output=True, text=True)
            if upload.returncode == 0:
                found = re.search(r"MAC: ([0-9a-fA-F:]{17})", upload.stdout)
                mac = found.group(1).lower() if found else f"{port} at {time.time()}"
                with count_lock:
                    again = mac in flashed
                    flashed.add(mac)
                    count = len(flashed)
                if again:
                    print(f"{color}{BOLD}✔ DONE: {port}  {mac}, flashed before, still {count} boards{RESET}")
                else:
                    print(f"{color}{BOLD}✔ DONE #{count}: {port}  {mac}{RESET}")
                if env == "Client_Device":
                    print(f"{color}   {port}  {mac}:{RESET} {battery_note(read_boot_battery(port))}")
            else:
                with count_lock:
                    failed += 1
                print(f"{RED}Upload failed{RESET} {color}on {port}{RESET}:\n", upload.stdout, upload.stderr)
            while os.path.exists(port):
                time.sleep(0.5)
        last_connected = connected
        time.sleep(0.5)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-d", "--device", required=True,
                        choices=VALID_ENVS,
                        help=f"PlatformIO environment to build and upload ({', '.join(VALID_ENVS)})")
    parser.add_argument("--no-compile", action="store_true",
                        help="Skip the build step and upload existing firmware")
    args = parser.parse_args()

    if not args.no_compile:
        build_firmware(args.device)

    threads = []
    for i, port in enumerate(PORTS):
        color = PORT_COLORS[i % len(PORT_COLORS)]
        t = threading.Thread(target=monitor_and_upload, args=(port, args.device, color), daemon=True)
        t.start()
        threads.append(t)
    print(f"Monitoring ports for {args.device}. Plug in devices to upload firmware. Press Enter to exit.")
    input()
    print(f"{len(flashed)} boards flashed, {failed} failed uploads")
    os._exit(0)
