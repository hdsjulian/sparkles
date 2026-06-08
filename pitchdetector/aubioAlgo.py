import sys
import aubio
from aubio import pitch
import queue
import music21
import pyaudio
import numpy as np
import struct
import math
import argparse
import time
import logging
import traceback
import json
import serial
import threading

# ---------------------------------------------------------------------------
# CLI args
# ---------------------------------------------------------------------------
parser = argparse.ArgumentParser()
parser.add_argument('--noserial', action='store_true', help='Disable serial output')
parser.add_argument('--port', default='/dev/ttyACM0', help='Serial port to master ESP32')
parser.add_argument('--baud', default=115200, type=int, help='Serial baud rate')
parser.add_argument('--log', action='store_true', help='Enable logging to aubioAlgo.txt')
parser.add_argument('--random', action='store_true', help='Generate random pitch/RMS instead of USB mic')
args = parser.parse_args()

logging.basicConfig(
    level=logging.DEBUG,
    format='%(asctime)s %(levelname)s %(message)s',
    handlers=[
        logging.FileHandler('aubioAlgo.log'),
        logging.StreamHandler(sys.stdout),
    ]
)
log = logging.getLogger(__name__)
log.info("aubioAlgo starting")

RETRY_DELAY = 5

# ---------------------------------------------------------------------------
# Mode constants (mirror of MyDefines.h)
# ---------------------------------------------------------------------------
FREQUENCY_MODE = 2
MIDI_MODE      = 1

# ---------------------------------------------------------------------------
# Mutable midi params (updated live from MSG_MIDI_PARAMS received over air)
# Mirrors midiParams defaults set in Raspi-Device setup()
# ---------------------------------------------------------------------------
midi_params = {
    'valMin': 0, 'valMax': 255,
    'satMin': 127, 'satMax': 255,
    'hue': 22,
    'rangeMin': 100, 'rangeMax': 1000,
    'rmsMin': -38.0, 'rmsMax': -20.0,
    'mode': FREQUENCY_MODE,
}

# ---------------------------------------------------------------------------
# Serial setup
# ---------------------------------------------------------------------------
ser = None
if not args.noserial:
    def _open_serial():
        global ser
        while True:
            try:
                ser = serial.Serial(args.port, args.baud, timeout=1)
                log.info(f"Serial opened on {args.port} at {args.baud} baud")
                return
            except Exception as e:
                log.error(f"Serial open failed: {e} — retrying in {RETRY_DELAY}s")
                time.sleep(RETRY_DELAY)

    def _serial_reader():
        """Read incoming JSON lines from master (e.g. midi_params updates)."""
        while True:
            try:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if not line:
                    continue
                data = json.loads(line)
                if data.get('event') == 'midi_params':
                    midi_params.update({k: v for k, v in data.items() if k != 'event'})
                    log.info(f"[MIDI_PARAMS] updated via serial: mode={midi_params.get('mode')} "
                             f"range={midi_params.get('rangeMin')}-{midi_params.get('rangeMax')} "
                             f"rms={midi_params.get('rmsMin'):.1f}..{midi_params.get('rmsMax'):.1f}")
            except json.JSONDecodeError:
                pass
            except Exception as e:
                log.error(f"Serial read error: {e}")
                time.sleep(1)

    _open_serial()
    threading.Thread(target=_serial_reader, daemon=True).start()

def serial_send(cmd_dict):
    if ser is None:
        return
    try:
        ser.write((json.dumps(cmd_dict) + '\n').encode('utf-8'))
    except Exception as e:
        log.error(f"Serial write failed: {e}")

# ---------------------------------------------------------------------------
# Audio setup
# ---------------------------------------------------------------------------
def open_audio():
    while True:
        try:
            p = pyaudio.PyAudio()
            selected_index = None
            for i in range(p.get_device_count()):
                info = p.get_device_info_by_index(i)
                log.debug(f"Audio device {i}: {info['name']} | in={info['maxInputChannels']} rate={info['defaultSampleRate']}")
                if info['maxInputChannels'] > 0 and selected_index is None:
                    selected_index = i
            if selected_index is None:
                raise RuntimeError("No input audio device found")
            log.info(f"Opening audio on device {selected_index}: {p.get_device_info_by_index(selected_index)['name']}")
            stream = p.open(format=pyaudio.paFloat32,
                            channels=1, rate=44100, input=True,
                            input_device_index=selected_index, frames_per_buffer=256)
            log.info("Audio stream opened")
            return p, stream
        except Exception as e:
            log.error(f"Audio open failed: {e}\n{traceback.format_exc()}")
            try:
                p.terminate()
            except Exception:
                pass
            log.info(f"Retrying audio in {RETRY_DELAY}s...")
            time.sleep(RETRY_DELAY)

p, stream = open_audio()

# ---------------------------------------------------------------------------
# Aubio pitch detection
# ---------------------------------------------------------------------------
samplerate = 44100
win_s = 1024
hop_s = 256
tolerance = 0.8

DB_SILENCE = -50.0

pitch_o = pitch("yinfft", win_s, hop_s, samplerate)
pitch_o.set_tolerance(tolerance)

# ---------------------------------------------------------------------------
# Output functions — ported from Raspi-Device/main.cpp
# ---------------------------------------------------------------------------
_last_midi_note    = -1
_shimmer_on        = False
_last_pitch        = 0.0
_last_sent_shimmer = 255   # invalid sentinel so first send always goes
_last_shimmer_ms   = 0.0
SHIMMER_RESEND_INTERVAL_MS = 300

