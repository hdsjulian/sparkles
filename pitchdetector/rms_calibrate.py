import pyaudio
import numpy as np

SAMPLERATE = 44100
RECORD_SECONDS = 3
FRAMES_PER_BUFFER = 1024
NUM_STEPS = 7

p = pyaudio.PyAudio()

input_device_index = None
for i in range(p.get_device_count()):
    info = p.get_device_info_by_index(i)
    if info['maxInputChannels'] > 0:
        input_device_index = i
        print(f"Using device {i}: {info['name']}")
        break

if input_device_index is None:
    raise RuntimeError("No input audio device found")

stream = p.open(
    format=pyaudio.paFloat32,
    channels=1,
    rate=SAMPLERATE,
    input=True,
    input_device_index=input_device_index,
    frames_per_buffer=FRAMES_PER_BUFFER,
)


def record_rms(label):
    input(f"\n>> Step {label}/{NUM_STEPS} — press ENTER then sing for {RECORD_SECONDS}s...")
    print("   Recording...")
    avail = stream.get_read_available()
    if avail > 0:
        stream.read(avail, exception_on_overflow=False)

    frames = []
    for _ in range(int(SAMPLERATE / FRAMES_PER_BUFFER * RECORD_SECONDS)):
        data = stream.read(FRAMES_PER_BUFFER, exception_on_overflow=False)
        frames.append(np.frombuffer(data, dtype=np.float32))

    audio = np.concatenate(frames)
    rms = float(np.sqrt(np.mean(audio ** 2)))
    db = 20 * np.log10(rms) if rms > 0 else -np.inf
    print(f"   RMS: {rms:.5f}  |  dB: {db:.2f}")
    return rms, db


print("\n=== Voice Linearity Calibration ===")
print(f"Sing a sustained note at {NUM_STEPS} evenly-spaced perceived volumes,")
print("from barely audible (1) to as loud as you can (7).\n")

steps = list(range(1, NUM_STEPS + 1))
rms_vals = []
db_vals = []

for step in steps:
    rms, db = record_rms(step)
    rms_vals.append(rms)
    db_vals.append(db)

stream.stop_stream()
stream.close()
p.terminate()

print("\n=== Results ===")
print(f"{'Step':<6} {'RMS':>10} {'dB':>8} {'RMS ratio':>11} {'dB step':>9}")
print("-" * 48)
for i, step in enumerate(steps):
    rms_ratio = f"{rms_vals[i] / rms_vals[i-1]:.3f}" if i > 0 else "—"
    db_step   = f"{db_vals[i] - db_vals[i-1]:.2f}" if i > 0 else "—"
    print(f"{step:<6} {rms_vals[i]:>10.5f} {db_vals[i]:>8.2f} {rms_ratio:>11} {db_step:>9}")

# Plot
try:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(10, 4))
    fig.suptitle("Voice loudness linearity — perceived steps 1–7")

    ax1.plot(steps, rms_vals, marker='o', color='steelblue')
    ax1.set_title("RMS (linear amplitude)")
    ax1.set_xlabel("Perceived step")
    ax1.set_ylabel("RMS")
    ax1.grid(True)

    ax2.plot(steps, db_vals, marker='o', color='tomato')
    ax2.set_title("dB (logarithmic)")
    ax2.set_xlabel("Perceived step")
    ax2.set_ylabel("dB")
    ax2.grid(True)

    plt.tight_layout()
    out = "rms_linearity.png"
    plt.savefig(out, dpi=150)
    print(f"\nPlot saved to {out}")
except ImportError:
    print("\nmatplotlib not installed — skipping plot (pip install matplotlib)")
