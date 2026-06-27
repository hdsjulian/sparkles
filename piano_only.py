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
  python3 piano_only.py --to-piano 1 song.mid    # also fold channel 1 (e.g. a sax
                                                 #   melody) in, played as piano

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


def pianofy(src: str, dst: str, keep_channels=None, drop_channels=None, to_piano=None):
    to_piano = to_piano or set()
    mid = mido.MidiFile(src)
    out = mido.MidiFile(ticks_per_beat=mid.ticks_per_beat)
    track = mido.MidiTrack()
    out.tracks.append(track)

    # force the converted channels to acoustic grand piano up front
    for ch in sorted(to_piano):
        track.append(mido.Message("program_change", channel=ch, program=0, time=0))

    program = {}   # channel -> last program number
    carry = 0      # delta-time of dropped messages, rolled into the next kept one
    for msg in mido.merge_tracks(mid.tracks):
        ch = getattr(msg, "channel", None)
        if msg.type == "program_change":
            program[msg.channel] = msg.program
            if msg.channel in to_piano:
                carry += msg.time                      # drop it; already forced to piano
                continue

        if msg.is_meta:
            keep = msg.type != "track_name"            # keep tempo/time-sig, drop names
        elif ch is None:
            keep = True
        elif ch in to_piano:
            keep = True                                # converted channels are always kept
        elif keep_channels is not None:
            keep = ch in keep_channels                 # explicit channel allow-list
        elif drop_channels is not None:
            keep = ch not in drop_channels             # explicit channel deny-list
        else:
            # default: piano-program channels only, no drums
            # (channels with no program default to 0 = piano, per GM)
            keep = ch != DRUM_CHANNEL and program.get(ch, 0) in PIANO_PROGRAMS

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
    ap.add_argument("--keep-channels",
                    help="comma-separated channels to keep, e.g. 0,3 (overrides piano filter)")
    ap.add_argument("--drop-channels",
                    help="comma-separated channels to drop, keep the rest")
    ap.add_argument("--to-piano",
                    help="comma-separated channels to KEEP and convert to piano "
                         "(e.g. fold a melody on a sax patch into the piano)")
    args = ap.parse_args()

    def parse(s):
        return {int(x) for x in s.split(",") if x.strip() != ""} if s else None

    keep = parse(args.keep_channels)
    drop = parse(args.drop_channels)
    to_piano = parse(args.to_piano)

    for f in args.files:
        try:
            if args.inspect:
                inspect(f)
            else:
                pianofy(f, f.rsplit(".", 1)[0] + "_piano.mid",
                        keep_channels=keep, drop_channels=drop, to_piano=to_piano)
        except Exception as e:
            print(f"{f}: error: {e}", file=sys.stderr)


if __name__ == "__main__":
    main()
