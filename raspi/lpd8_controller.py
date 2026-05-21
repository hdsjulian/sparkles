#!/usr/bin/env python3
"""
LPD8 MIDI controller bridge for Sparkles installation.

Reads AKAI LPD8 pad input via USB-MIDI and sends commands to the
Raspi-Device ESP32 over serial.

Pad 8 toggles between FREQUENCY_MODE and MIDI_MODE.
Pad LEDs on the LPD8 mirror the current toggle state via MIDI feedback.

Serial protocol to ESP32:
  PAD,<idx>,<velocity>   pad state (idx 0-7, velocity 0=off 127=on)
  MODE,<mode>            mode change (1=MIDI, 2=FREQUENCY)

Requirements:
  pip install mido python-rtmidi pyserial
"""

import sys
import time
import mido
import serial

SERIAL_PORT = '/dev/ttyUSB0'   # Adjust: /dev/ttyACM0, /dev/ttyUSB0, etc.
BAUD_RATE = 115200

FREQUENCY_MODE = 2
MIDI_MODE = 1

# LPD8 Bank A default note assignments (pad index 0-7)
LPD8_NOTES = [40, 41, 42, 43, 44, 45, 46, 47]
LPD8_CHANNEL = 9  # MIDI channel 10 (0-indexed)


def find_midi_port(ports, keyword):
    for name in ports:
        if keyword.lower() in name.lower():
            return name
    return None


def main():
    input_ports = mido.get_input_names()
    output_ports = mido.get_output_names()

    in_port_name = find_midi_port(input_ports, 'LPD8')
    out_port_name = find_midi_port(output_ports, 'LPD8')

    if not in_port_name:
        print("LPD8 input not found. Available ports:")
        for p in input_ports:
            print(f"  {p}")
        sys.exit(1)

    print(f"LPD8 input:  {in_port_name}")
    print(f"LPD8 output: {out_port_name or '(not found, no LED feedback)'}")

    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.1)
    except serial.SerialException as e:
        print(f"Serial error: {e}")
        sys.exit(1)

    time.sleep(2)  # Wait for ESP32 ready

    pad_states = [False] * 8
    current_mode = FREQUENCY_MODE

    def send_midi_feedback(out_port, pad_idx, on: bool):
        if out_port is None:
            return
        vel = 127 if on else 0
        out_port.send(mido.Message('note_on', channel=LPD8_CHANNEL,
                                   note=LPD8_NOTES[pad_idx], velocity=vel))

    def send_all_led_state(out_port):
        for i, state in enumerate(pad_states):
            send_midi_feedback(out_port, i, state)

    with mido.open_input(in_port_name) as inport:
        out_port = mido.open_output(out_port_name) if out_port_name else None

        # Clear all pad LEDs on startup
        if out_port:
            send_all_led_state(out_port)

        print(f"Ready — mode: {'FREQUENCY' if current_mode == FREQUENCY_MODE else 'MIDI'}")
        print("Listening for LPD8 pads...")

        for msg in inport:
            if msg.type not in ('note_on', 'note_off'):
                continue

            is_on = (msg.type == 'note_on' and msg.velocity > 0)

            if msg.note not in LPD8_NOTES:
                continue
            pad_idx = LPD8_NOTES.index(msg.note)

            if not is_on:
                continue  # Only act on note_on (toggle on press)

            # Toggle pad state
            pad_states[pad_idx] = not pad_states[pad_idx]
            velocity = 127 if pad_states[pad_idx] else 0

            # Send pad state to ESP32
            ser.write(f"PAD,{pad_idx},{velocity}\n".encode())
            print(f"Pad {pad_idx + 1}: {'ON ' if pad_states[pad_idx] else 'OFF'}")

            # Update LPD8 pad LED
            send_midi_feedback(out_port, pad_idx, pad_states[pad_idx])

            # Pad 8 toggles mode
            if pad_idx == 7:
                current_mode = MIDI_MODE if current_mode == FREQUENCY_MODE else FREQUENCY_MODE
                ser.write(f"MODE,{current_mode}\n".encode())
                print(f">>> Mode: {'MIDI' if current_mode == MIDI_MODE else 'FREQUENCY'}")


if __name__ == '__main__':
    main()
