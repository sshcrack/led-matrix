#pragma once

// Drawing helpers shared by the genre vista scenes (GenreVistaScenes.h).
// Header-only and allocation-free; everything draws into an AudioKit::Buffer.

#include "AudioSceneKit.h"

namespace AudioKit {

// Soft round glow: quadratic falloff from the centre to radius r (additive).
inline void glow(Buffer& b, float cx, float cy, float r, const RGB& c)
{
    const int ri = static_cast<int>(std::ceil(r)) + 1;
    for (int y = static_cast<int>(cy) - ri; y <= static_cast<int>(cy) + ri; ++y)
        for (int x = static_cast<int>(cx) - ri; x <= static_cast<int>(cx) + ri; ++x) {
            if (!b.inside(x, y))
                continue;
            const float d = len(x + 0.5f - cx, y + 0.5f - cy) / r;
            if (d >= 1.0f)
                continue;
            const float a = (1.0f - d) * (1.0f - d);
            b.add(x, y, c * a);
        }
}

// Anti-aliased additive line.
inline void line(Buffer& b, float x0, float y0, float x1, float y1, const RGB& c)
{
    const float dx = x1 - x0, dy = y1 - y0;
    const int n = static_cast<int>(std::ceil(std::max(std::fabs(dx), std::fabs(dy)))) + 1;
    for (int i = 0; i <= n; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(n);
        b.splat(x0 + dx * t - 0.5f, y0 + dy * t - 0.5f, c);
    }
}

// Opaque axis-aligned rectangle [x0, x1) x [y0, y1).
inline void rect(Buffer& b, int x0, int y0, int x1, int y1, const RGB& c)
{
    for (int y = std::max(0, y0); y < std::min(b.h, y1); ++y)
        for (int x = std::max(0, x0); x < std::min(b.w, x1); ++x) b.at(x, y) = c;
}

// Opaque filled ellipse.
inline void ellipse(Buffer& b, float cx, float cy, float rx, float ry, const RGB& c)
{
    for (int y = static_cast<int>(cy - ry) - 1; y <= static_cast<int>(cy + ry) + 1; ++y)
        for (int x = static_cast<int>(cx - rx) - 1; x <= static_cast<int>(cx + rx) + 1; ++x) {
            const float nx = (x + 0.5f - cx) / rx, ny = (y + 0.5f - cy) / ry;
            const float d = len(nx, ny);
            if (d < 1.0f)
                b.mix(x, y, c, std::clamp((1.0f - d) * std::min(rx, ry), 0.0f, 1.0f));
        }
}

// Vertical gradient sky/backdrop over rows [y0, y1).
inline void vgradient(Buffer& b, int y0, int y1, const RGB& top, const RGB& bottom)
{
    for (int y = std::max(0, y0); y < std::min(b.h, y1); ++y) {
        const RGB c = lerp(top, bottom, static_cast<float>(y - y0) / std::max(1, y1 - y0 - 1));
        for (int x = 0; x < b.w; ++x) b.at(x, y) = c;
    }
}

inline float fract(float x) { return x - std::floor(x); }

}  // namespace AudioKit
