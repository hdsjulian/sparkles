import subprocess
import sys
import os

HOST = "192.168.4.1"
USER = "raspi"
REMOTE_PATH = "/var/www/html/firmware.bin"
BIN_PATH = ".pio/build/Client_Device/firmware.bin"


def build():
    print("Building Client_Device...")
    result = subprocess.run(["pio", "run", "-e", "Client_Device"], capture_output=False)
    if result.returncode != 0:
        print("Build failed.")
        sys.exit(1)
    print("Build complete.")


def deploy():
    if not os.path.exists(BIN_PATH):
        print(f"Binary not found at {BIN_PATH}")
        sys.exit(1)
    with open(BIN_PATH, "rb") as f:
        magic = f.read(1)
    if magic != b'\xe9':
        print(f"Bad binary: first byte is {magic.hex()!r}, expected e9. Not uploading.")
        sys.exit(1)
    print(f"Copying to {USER}@{HOST}:{REMOTE_PATH} ...")
    result = subprocess.run(["scp", BIN_PATH, f"{USER}@{HOST}:{REMOTE_PATH}"], capture_output=False)
    if result.returncode != 0:
        print("Upload failed.")
        sys.exit(1)
    print("Done.")


if __name__ == "__main__":
    build()
    deploy()
