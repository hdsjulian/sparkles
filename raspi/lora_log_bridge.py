#!/usr/bin/env python3
"""
Tails the Pi system journal (or a log file) and forwards each line
to a Meshtastic T-Beam over serial, which relays it over LoRa via
the Meshtastic Serial Module.

Setup on T-Beam (once, via Meshtastic CLI or app):
  meshtastic --set serial.enabled true
  meshtastic --set serial.mode TEXTMSG
  meshtastic --set serial.baud BAUD_115200
  meshtastic --set serial.rxd 34   # T-Beam default RX pin
  meshtastic --set serial.txd 12   # T-Beam default TX pin

Requirements:
  pip install pyserial

Usage:
  python3 lora_log_bridge.py                    # tail journalctl
  python3 lora_log_bridge.py /var/log/syslog    # tail a file
  python3 lora_log_bridge.py -u myservice.service  # tail a specific unit
"""

import sys
import time
import signal
import serial
import subprocess
import argparse

SERIAL_PORT = '/dev/ttyUSB0'   # Adjust to wherever the T-Beam appears
BAUD_RATE   = 115200
MAX_LINE    = 200              # Meshtastic message limit (chars)


def open_serial(port: str, baud: int) -> serial.Serial:
    while True:
        try:
            ser = serial.Serial(port, baud, timeout=1)
            print(f"[bridge] Connected to T-Beam on {port}")
            return ser
        except serial.SerialException as e:
            print(f"[bridge] Waiting for T-Beam ({e}) ...")
            time.sleep(3)


def send_line(ser: serial.Serial, line: str):
    line = line.strip()
    if not line:
        return
    # Truncate so we never overflow a Meshtastic packet
    if len(line) > MAX_LINE:
        line = line[:MAX_LINE - 1] + "…"
    try:
        ser.write((line + "\n").encode("utf-8", errors="replace"))
    except serial.SerialException as e:
        print(f"[bridge] Serial write error: {e}")


def tail_journal(unit: str | None):
    cmd = ["journalctl", "-f", "-o", "short-monotonic", "--no-pager"]
    if unit:
        cmd += ["-u", unit]
    return subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                            text=True, bufsize=1)


def tail_file(path: str):
    return subprocess.Popen(["tail", "-F", path],
                            stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                            text=True, bufsize=1)


def main():
    parser = argparse.ArgumentParser(description="Forward Pi logs to T-Beam over LoRa")
    parser.add_argument("logfile", nargs="?", help="Path to a log file to tail (default: journalctl)")
    parser.add_argument("-u", "--unit", help="Systemd unit to filter (e.g. sparkles.service)")
    parser.add_argument("-p", "--port", default=SERIAL_PORT, help=f"Serial port (default: {SERIAL_PORT})")
    parser.add_argument("-b", "--baud", default=BAUD_RATE, type=int, help=f"Baud rate (default: {BAUD_RATE})")
    args = parser.parse_args()

    ser = open_serial(args.port, args.baud)
    time.sleep(2)  # Let T-Beam settle after serial connect

    proc = tail_file(args.logfile) if args.logfile else tail_journal(args.unit)

    def shutdown(sig, frame):
        print("\n[bridge] Shutting down.")
        proc.terminate()
        ser.close()
        sys.exit(0)

    signal.signal(signal.SIGINT, shutdown)
    signal.signal(signal.SIGTERM, shutdown)

    source = args.logfile or (f"unit={args.unit}" if args.unit else "journalctl")
    print(f"[bridge] Forwarding {source} → {args.port} → LoRa")

    for line in proc.stdout:
        send_line(ser, line)


if __name__ == "__main__":
    main()
