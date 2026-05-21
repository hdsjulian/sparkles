import subprocess
import threading
import time
import os
import argparse

PORTS = [
    "/dev/tty.usbmodem11101",
    "/dev/tty.usbmodem11201",
    "/dev/tty.usbmodem11301"
]

VALID_ENVS = ["Master_Device", "Client_Device", "Clap_Device", "Raspi-Device", "Test_Device"]


def build_firmware(env):
    print(f"Building {env}...")
    result = subprocess.run(["pio", "run", "-e", env], capture_output=True, text=True)
    if result.returncode != 0:
        print("Build failed:\n", result.stdout, result.stderr)
        exit(1)
    print("Build complete.")


def monitor_and_upload(port, env):
    last_connected = False
    while True:
        connected = os.path.exists(port)
        if connected and not last_connected:
            print(f"Device detected on {port}, uploading...")
            upload = subprocess.run([
                "pio", "run", "-e", env, "-t", "upload", "--upload-port", port
            ], capture_output=True, text=True)
            if upload.returncode == 0:
                print(f"Upload complete, monitoring for battery info on {port}...")
                monitor_cmd = ["pio", "device", "monitor", "--port", port, "--baud", "115200"]
                try:
                    with subprocess.Popen(monitor_cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True) as proc:
                        battery_line = None
                        start_time = time.time()
                        while True:
                            line = proc.stdout.readline()
                            if not line:
                                break
                            if "Battery:" in line:
                                battery_line = line.strip()
                                print(f"{port} {battery_line}")
                                break
                            if time.time() - start_time > 15:
                                break
                        proc.terminate()
                        if not battery_line:
                            print(f"{port} Battery info not found.")
                except Exception as e:
                    print(f"Error monitoring device on {port}: {e}")
                print(f"Done: {port}")
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
    args = parser.parse_args()

    build_firmware(args.device)

    threads = []
    for port in PORTS:
        t = threading.Thread(target=monitor_and_upload, args=(port, args.device), daemon=True)
        t.start()
        threads.append(t)
    print(f"Monitoring ports for {args.device}. Plug in devices to upload firmware.")
    while True:
        time.sleep(1)
