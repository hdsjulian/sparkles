#!/bin/bash

set -e

# 1. Compile for Client-Device environment
echo "Building firmware for Client-Device..."
pio run -e Client-Device

# 2. Copy the firmware to Master-Device/data folder
echo "Copying firmware.bin to Master-Device/data/..."
cp .pio/build/Client-Device/firmware.bin ./data/firmware_client.bin


# 3. Download clientAddress from 192.168.1.4 with timeout and fallback
echo "Downloading clientAddress..."
if ! curl -f --max-time 5 http://192.168.1.4/clientAddress -o ./Master-Device/data/clientAddress; then
  echo "Download failed, creating empty clientAddress file."
  > ./Master-Device/data/clientAddress
fi

# 4. Upload the data folder to the master device (SPIFFS/LittleFS)
echo "Uploading data folder to Master-Device..."
pio run -e Master-Device -t uploadfs

echo "Done!"