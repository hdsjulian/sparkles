#!/usr/bin/env python3
"""
Strip a MIDI file down to just the piano part, for piano karaoke.

Full arrangements (strings, bass, drums, ...) all get dumped onto the stage
piano's single sound and turn to mush. This keeps only piano-program channels
(GM 0-7) and drops the drum channel, preserving tempo/time signature.

Usage:
  python3 piano_only.py song.mid                 # -> song_piano.mid
  python3 piano_only.py songs/*.mid              # batch
  python3 piano_only.py --inspect song.mid       # list tracks/channels/instruments

Needs mido (it's in the keyboard_midi venv):
  ~/myenv/bin/python3 piano_only.py ...
"""

import argparse
import sys

import mido

PIANO_PROGRAMS = set(range(0, 8))   # GM acoustic + electric pianos
DRUM_CHANNEL = 9                    # GM percussion (0-indexed channel 10)


def inspect(path: str):
    mid = mido.MidiFile(path)
    print(f"{path}  ({mid.ticks_per_beat} ticks/beat, {len(mid.tracks)} tracks)")
    for i, tr in enumerate(mid.tracks):
        name = next((m.name for m in tr if m.type == "track_name"), "")
        progs = sorted({m.program for m in tr if m.type == "program_change"})
        chans = sorted({m.channel for m in tr if hasattr(m, "channel")})
        notes = sum(1 for m in tr if m.type == "note_on" and m.velocity > 0)
        print(f"  track {i:2}  ch={chans} prog={progs} notes={notes}  {name}")


def pianofy(src: str, dst: str):
    mid = mido.MidiFile(src)
    out = mido.MidiFile(ticks_per_beat=mid.ticks_per_beat)
    track = mido.MidiTrack()
    out.tracks.append(track)

    program = {}   # channel -> last program number
    carry = 0      # delta-time of dropped messages, rolled into the next kept one
    for msg in mido.merge_tracks(mid.tracks):
        if msg.is_meta:
            keep = msg.type != "track_name"            # keep tempo/time-sig, drop names
        elif msg.type == "program_change":
            program[msg.channel] = msg.program
            keep = msg.channel != DRUM_CHANNEL and msg.program in PIANO_PROGRAMS
        elif hasattr(msg, "channel"):
            # channels with no program default to 0 (piano), per GM
            keep = msg.channel != DRUM_CHANNEL and program.get(msg.channel, 0) in PIANO_PROGRAMS
        else:
            keep = True

        if keep:
            track.append(msg.copy(time=msg.time + carry))
            carry = 0
        else:
            carry += msg.time

    out.save(dst)
    notes = sum(1 for m in track if m.type == "note_on" and m.velocity > 0)
    print(f"{src} -> {dst}  ({notes} piano notes)")
    if notes == 0:
        print("  warning: no piano notes kept — this file may have no piano part")


def main():
    ap = argparse.ArgumentParser(description="Strip a MIDI file to just the piano part.")
    ap.add_argument("files", nargs="+", help="MIDI file(s) to process")
    ap.add_argument("--inspect", action="store_true",
                    help="list tracks/channels/instruments instead of filtering")
    args = ap.parse_args()

    for f in args.files:
        try:
            if args.inspect:
                inspect(f)
            else:
                pianofy(f, f.rsplit(".", 1)[0] + "_piano.mid")
        except Exception as e:
            print(f"{f}: error: {e}", file=sys.stderr)


if __name__ == "__main__":
    main()
