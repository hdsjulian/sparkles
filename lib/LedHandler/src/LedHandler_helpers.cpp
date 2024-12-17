#include <LedHandler.h>


float LedHandler::fract(float x) {
    return x - floor(x);
}

float LedHandler::mix(float a, float b, float t) { return a + (b - a) * t; }

float LedHandler::step(float e, float x) { return x < e ? 0.0 : 1.0; }

float LedHandler::intRGBToFloat(int val) { return val / 255.0; }

float LedHandler::float_to_sRGB(float val)
{
    if (val < 0.0031308)
        val *= 12.92;
    else
        val = 1.055 * pow(val, 1.0 / 2.4) - 0.055;
    return val;
}
float *LedHandler::hsv2rgb(float h, float s, float b, float *rgb)
{
    rgb[0] = float_to_sRGB(b * mix(1.0, constrain(abs(fract(h + 1.0) * 6.0 - 3.0) - 1.0, 0.0, 1.0), s) * 255);
    rgb[1] = float_to_sRGB(b * mix(1.0, constrain(abs(fract(h + 0.6666666) * 6.0 - 3.0) - 1.0, 0.0, 1.0), s) * 255);
    rgb[2] = float_to_sRGB(b * mix(1.0, constrain(abs(fract(h + 0.3333333) * 6.0 - 3.0) - 1.0, 0.0, 1.0), s) * 255);
    return rgb;
}

float LedHandler::sRGB_to_float(float val)
{
    if (val < 0.04045)
        val /= 12.92;
    else
        val = pow((val + 0.055) / 1.055, 2.4);
    return val;
}





TickType_t LedHandler::microsToTicks(unsigned long long micros) {
    return (TickType_t)(micros / (1000000 / configTICK_RATE_HZ));
}
