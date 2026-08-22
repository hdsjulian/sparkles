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
import os
import traceback
import json
import socket
import threading

# ---------------------------------------------------------------------------
# CLI args
# ---------------------------------------------------------------------------
parser = argparse.ArgumentParser()
parser.add_argument('--noserial', action='store_true', help='Disable serial output')
parser.add_argument('--sock', default='/tmp/music.sock', help="Unix socket path to serial_bridge's music socket")
parser.add_argument('--api', default='http://localhost:8080', help='FastAPI base URL for midi_params and heartbeat')
parser.add_argument('--log', action='store_true', help='Enable logging to aubioAlgo.txt')
parser.add_argument('--random', action='store_true', help='Generate random pitch/RMS instead of USB mic')
parser.add_argument('--debug', action='store_true', help='DEBUG log level (writes per-frame logs to disk — adds latency)')
parser.add_argument('--list-devices', action='store_true', help='List audio input devices and exit')
parser.add_argument('--device', default=os.environ.get('SPARKLES_AUDIO_DEVICE'),
                    help='Input device index, or part of its name (env: SPARKLES_AUDIO_DEVICE). Default is the '
                         'first device with an input channel, which on a pi is often not the microphone')
parser.add_argument('--meter', action='store_true',
                    help='Print level and pitch every 200ms regardless of the silence gate, and send nothing')
parser.add_argument('--no-auto-level', dest='auto_level', action='store_false',
                    default=os.environ.get('SPARKLES_AUTO_LEVEL', '1') != '0',
                    help='Do not derive the thresholds from the room — use the values from the UI as given')
args = parser.parse_args()

logging.basicConfig(
    level=logging.DEBUG if args.debug else logging.INFO,
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
    'rmsCalibrated': False,   # set by the level test in the web interface
}

# ---------------------------------------------------------------------------
# Music bridge socket (write-only: send aubio_shimmer / aubio_midi commands)
# ---------------------------------------------------------------------------
_sock: socket.socket | None = None
_sock_lock = threading.Lock()

def _open_sock():
    global _sock
    while True:
        try:
            s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            s.connect(args.sock)
            with _sock_lock:
                _sock = s
            log.info(f"Connected to music bridge at {args.sock}")
            return
        except Exception as e:
            log.error(f"Music bridge connect failed: {e} — retrying in {RETRY_DELAY}s")
            time.sleep(RETRY_DELAY)

def serial_send(cmd_dict):
    if args.noserial or _sock is None:
        return
    try:
        with _sock_lock:
            _sock.sendall((json.dumps(cmd_dict) + "\n").encode("utf-8"))
    except Exception as e:
        log.error(f"Music bridge send failed: {e}")

# ---------------------------------------------------------------------------
# midi_params: fetch from FastAPI on startup, poll every 30 s for live updates
# ---------------------------------------------------------------------------

def _fetch_midi_params():
    import urllib.request
    try:
        resp = urllib.request.urlopen(f"{args.api}/getMidiParams", timeout=3)
        data = json.loads(resp.read())
        midi_params.update({k: v for k, v in data.items() if k in midi_params})
        midi_params['rmsCalibrated'] = bool(data.get('rmsCalibrated'))
        log.info(f"midi_params fetched: mode={midi_params.get('mode')} "
                 f"range={midi_params.get('rangeMin')}-{midi_params.get('rangeMax')} "
                 f"rms={midi_params.get('rmsMin'):.1f}..{midi_params.get('rmsMax'):.1f}")
    except Exception as e:
        log.warning(f"midi_params fetch failed: {e}")
    apply_auto_levels()   # a fetch would otherwise overwrite what the room told us

# Latest level, published to the API by one background thread. Spawning a
# thread per sample was five a second for as long as the service runs, and they
# pile up the moment the API is slow to answer.
_level_report = {'db': -96.0, 'pitch': 0.0}


def _level_reporter():
    """Publish what we are hearing so the web level test can use it.

    Nothing else can open the audio device while this process holds it, so the
    browser cannot measure for itself — it reads what we already compute.
    """
    import urllib.request
    while True:
        time.sleep(0.2)
        try:
            body = json.dumps({"db": round(float(_level_report['db']), 2),
                               "pitch": round(float(_level_report['pitch']), 1)}).encode()
            req = urllib.request.Request(f"{args.api}/internal/aubio_level", data=body,
                                         headers={"Content-Type": "application/json"})
            urllib.request.urlopen(req, timeout=1).read()
        except Exception:
            pass   # best effort: the test is a convenience, never worth a stall


def _poll_midi_params():
    # Wait for FastAPI to be ready, then keep params fresh
    for _ in range(20):
        try:
            _fetch_midi_params()
            break
        except Exception:
            time.sleep(5)
    while True:
        time.sleep(30)
        _fetch_midi_params()

