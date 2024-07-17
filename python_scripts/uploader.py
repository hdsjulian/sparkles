import serial.tools.list_ports
import subprocess
import time
import sys
def find_device():
    devices = serial.tools.list_ports.comports()
    target_devices = ['usbmodem11101', 'usbmodem11201', 'usbmodem11301', 'usbmodem11401']
    for device in devices:
        for target in target_devices:
            if target in device.device:
                return device.device, target_devices.index(target) + 1
    return None, None

def upload_project(device):
    # Replace '/path/to/project' with the actual path to your PlatformIO project
    project_path = sys.argv[1]
    environment_name = 'Blinky-V4'
    command = f'platformio run --target upload --upload-port {device} --project-dir {project_path} --environment {environment_name}'
    print(f"Uploading project to {device}...")
    print(f"Running command: {command}")
    subprocess.run(command, shell=True, check=True)

def main():
    print("Starting device scan...")
    known_devices = set()
    while True:
        device, device_number = find_device()
        if device and device not in known_devices:
            print(f"Device {device_number} found: {device}")
            upload_project(device)
            print(f"Device {device_number} is ready to be removed")
            known_devices.add(device)
        elif device in known_devices:
            # Check if the device has been removed
            current_devices = [dev.device for dev in serial.tools.list_ports.comports()]
            if device not in current_devices:
                known_devices.remove(device)
                print(f"Device {device_number} removed. Waiting for the next device...")
        time.sleep(1)

if __name__ == "__main__":
    main()