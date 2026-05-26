import subprocess
import threading
import time
import os
import signal
import argparse

PORTS = [
    "/dev/tty.usbmodem11101",
    "/dev/tty.usbmodem11201",
    "/dev/tty.usbmodem11301"
]

# ANSI colors per port index
PORT_COLORS = ["\033[92m", "\033[96m", "\033[95m"]  # green, cyan, magenta
RESET = "\033[0m"
BOLD  = "\033[1m"

VALID_ENVS = ["Master_Device", "Client_Device", "Clap_Device", "Raspi-Device", "Test_Device", "Log_Device"]


def build_firmware(env):
    print(f"Building {env}...")
    result = subprocess.run(["pio", "run", "-e", env], capture_output=True, text=True)
    if result.returncode != 0:
        print("Build failed:\n", result.stdout, result.stderr)
        exit(1)
    print("Build complete.")


def monitor_and_upload(port, env, color):
    last_connected = False
    while True:
        connected = os.path.exists(port)
        if connected and not last_connected:
            print(f"Device detected on {port}, uploading...")
            upload = subprocess.run([
                "pio", "run", "-e", env, "-t", "upload", "--upload-port", port
            ], capture_output=True, text=True)
            if upload.returncode == 0:
                print(f"{color}{BOLD}✔ DONE: {port}{RESET}")
            else:
                print(f"Upload failed on {port}:\n", upload.stdout, upload.stderr)
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
    os._exit(0)