def output_midi(pitch_val, db):
    global _last_midi_note
    if pitch_val <= 0:
        return
    p = midi_params
    min_db = p['rmsMin']
    max_db = p['rmsMax']

    midi_note = round(69 + 12 * math.log2(pitch_val / 440.0))
    midi_note = max(0, min(127, midi_note))

    if midi_note == _last_midi_note:
        return
    _last_midi_note = midi_note

    if db <= min_db:
        velocity = 0
    elif db >= max_db:
        velocity = 127
    else:
        velocity = int(127.0 * (db - min_db) / (max_db - min_db))

    log.debug(f"MIDI note={midi_note} velocity={velocity}")
    serial_send({"cmd": "aubio_midi", "note": midi_note, "velocity": velocity})

def output_frequency(pitch_val, db):
    global _shimmer_on, _last_pitch, _last_sent_shimmer, _last_shimmer_ms
    p = midi_params
    min_pitch = p['rangeMin'] if p['rangeMin'] > 0 else 100.0
    max_pitch = p['rangeMax'] if p['rangeMax'] > 0 else 1000.0
    val_min   = p['valMin']
    val_max   = p['valMax']
    min_db    = p['rmsMin']
    max_db    = p['rmsMax']
    hue_start = 22
    hue_end   = 8

    if db < min_db:
        if _shimmer_on:
            _shimmer_on = False
            _last_sent_shimmer = 0
            _last_shimmer_ms = time.time() * 1000
            log.debug("Shimmer off")
            serial_send({"cmd": "aubio_shimmer", "hue": hue_start, "saturation": 255, "value": 0})
        return

    # Only recalculate hue if pitch changed by more than a quarter-note interval
    send_shimmer = False
    quarter_note_ratio = 2.0 ** (1.0 / 48.0)
    if _last_pitch > 0 and pitch_val > 0:
        ratio = pitch_val / _last_pitch
        if ratio > quarter_note_ratio or ratio < 1.0 / quarter_note_ratio:
            send_shimmer = True
    else:
        send_shimmer = True
    _last_pitch = pitch_val

    # Map pitch → hue (log scale)
    if min_pitch < pitch_val <= max_pitch:
        scale = math.log2(pitch_val / min_pitch) / math.log2(max_pitch / min_pitch)
        hue = int(hue_start + (hue_end - hue_start) * scale)
    elif pitch_val > max_pitch:
        hue = hue_end
        scale = 1.0
    else:
        hue = hue_start
        scale = 0.0

    # Map pitch → saturation (inverse log scale)
    if min_pitch < pitch_val <= max_pitch:
        saturation = int(255.0 * (1.0 - scale) + 0.5)
    elif pitch_val > max_pitch:
        saturation = 0
    else:
        saturation = 255

    # Map db → value
    db_clamped = max(min_db, min(max_db, db))
    denom = max_db - min_db
    norm = (db_clamped - min_db) / denom if denom > 0 else 0.0
    value = int(val_min + norm * (val_max - val_min) + 0.5)

    _shimmer_on = True
    now_ms = time.time() * 1000
    refresh_due  = (value == _last_sent_shimmer) and (now_ms - _last_shimmer_ms >= SHIMMER_RESEND_INTERVAL_MS)
    denom_v = _last_sent_shimmer if _last_sent_shimmer != 0 else 1
    volume_change_due = abs(value - _last_sent_shimmer) / denom_v > 0.10

    if send_shimmer or refresh_due or volume_change_due:
        log.debug(f"Shimmer hue={hue} sat={saturation} val={value}")
        serial_send({"cmd": "aubio_shimmer", "hue": hue, "saturation": saturation, "value": value})
        _last_sent_shimmer = value
        _last_shimmer_ms = now_ms

# ---------------------------------------------------------------------------
# Main loop
# ---------------------------------------------------------------------------
def get_current_note():
    global p, stream
    import random

    buffer_pitch = []
    buffer_rms   = []
    start_time   = time.time()
    last_output  = start_time

    while True:
        if args.random:
            pitch_val  = random.uniform(50, 1000)
            if random.random() < 0.6:
                rms = 0.0
                db  = -96.0
            else:
                rms = random.uniform(0.01, 1.0)
                db  = 20 * np.log10(rms)
        else:
            try:
                data = stream.read(hop_s, exception_on_overflow=False)
            except Exception as e:
                log.error(f"Microphone read failed: {e}\n{traceback.format_exc()}")
                try:
                    stream.stop_stream()
                    stream.close()
                    p.terminate()
                except Exception:
                    pass
                p, stream = open_audio()
                continue
            samples   = np.frombuffer(data, dtype=aubio.float_type)
            pitch_val = pitch_o(samples)[0]
            rms       = np.sqrt(np.mean(samples ** 2))
            db        = 20 * np.log10(rms) if rms > 0 else -96.0

        if pitch_val > 0:
            buffer_pitch.append(pitch_val)
            buffer_rms.append(db)

        now = time.time()
        if (now - last_output) >= (1.0 / 30.0):
            if buffer_pitch:
                avg_pitch = sum(buffer_pitch) / len(buffer_pitch)
                avg_db    = sum(buffer_rms)   / len(buffer_rms)
            else:
                avg_pitch = 0.0
                avg_db    = -96.0

            if avg_db > DB_SILENCE:
                log.debug(f"Pitch={avg_pitch:.1f}Hz  dB={avg_db:.1f}")
                mode = midi_params['mode']
                if mode == FREQUENCY_MODE:
                    output_frequency(avg_pitch, avg_db)
                elif mode == MIDI_MODE:
                    output_midi(avg_pitch, avg_db)

                if args.log:
                    with open("aubioAlgo.txt", "a") as f:
                        f.write(f"{avg_pitch:.2f},{avg_db:.2f}\n")

            buffer_pitch.clear()
            buffer_rms.clear()
            last_output = now

if __name__ == '__main__':
    get_current_note()
