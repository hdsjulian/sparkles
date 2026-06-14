#!/bin/bash

set -e

# data/ holds only clientAddress now, it gets uploaded to the master's LittleFS
mkdir -p ./data

# 1. Download clientAddress from 192.168.1.4 with timeout and fallback
echo "Downloading clientAddress..."
if curl -f --max-time 5 http://192.168.1.4/clientAddress -o ./data/clientAddress.tmp; then
  mv ./data/clientAddress.tmp ./data/clientAddress
else
  rm -f ./data/clientAddress.tmp
  if [ -f ./data/clientAddress ]; then
    echo "Download failed, keeping existing clientAddress file."
  else
    echo "Download failed, creating empty clientAddress file."
    > ./data/clientAddress
  fi
fi

# 2. Upload the data folder to the master device (LittleFS) and flash firmware
echo "Uploading data folder to Master-Device..."
pio run -e Master_Device -t uploadfs
pio run -e Master_Device -t upload
echo "Done!"
