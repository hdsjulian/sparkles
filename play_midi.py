#!/usr/bin/env python3
"""
Play a MIDI file live to a MIDI output, optionally filtering channels — without
converting/writing a new file. Useful for piano karaoke: play only the piano
channel(s) of a full arrangement straight to the keyboard.

Usage:
  python3 play_midi.py song.mid                      # play all channels
  python3 play_midi.py --channels 0 song.mid         # only channel 0 (piano)
  python3 play_midi.py --channels 0,3 song.mid       # piano + bass
  python3 play_midi.py --port NC2 --channels 0 song.mid
  python3 play_midi.py --list                        # list output ports

Channels are 0-indexed, matching `piano_only.py --inspect`.
The MIDI port must be free, so stop the keyboard service first:
  sudo systemctl stop keyboard_midi
Run with the venv that has mido:
  ~/myenv/bin/python3 play_midi.py ...
Ctrl-C to stop (sends an all-notes-off first).
"""

import argparse
import sys

import mido

DEFAULT_PORT_MATCH = "NC2"   # substring of the keyboard's output port name


def pick_port(match: str):
    names = mido.get_output_names()
    for n in names:
        if match.lower() in n.lower():
            return n
    return None


def main():
    ap = argparse.ArgumentParser(description="Play a MIDI file live, filtering channels.")
    ap.add_argument("file", nargs="?", help="MIDI file to play")
    ap.add_argument("--channels", help="comma-separated channels to play (0-indexed); default all")
    ap.add_argument("--port", default=DEFAULT_PORT_MATCH,
                    help=f"output port name substring (default: {DEFAULT_PORT_MATCH})")
    ap.add_argument("--list", action="store_true", help="list MIDI output ports and exit")
    args = ap.parse_args()

    if args.list:
        for n in mido.get_output_names():
            print(n)
        return
    if not args.file:
        ap.error("a MIDI file is required (or use --list)")

    keep = {int(c) for c in args.channels.split(",") if c.strip() != ""} if args.channels else None

    name = pick_port(args.port)
    if not name:
        print(f"No output port matching '{args.port}'. Available:", file=sys.stderr)
        for n in mido.get_output_names():
            print(f"  {n}", file=sys.stderr)
        print("(is keyboard_midi still holding the port? sudo systemctl stop keyboard_midi)",
              file=sys.stderr)
        sys.exit(1)

    out = mido.open_output(name)
    print(f"Playing {args.file} -> {name}" + (f"  channels={sorted(keep)}" if keep else "  (all channels)"))
    try:
        for msg in mido.MidiFile(args.file).play():     # real-time timing, skips meta
            if keep is not None and hasattr(msg, "channel") and msg.channel not in keep:
                continue
            out.send(msg)
    except KeyboardInterrupt:
        print("\nstopped")
    finally:
        for ch in range(16):                            # all notes off, so nothing hangs
            out.send(mido.Message("control_change", channel=ch, control=123, value=0))
        out.close()


if __name__ == "__main__":
    main()
