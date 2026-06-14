#!/usr/bin/env python3
"""
Generates the /clientAddress binary file for LittleFS upload.
Place the output in data/clientAddress and run: pio run -t uploadfs

Struct layout (ESP32, little-endian, default alignment):
  uint8_t  address[6]         6 bytes
  uint8_t  _pad[2]            2 bytes  (compiler aligns 'int' to 4 bytes)
  int32_t  id                 4 bytes
  float    xPos               4 bytes
  float    yPos               4 bytes
  float    zPos               4 bytes
  int32_t  timerOffset        4 bytes
  int32_t  delay              4 bytes
  float    distances[20]     80 bytes
  int32_t  active             4 bytes  (0=ACTIVE, 1=INACTIVE)
  float    batteryPercentage  4 bytes
  int32_t  tries              4 bytes
  float    distanceFromCenter 4 bytes
  uint32_t lastUpdateTime     4 bytes
  Total: 140 bytes per struct
"""

import struct
import os

NUM_DEVICES = 180
INACTIVE = 1
NUM_CLAPS = 20

# Unique MAC addresses extracted from loglog.txt + loglog2.txt (63 devices)
KNOWN_MACS = [
    "34:85:18:8e:f8:4c",
    "34:85:18:8e:f8:74",
    "34:85:18:8e:f8:84",
    "34:85:18:8e:f8:9c",
    "34:85:18:8e:f8:b4",
    "34:85:18:8e:f8:b8",
    "34:85:18:8e:f8:e8",
    "34:85:18:8e:f8:f8",
    "34:85:18:8e:f9:00",
    "34:85:18:8e:f9:08",
    "34:85:18:8e:f9:14",
    "34:85:18:8e:f9:2c",
    "34:85:18:8e:f9:38",
    "34:85:18:8e:f9:48",
    "34:85:18:8e:f9:68",
    "34:85:18:8f:8a:f8",
    "34:85:18:8f:8b:50",
    "34:85:18:8f:bf:1c",
    "34:85:18:8f:bf:20",
    "34:85:18:8f:bf:28",
    "34:85:18:8f:bf:30",
    "34:85:18:8f:bf:34",
    "34:85:18:8f:bf:3c",
    "34:85:18:8f:bf:74",
    "34:85:18:8f:bf:78",
    "34:85:18:8f:bf:8c",
    "34:85:18:8f:bf:94",
    "34:85:18:8f:bf:a8",
    "34:85:18:8f:bf:b4",
    "34:85:18:8f:bf:bc",
    "34:85:18:8f:bf:d4",
    "34:85:18:8f:bf:e8",
    "34:85:18:8f:bf:f4",
    "34:85:18:8f:bf:fc",
    "34:85:18:8f:c0:00",
    "34:85:18:8f:c0:20",
    "34:85:18:8f:c0:40",
    "34:85:18:8f:c0:68",
    "34:85:18:8f:c0:70",
    "34:85:18:8f:c0:a0",
    "34:85:18:8f:c0:b0",
    "34:85:18:8f:c0:c8",
    "34:85:18:8f:c0:e0",
    "34:85:18:8f:c0:f8",
    "34:85:18:8f:c1:18",
    "34:85:18:8f:c1:20",
    "34:85:18:8f:c1:24",
    "34:85:18:8f:c1:2c",
    "34:85:18:8f:c1:3c",
    "34:85:18:8f:c1:50",
    "34:85:18:8f:c1:60",
    "34:85:18:8f:c1:80",
    "34:85:18:8f:c1:94",
    "34:85:18:90:3a:90",
    "34:85:18:95:ea:34",
    "34:85:18:95:ea:38",
    "34:85:18:95:ea:80",
    "34:85:18:95:ea:94",
    "34:85:18:95:ea:a4",
    "34:85:18:95:ea:ac",
    "34:85:18:95:ea:e4",
    "34:85:18:95:eb:20",
    "34:85:18:95:eb:38",
]

def mac_to_bytes(mac_str):
    return bytes(int(b, 16) for b in mac_str.split(':'))

def pack_struct(address_bytes, slot_id):
    """Pack one client_address struct."""
    distances = [0.0] * NUM_CLAPS
    return struct.pack(
        '<6sxx'   # address[6] + 2 padding bytes
        'i'       # id
        'fff'     # xPos, yPos, zPos
        'ii'      # timerOffset, delay
        '20f'     # distances[20]
        'i'       # active (INACTIVE=1)
        'f'       # batteryPercentage
        'i'       # tries
        'f'       # distanceFromCenter
        'I',      # lastUpdateTime (unsigned long)
        address_bytes,
        slot_id,
        0.0, 0.0, 0.0,
        0, 0,
        *distances,
        INACTIVE,
        0.0,
        0,
        0.0,
        0,
    )

empty_address = bytes(6)

output = bytearray()
for i in range(NUM_DEVICES):
    if i < len(KNOWN_MACS):
        addr = mac_to_bytes(KNOWN_MACS[i])
    else:
        addr = empty_address
    output += pack_struct(addr, i)

out_dir = os.path.join(os.path.dirname(__file__), 'data')
os.makedirs(out_dir, exist_ok=True)
out_path = os.path.join(out_dir, 'clientAddress')
with open(out_path, 'wb') as f:
    f.write(output)

struct_size = len(output) // NUM_DEVICES
print(f"Struct size: {struct_size} bytes")
print(f"Total file size: {len(output)} bytes ({NUM_DEVICES} slots)")
print(f"Known MACs written: {len(KNOWN_MACS)}")
print(f"Written to: {out_path}")
