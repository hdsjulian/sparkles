#!/usr/bin/env python3
"""
Send a MSG_LOG ESP-NOW message to master using ESPythoNOW.

Requires:
  pip install ESPythoNOW scapy
  sudo iw <iface> set type monitor   (or use --setup-monitor)

Usage:
  sudo python3 espnow_log_sender.py --iface wlan0 "Hello from Pi"
  sudo python3 espnow_log_sender.py --iface wlan0 --mac AA:BB:CC:DD:EE:FF "targeted msg"
"""

import argparse
import struct
import time

BROADCAST_MAC = "FF:FF:FF:FF:FF:FF"

MSG_LOG = 18

# message_data wire layout (Xtensa ESP32, no padding between these fields):
#   1B  messageType
#   6s  targetAddress
#   6s  senderAddress
#   Q   msgReceiveTime  (uint64_t, 8 bytes)
#   ------- payload starts here (offset 21) -------
#   180s  log.text  (char[180])
#
# Total sent: 201 bytes — within sizeof(message_data) so pushToRecvQueue accepts it.
HEADER_FMT = "<B6s6sQ"
TEXT_LEN = 180


def build_msg(text: str, target_mac: str, sender_mac: str) -> bytes:
    def mac_bytes(mac_str):
        return bytes(int(x, 16) for x in mac_str.split(":"))

    text_bytes = text.encode("utf-8", errors="replace")[:TEXT_LEN - 1]
    text_padded = text_bytes + b"\x00" * (TEXT_LEN - len(text_bytes))

    header = struct.pack(
        HEADER_FMT,
        MSG_LOG,
        mac_bytes(target_mac),
        mac_bytes(sender_mac),
        0,  # msgReceiveTime — master doesn't use this for MSG_LOG
    )
    return header + text_padded


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("text", nargs="?", help="Log message to send (omit to listen only)")
    parser.add_argument("--iface", default="wlan0", help="Monitor-mode WiFi interface")
    parser.add_argument("--mac", default=BROADCAST_MAC, help="Target MAC (default: broadcast)")
    parser.add_argument("--sender", default="DE:AD:BE:EF:00:01", help="Sender MAC to report")
    parser.add_argument("--count", type=int, default=1, help="Number of times to send")
    parser.add_argument("--interval", type=float, default=0.5, help="Seconds between sends")
    args = parser.parse_args()

    try:
        from ESPythoNOW import ESPythoNow
    except ImportError:
        print("ESPythoNOW not installed. Run:  pip install ESPythoNOW scapy")
        raise SystemExit(1)

    msg = build_msg(args.text, args.mac, args.sender)
    print(f"Sending {len(msg)}-byte MSG_LOG to {args.mac} via {args.iface}: {args.text!r}")

    def on_recv(from_mac, to_mac, data):
        print(f"[RECV] {from_mac} → {to_mac}  ({len(data)} bytes): {data!r}")

    espnow = ESPythoNow(interface=args.iface, callback=on_recv)
    espnow.start()

    if not args.text:
        print("Listening for ESP-NOW messages (Ctrl+C to stop)...")
        try:
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            pass
        return

    for i in range(args.count):
        espnow.send(args.mac, msg)
        print(f"  [{i+1}/{args.count}] sent")
        if i < args.count - 1:
            time.sleep(args.interval)


if __name__ == "__main__":
    main()
