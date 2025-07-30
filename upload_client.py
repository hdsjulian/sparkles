import subprocess
import threading
import time
import os

PORTS = [
    "/dev/tty.usbmodem11101",
    "/dev/tty.usbmodem11201",
    "/dev/tty.usbmodem11301"
]

def build_firmware():
    print("Building firmware...")
    result = subprocess.run(["pio", "run", "-e", "Client_Device"], capture_output=True, text=True)
    if result.returncode != 0:
        print("Build failed:\n", result.stdout, result.stderr)
        exit(1)
    print("Build complete.")

def monitor_and_upload(port):
    last_connected = False
    while True:
        connected = os.path.exists(port)
        if connected and not last_connected:
            print(f"Device detected on {port}, uploading...")
            upload = subprocess.run([
                "pio", "run", "-e", "Client_Device", "-t", "upload", "--upload-port", port
            ], capture_output=True, text=True)
            if upload.returncode == 0:
                # After upload, monitor for Battery output
                print(f"Upload complete, monitoring for battery info on {port}...")
                monitor_cmd = ["pio", "device", "monitor", "--port", port, "--baud", "115200"]
                try:
                    with subprocess.Popen(monitor_cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True) as proc:
                        battery_line = None
                        start_time = time.time()
                        # Read lines for up to 10 seconds
                        while True:
                            line = proc.stdout.readline()
                            if not line:
                                break
                            if "Battery:" in line:
                                battery_line = line.strip()
                                print(f"{port} {battery_line}")
                                break
                            # Timeout after 10 seconds
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
            # Wait for device to disconnect before next upload
            while os.path.exists(port):
                time.sleep(0.5)
        last_connected = connected
        time.sleep(0.5)

if __name__ == "__main__":
    build_firmware()
    threads = []
    for port in PORTS:
        t = threading.Thread(target=monitor_and_upload, args=(port,), daemon=True)
        t.start()
        threads.append(t)
    print("Monitoring ports. Plug in devices to upload firmware.")
    while True:
        time.sleep(1)