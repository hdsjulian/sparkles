import math
import argparse

def fract(x):
    return x - math.floor(x)

def mix(a, b, t):
    return a + (b - a) * t

def constrain(x, a, b):
    return max(min(x, b), a)

def hsv2rgb(h, s, v):
    h = h / 360.0  # Normalize hue to [0, 1]
    i = int(h * 6)
    f = h * 6 - i
    p = v * (1 - s)
    q = v * (1 - f * s)
    t = v * (1 - (1 - f) * s)

    i = i % 6

    if i == 0:
        r, g, b = v, t, p
    elif i == 1:
        r, g, b = q, v, p
    elif i == 2:
        r, g, b = p, v, t
    elif i == 3:
        r, g, b = p, q, v
    elif i == 4:
        r, g, b = t, p, v
    elif i == 5:
        r, g, b = v, p, q

    return [int(r * 255), int(g * 255), int(b * 255)]
# Example usage
import math

def fract(x):
    return x - math.floor(x)

def mix(a, b, t):
    return a + (b - a) * t

def step(e, x):
    return 0.0 if x < e else 1.0

def intRGBToFloat(val):
    return val / 255.0

def constrain(x, a, b):
    return max(min(x, b), a)

def sRGB_to_float(val):
    if val < 0.04045:
        val /= 12.92
    else:
        val = ((val + 0.055) / 1.055) ** 2.4
    return val

def float_to_sRGB(val):
    if val < 0.0031308:
        val *= 12.92
    else:
        val = 1.055 * (val ** (1.0 / 2.4)) - 0.055
    return val

def hsv2rgb(h, s, b):
    rgb = [0.0, 0.0, 0.0]
    rgb[0] = float_to_sRGB(b * mix(1.0, constrain(abs(fract(h + 1.0) * 6.0 - 3.0) - 1.0, 0.0, 1.0), s) * 255)
    rgb[1] = float_to_sRGB(b * mix(1.0, constrain(abs(fract(h + 0.6666666) * 6.0 - 3.0) - 1.0, 0.0, 1.0), s) * 255)
    rgb[2] = float_to_sRGB(b * mix(1.0, constrain(abs(fract(h + 0.3333333) * 6.0 - 3.0) - 1.0, 0.0, 1.0), s) * 255)
    return rgb

# Example usage
def main():
    parser = argparse.ArgumentParser(description='Convert HSV to RGB.')
    parser.add_argument('h', type=float, help='Hue value (0.0 to 1.0)')
    parser.add_argument('s', type=float, help='Saturation value (0.0 to 1.0)')
    parser.add_argument('b', type=float, help='Brightness value (0.0 to 1.0)')
    args = parser.parse_args()

    rgb = hsv2rgb(args.h, args.s, args.b)
    print(f"R: {rgb[0]}, G: {rgb[1]}, B: {rgb[2]}")

if __name__ == "__main__":
    main()