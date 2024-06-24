import os
import subprocess

# Get the list of devices
devices = os.popen('ls /dev/tty.wchusbserial*').read().strip().split()

# Check if any device was found
if not devices:
    print("No devices found.")
else:
    # Assuming there's only one device that matches
    device = devices[0]
    print(f"Device found: {device}")

    # Set the environment variable for the upload port
    os.environ['PLATFORMIO_UPLOAD_PORT'] = device

    # Upload the script using PlatformIO
    try:
        subprocess.run(['platformio', 'run', '--target', 'upload'], check=True)
        print("Upload successful.")
    except subprocess.CalledProcessError as e:
        print(f"Upload failed: {e}")