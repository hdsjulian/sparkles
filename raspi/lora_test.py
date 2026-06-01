#!/usr/bin/env python3
"""
Sends a test message via the T-Beam serial → LoRa every 10 seconds.
Use to verify the T-Beam → T-Deck LoRa link before deploying the log bridge.

Requirements:
  pip install pyserial

Usage:
  python3 lora_test.py
  python3 lora_test.py -p /dev/ttyACM0 -i 10
"""

import sys
import time
import signal
import serial
import argparse

SERIAL_PORT = '/dev/ttyUSB0'
BAUD_RATE   = 115200
INTERVAL    = 10  # seconds


def open_serial(port: str, baud: int) -> serial.Serial:
    while True:
        try:
            ser = serial.Serial(port, baud, timeout=1)
            print(f"[lora-test] Connected to T-Beam on {port}")
            return ser
        except serial.SerialException as e:
            print(f"[lora-test] Waiting for T-Beam ({e}) ...")
            time.sleep(3)


def main():
    parser = argparse.ArgumentParser(description="Send periodic LoRa test messages via T-Beam")
    parser.add_argument("-p", "--port", default=SERIAL_PORT, help=f"Serial port (default: {SERIAL_PORT})")
    parser.add_argument("-b", "--baud", default=BAUD_RATE, type=int, help=f"Baud rate (default: {BAUD_RATE})")
    parser.add_argument("-i", "--interval", default=INTERVAL, type=int, help=f"Interval in seconds (default: {INTERVAL})")
    args = parser.parse_args()

    ser = open_serial(args.port, args.baud)
    time.sleep(2)

    count = 0

    def shutdown(sig, frame):
        print("\n[lora-test] Stopped.")
        ser.close()
        sys.exit(0)

    signal.signal(signal.SIGINT, shutdown)
    signal.signal(signal.SIGTERM, shutdown)

    print(f"[lora-test] Sending every {args.interval}s — watch your T-Deck for messages")

    while True:
        count += 1
        msg = f"[LoRa test #{count}] Pi is alive — {time.strftime('%H:%M:%S')}"
        try:
            ser.write((msg + "\n").encode("utf-8"))
            print(f"[lora-test] Sent: {msg}")
        except serial.SerialException as e:
            print(f"[lora-test] Serial error: {e} — reconnecting...")
            ser.close()
            ser = open_serial(args.port, args.baud)
        time.sleep(args.interval)


if __name__ == "__main__":
    main()