# The master keeps its idle animation suppressed straight off the music stream
# (each aubio_shimmer/aubio_midi refreshes its lastMidiTime), so no heartbeat needed.

if not args.noserial:
    _open_sock()
    threading.Thread(target=_level_reporter, daemon=True).start()
    threading.Thread(target=_poll_midi_params, daemon=True).start()

# ---------------------------------------------------------------------------
# Audio setup
# ---------------------------------------------------------------------------
input_channels = 1   # set by open_audio; >1 means the read path deinterleaves

# Recognising the input by name rather than taking whatever enumerates first.
# Interfaces and USB microphones score positively; the pi's own pseudo-devices
# and loopbacks score negatively — several of those report input channels and
# would happily be selected while returning silence forever.
DEVICE_PREFERRED = ('scarlett', 'focusrite', 'shure', 'rode', 'zoom', 'behringer', 'audio-technica',
                    'yeti', 'snowball', 'samson', 'presonus', 'motu', 'steinberg', 'usb audio',
                    'usb pnp', 'microphone', 'mic')
DEVICE_AVOIDED = ('dummy', 'null', 'loopback', 'monitor', 'dmix', 'surround', 'hdmi',
                  'bcm2835', 'default', 'sysdefault', 'pulse', 'jack')


def score_device(name):
    low = name.lower()
    score = 0
    for i, token in enumerate(DEVICE_PREFERRED):
        if token in low:
            score += 100 - i        # earlier in the list is a stronger claim
            break
    for token in DEVICE_AVOIDED:
        if token in low:
            score -= 100
            break
    return score


def pick_device(inputs, want):
    """inputs: [(index, name)]. Explicit choice wins, else the best-scoring one."""
    if want:
        if want.isdigit():
            for i, _ in inputs:
                if i == int(want):
                    return i, "index given"
        for i, name in inputs:
            if want.lower() in name.lower():
                return i, "name given"
        # asked for something absent — fall through to automatic rather than
        # retry forever on a name that may never appear
    if not inputs:
        return None, "nothing available"
    best = max(inputs, key=lambda entry: (score_device(entry[1]), -entry[0]))
    return best[0], f"auto, score {score_device(best[1])}"


def list_devices():
    p = pyaudio.PyAudio()
    print("input devices:")
    for i in range(p.get_device_count()):
        info = p.get_device_info_by_index(i)
        if info['maxInputChannels'] > 0:
            print(f"  [{i}] {info['name']}  channels={info['maxInputChannels']} "
                  f"rate={int(info['defaultSampleRate'])}")
    p.terminate()


CALIBRATION_SECONDS = 1.5   # how long to listen to the room
CALIBRATION_TIMEOUT = 4.0   # hard ceiling: never hang the startup on a silent device
AUTO_MARGIN_DB = 8.0    # how far above the room the gate sits
AUTO_SPAN_DB   = 25.0   # assumed vocal range until something louder is heard
AUTO_MIN_FLOOR = -75.0  # a silent digital input reads absurdly low; do not trust it
AUTO_MIN_CEIL  = -25.0

# Derived from the room rather than configured. The thresholds that matter are
# relative to whatever the chain actually delivers — swapping a USB mic for an
# interface and a dynamic mic moves the whole scale, and a number tuned for one
# is meaningless for the other.
auto_levels = {'min': None, 'max': None}


