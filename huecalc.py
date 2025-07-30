import math

def calc_hue(pitch, min_pitch, max_pitch, hue_start, hue_end, exponent=1.0):
    """
    Calculate hue for a given pitch using a logarithmic scale.
    Optionally steepen the curve with exponent < 1.
    """
    if pitch < min_pitch:
        return hue_start
    if pitch > max_pitch:
        return hue_end
    scale = math.log2(pitch / min_pitch) / math.log2(max_pitch / min_pitch)
    scale = math.pow(scale, exponent)
    hue = hue_start + (hue_end - hue_start) * scale
    return hue

if __name__ == "__main__":
    # Example usage
    min_pitch = 100
    max_pitch = 1000
    hue_start = 0
    hue_end = 160
    exponent = 0.5  # Steeper at low end
    for pitch in [100, 150, 200, 300, 400, 600, 800, 1000]:
        hue = calc_hue(pitch, min_pitch, max_pitch, hue_start, hue_end, exponent)
        print(f"Pitch: {pitch} Hz -> Hue: {hue:.2f}")
