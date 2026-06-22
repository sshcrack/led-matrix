#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>

namespace color {

// h in [0, 360) degrees, s/l in [0, 1]
inline void hsl_to_rgb(float h, float s, float l, uint8_t& r, uint8_t& g, uint8_t& b) {
    if (std::isnan(h) || std::isnan(s) || std::isnan(l)) { r = g = b = 0; return; }
    h = std::fmod(h, 360.0f);
    if (h < 0) h += 360.0f;
    float c = (1.0f - std::abs(2.0f * l - 1.0f)) * s;
    float x = c * (1.0f - std::abs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    float m = l - c / 2.0f;

    float r1 = 0, g1 = 0, b1 = 0;
    if (h >= 0 && h < 60) { r1 = c; g1 = x; }
    else if (h < 120) { r1 = x; g1 = c; }
    else if (h < 180) { g1 = c; b1 = x; }
    else if (h < 240) { g1 = x; b1 = c; }
    else if (h < 300) { r1 = x; b1 = c; }
    else { r1 = c; b1 = x; }

    r = static_cast<uint8_t>(std::clamp((r1 + m) * 255.0f, 0.0f, 255.0f));
    g = static_cast<uint8_t>(std::clamp((g1 + m) * 255.0f, 0.0f, 255.0f));
    b = static_cast<uint8_t>(std::clamp((b1 + m) * 255.0f, 0.0f, 255.0f));
}

// h in [0, 360) degrees (same convention as hsl_to_rgb), s/v in [0, 1]
inline void hsv_to_rgb(float h, float s, float v, uint8_t& r, uint8_t& g, uint8_t& b) {
    if (std::isnan(h) || std::isnan(s) || std::isnan(v)) { r = g = b = 0; return; }
    h = std::fmod(h, 360.0f);
    if (h < 0) h += 360.0f;
    h /= 360.0f;
    float c = v * s;
    float x = c * (1.0f - std::abs(std::fmod(h * 6.0f, 2.0f) - 1.0f));
    float m = v - c;

    float r1 = 0, g1 = 0, b1 = 0;
    if (h < 1.0f/6.0f) { r1 = c; g1 = x; b1 = 0; }
    else if (h < 2.0f/6.0f) { r1 = x; g1 = c; b1 = 0; }
    else if (h < 3.0f/6.0f) { r1 = 0; g1 = c; b1 = x; }
    else if (h < 4.0f/6.0f) { r1 = 0; g1 = x; b1 = c; }
    else if (h < 5.0f/6.0f) { r1 = x; g1 = 0; b1 = c; }
    else { r1 = c; g1 = 0; b1 = x; }

    r = static_cast<uint8_t>(std::clamp((r1 + m) * 255.0f, 0.0f, 255.0f));
    g = static_cast<uint8_t>(std::clamp((g1 + m) * 255.0f, 0.0f, 255.0f));
    b = static_cast<uint8_t>(std::clamp((b1 + m) * 255.0f, 0.0f, 255.0f));
}

}