def calibrate_levels(stream):
    """Listen to an empty room for a moment and put the gate just above it.

    Bounded by the clock, not by a frame count. stream.read() blocks until the
    device delivers, so a device that is open but not streaming — claimed by
    something else, or a rate the interface will not actually run — would hang
    here for ever, before the detection loop ever starts. The only sign of that
    from outside is the midi_params poll still logging on its own thread, which
    looks exactly like a healthy process.
    """
    log.info("measuring the room for %.1fs...", CALIBRATION_SECONDS)
    deadline = time.time() + CALIBRATION_TIMEOUT
    frames, floor_samples = int(CALIBRATION_SECONDS * samplerate / hop_s), []
    for _ in range(frames):
        if time.time() > deadline:
            log.warning("level calibration timed out after %.1fs with %d frame(s) — "
                        "the device is open but not delivering audio",
                        CALIBRATION_TIMEOUT, len(floor_samples))
            break
        try:
            data = stream.read(hop_s, exception_on_overflow=False)
        except Exception as exc:
            log.warning("level calibration read failed: %s", exc)
            return
        block = np.frombuffer(data, dtype=aubio.float_type)
        if input_channels > 1:
            block = block[0::input_channels]
        rms = np.sqrt(np.mean(block ** 2))
        floor_samples.append(20 * np.log10(rms) if rms > 0 else -96.0)
    if not floor_samples:
        log.warning("no audio during calibration — leaving the thresholds alone")
        return
    floor_samples.sort()
    noise = floor_samples[len(floor_samples) // 2]          # median, ignores a cough
    gate  = min(AUTO_MIN_CEIL, max(AUTO_MIN_FLOOR, noise + AUTO_MARGIN_DB))
    auto_levels['min'] = gate
    auto_levels['max'] = gate + AUTO_SPAN_DB
    log.info("levels from the room: noise %.1f dB -> gate %.1f dB, top %.1f dB "
             "(raises itself if you sing louder)", noise, gate, auto_levels['max'])
    apply_auto_levels()


def apply_auto_levels():
    """Auto values win over the fetched ones — unless someone has measured.

    rmsCalibrated comes from the level test in the web interface. A deliberate
    measurement beats a guess from an empty room, and without this check the
    calibration would be overwritten within a second of being set.
    """
    if midi_params.get('rmsCalibrated'):
        return
    if not args.auto_level or auto_levels['min'] is None:
        return
    midi_params['rmsMin'] = auto_levels['min']
    midi_params['rmsMax'] = auto_levels['max']


def _sound_cards():
    """Fingerprint of the sound hardware, so a plug or unplug is noticed.

    Reading a small file every few seconds is nearly free, where re-enumerating
    through portaudio is not — and it catches a device appearing, which is the
    case a read error never will: a working stream throws nothing just because
    something better got plugged in next to it.
    """
    try:
        with open("/proc/asound/cards") as f:
            return f.read()
    except OSError:
        return ""


def reopen_audio(p, stream, why):
    log.info("reopening audio: %s", why)
    try:
        stream.stop_stream()
        stream.close()
        p.terminate()
    except Exception:
        pass
    return open_audio()


def open_audio():
    while True:
        try:
            p = pyaudio.PyAudio()
            inputs = []
            for i in range(p.get_device_count()):
                info = p.get_device_info_by_index(i)
                if info['maxInputChannels'] > 0:
                    inputs.append((i, info['name']))
            log.info("input devices: %s",
                     ", ".join(f"[{i}] {n} ({score_device(n):+d})" for i, n in inputs) or "none")
            selected_index, why = pick_device(inputs, args.device)
            if selected_index is None:
                raise RuntimeError("No input audio device found")
            log.info("chose [%d] %s — %s", selected_index,
                     dict(inputs)[selected_index], why)
            log.info(f"Opening audio on device {selected_index}: {p.get_device_info_by_index(selected_index)['name']}")
            # Interfaces with more than one input (the Scarlett Solo has two)
            # sometimes refuse a mono open outright. Fall back to the device's
            # own channel count and take the first channel in the read path.
            global input_channels
            want = p.get_device_info_by_index(selected_index)['maxInputChannels']
            for channels in (1, int(want)):
                try:
                    stream = p.open(format=pyaudio.paFloat32,
                                    channels=channels, rate=44100, input=True,
                                    input_device_index=selected_index, frames_per_buffer=256)
                    input_channels = channels
                    log.info("Audio stream opened, %d channel(s)", channels)
                    if args.auto_level:
                        calibrate_levels(stream)
                    return p, stream
                except Exception as chan_err:
                    log.warning("open with %d channel(s) failed: %s", channels, chan_err)
                    if channels == int(want):
                        raise
        except Exception as e:
            log.error(f"Audio open failed: {e}\n{traceback.format_exc()}")
            try:
                p.terminate()
            except Exception:
                pass
            log.info(f"Retrying audio in {RETRY_DELAY}s...")
            time.sleep(RETRY_DELAY)

# Defined before the first open_audio(): calibrate_levels() runs inside it and
# needs the frame size, so leaving these below the call was a NameError waiting
# for the first boot.
samplerate = 44100
win_s = 1024
hop_s = 256
tolerance = 0.8

DB_SILENCE = -50.0

pitch_o = pitch("yinfft", win_s, hop_s, samplerate)
pitch_o.set_tolerance(tolerance)

if args.list_devices:
    list_devices()
    sys.exit(0)

p, stream = open_audio()

# ---------------------------------------------------------------------------
# Aubio pitch detection
# ---------------------------------------------------------------------------

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
            serial_send({"cmd": "aubio_shimmer", "hue": hue_start, "saturation": 255, "value": 0, "scale": 0.0})
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
        # scale = 0..1 pitch position — the bridge maps it onto the configured color
        serial_send({"cmd": "aubio_shimmer", "hue": hue, "saturation": saturation, "value": value, "scale": round(scale, 3)})
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

    # instrumentation to find intermittent lag: how far behind real time the
    # loop gets (backlog) and how long per-hop processing takes
    last_backlog_warn  = 0.0
    max_backlog_frames = 0
    max_work_ms        = 0.0
    last_stat          = start_time
    peak_db            = -96.0   # loudest frame since the last stats line
    last_meter         = 0.0
    cards              = _sound_cards()   # hardware as it was when we opened
    last_card_check    = 0.0
    floor_since        = time.time()      # since when the input has been silent

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
            backlog = stream.get_read_available()
            if backlog > max_backlog_frames:
                max_backlog_frames = backlog
            if backlog > hop_s * 4 and time.time() - last_backlog_warn > 1.0:
                log.warning("audio backlog %d frames, ~%.0f ms behind real time",
                            backlog, backlog / samplerate * 1000.0)
                last_backlog_warn = time.time()
            try:
                data = stream.read(hop_s, exception_on_overflow=False)
            except Exception as e:
                log.error(f"Microphone read failed: {e}\n{traceback.format_exc()}")
                p, stream = reopen_audio(p, stream, "read failed")
                cards, floor_since = _sound_cards(), time.time()
                continue
            work_t0   = time.perf_counter()
            samples   = np.frombuffer(data, dtype=aubio.float_type)
            if input_channels > 1:
                samples = samples[0::input_channels].copy()  # channel 1, contiguous for aubio
            pitch_val = pitch_o(samples)[0]
            rms       = np.sqrt(np.mean(samples ** 2))
            db        = 20 * np.log10(rms) if rms > 0 else -96.0
            work_ms   = (time.perf_counter() - work_t0) * 1000.0
            if work_ms > max_work_ms:
                max_work_ms = work_ms

        if db > peak_db:
            peak_db = db

        # Something plugged in or pulled out: re-pick the input, which may now be
        # a better one than the stream currently open.
        if not args.random and time.time() - last_card_check > 3.0:
            last_card_check = time.time()
            now_cards = _sound_cards()
            if now_cards != cards:
                cards = now_cards
                p, stream = reopen_audio(p, stream, "sound hardware changed")
                floor_since = time.time()
                continue

        # A device can vanish without the read ever failing — it just returns
        # silence for ever, which is indistinguishable from a dead microphone.
        if db > -90.0:
            floor_since = time.time()
        elif not args.random and time.time() - floor_since > 30.0:
            p, stream = reopen_audio(p, stream, "30s of digital silence")
            cards, floor_since = _sound_cards(), time.time()
            continue

        # --meter: what the microphone is doing, ahead of every gate, so a level
        # too low to pass them is still visible
        if time.time() - last_meter >= 0.2:
            last_meter = time.time()
            if args.meter:
                bar = "#" * max(0, min(40, int((db + 90) / 2)))
                print(f"{db:7.1f} dB  {pitch_val:7.1f} Hz  {bar}", flush=True)
            _level_report['db'], _level_report['pitch'] = db, pitch_val

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

            # DB_SILENCE is a fixed floor that predates auto levelling; once the
            # gate is derived from the room, that is the number to respect,
            # otherwise a quiet chain is blocked by a constant nobody can reach
            # from the UI.
            gate = auto_levels['min'] if (args.auto_level and auto_levels['min'] is not None) else DB_SILENCE
            if args.meter:
                pass  # measuring only, nothing goes to the fleet
            elif avg_db > gate:
                log.debug(f"Pitch={avg_pitch:.1f}Hz  dB={avg_db:.1f}")
                mode = midi_params['mode']
                if mode == FREQUENCY_MODE:
                    output_frequency(avg_pitch, avg_db)
                elif mode == MIDI_MODE:
                    output_midi(avg_pitch, avg_db)

                if args.log:
                    with open("aubioAlgo.txt", "a") as f:
                        f.write(f"{avg_pitch:.2f},{avg_db:.2f}\n")

            # The top of the range cannot be measured from an empty room, so it
            # starts as an assumption and corrects itself upward the first time
            # someone actually sings.
            if (args.auto_level and auto_levels['max'] is not None
                    and avg_db > auto_levels['max']):
                auto_levels['max'] = avg_db
                apply_auto_levels()
                log.info("heard %.1f dB, raising the top of the range", avg_db)

            buffer_pitch.clear()
            buffer_rms.clear()
            last_output = now

        if now - last_stat >= 2.0:
            # peak level against the gate that has to be cleared before anything
            # is sent — without it a silent or too-quiet input looks identical to
            # a healthy one in the log
            log.info("loop stats: peak %.1f dB (gate %.1f, mode %s), backlog<=%.0f ms, "
                     "work<=%.1f ms (hop budget %.1f ms)",
                     peak_db, midi_params['rmsMin'], midi_params['mode'],
                     max_backlog_frames / samplerate * 1000.0, max_work_ms,
                     hop_s / samplerate * 1000.0)
            max_backlog_frames = 0
            max_work_ms        = 0.0
            peak_db            = -96.0
            last_stat          = now

if __name__ == '__main__':
    get_current_note()
