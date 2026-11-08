#pragma once
// Tiny software rasteriser shared by every hobby scene. Everything is drawn
// into a float RGB buffer (0..1) with soft-edged capsule/ellipse primitives,
// so a figure costs a few hundred pixels and full-screen passes stay cheap.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Hobbies {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTau = 2.0f * kPi;

struct Point {
    float x, y;
};

struct Rgb {
    float r = 0, g = 0, b = 0;
};

inline Rgb mix(Rgb a, Rgb b, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}
inline Rgb scale(Rgb a, float s) { return {a.r * s, a.g * s, a.b * s}; }

inline float fract(float x) { return x - std::floor(x); }
inline float smoothstep(float a, float b, float x)
{
    const float t = std::clamp((x - a) / (b - a), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
// Stable pseudo-random 0..1 from an integer (for per-object variation that
// must not change between frames).
inline float hash01(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return static_cast<float>(x & 0xFFFFFFU) / static_cast<float>(0xFFFFFF);
}

class Canvas {
public:
    int w = 0, h = 0;
    std::vector<float> px;
    void resize(int width, int height)
    {
        w = width;
        h = height;
        px.assign(static_cast<size_t>(w * h * 3), 0.0f);
    }
    float* at(int x, int y) { return &px[static_cast<size_t>((y * w + x) * 3)]; }
    void set(int x, int y, Rgb c)
    {
        if (x < 0 || y < 0 || x >= w || y >= h)
            return;
        float* p = at(x, y);
        p[0] = c.r;
        p[1] = c.g;
        p[2] = c.b;
    }
    void blend(int x, int y, Rgb c, float a)
    {
        if (x < 0 || y < 0 || x >= w || y >= h || a <= 0.0f)
            return;
        a = std::min(a, 1.0f);
        float* p = at(x, y);
        p[0] += (c.r - p[0]) * a;
        p[1] += (c.g - p[1]) * a;
        p[2] += (c.b - p[2]) * a;
    }
    void add(int x, int y, Rgb c, float a)
    {
        if (x < 0 || y < 0 || x >= w || y >= h)
            return;
        float* p = at(x, y);
        p[0] = std::min(1.0f, p[0] + c.r * a);
        p[1] = std::min(1.0f, p[1] + c.g * a);
        p[2] = std::min(1.0f, p[2] + c.b * a);
    }
    // Soft-edged capsule from (x0,y0) radius r0 to (x1,y1) radius r1.
    void capsule(float x0, float y0, float r0, float x1, float y1, float r1, Rgb c, float alpha = 1.0f)
    {
        const float rm = std::max(r0, r1) + 1.0f;
        const int minx = static_cast<int>(std::floor(std::min(x0, x1) - rm)), maxx = static_cast<int>(std::ceil(std::max(x0, x1) + rm));
        const int miny = static_cast<int>(std::floor(std::min(y0, y1) - rm)), maxy = static_cast<int>(std::ceil(std::max(y0, y1) + rm));
        const float dx = x1 - x0, dy = y1 - y0, len2 = dx * dx + dy * dy;
        for (int y = std::max(0, miny); y <= std::min(h - 1, maxy); ++y)
            for (int x = std::max(0, minx); x <= std::min(w - 1, maxx); ++x) {
                const float qx = x + 0.5f - x0, qy = y + 0.5f - y0;
                const float t = len2 > 1e-6f ? std::clamp((qx * dx + qy * dy) / len2, 0.0f, 1.0f) : 0.0f;
                const float ex = qx - dx * t, ey = qy - dy * t;
                const float r = r0 + (r1 - r0) * t;
                const float d = std::sqrt(ex * ex + ey * ey) - r;
                blend(x, y, c, std::clamp(0.5f - d, 0.0f, 1.0f) * alpha);
            }
    }
    void ellipse(float cx, float cy, float rx, float ry, float angle, Rgb c, float alpha = 1.0f)
    {
        const float ca = std::cos(angle), sa = std::sin(angle);
        // Bounds of the rotated ellipse (not a square), plus the AA fringe.
        const float ex = std::sqrt(rx * rx * ca * ca + ry * ry * sa * sa) + 1.0f;
        const float ey = std::sqrt(rx * rx * sa * sa + ry * ry * ca * ca) + 1.0f;
        const float rmin = std::min(rx, ry);
        const float outer = (1.0f + 0.5f / rmin) * (1.0f + 0.5f / rmin);  // (u²+v²) beyond the fringe
        for (int y = std::max(0, static_cast<int>(cy - ey)); y <= std::min(h - 1, static_cast<int>(cy + ey)); ++y)
            for (int x = std::max(0, static_cast<int>(cx - ex)); x <= std::min(w - 1, static_cast<int>(cx + ex)); ++x) {
                const float qx = x + 0.5f - cx, qy = y + 0.5f - cy;
                const float u = (qx * ca + qy * sa) / rx, v = (-qx * sa + qy * ca) / ry;
                const float r2 = u * u + v * v;
                if (r2 >= outer)
                    continue;
                const float d = (std::sqrt(r2) - 1.0f) * rmin;
                blend(x, y, c, std::clamp(0.5f - d, 0.0f, 1.0f) * alpha);
            }
    }
    // Additive radial glow with quadratic falloff out to radius r.
    void glow(float cx, float cy, float r, Rgb c, float strength)
    {
        if (strength <= 0.0f || r <= 0.0f)
            return;
        const float inv = 1.0f / r;
        for (int y = std::max(0, static_cast<int>(cy - r)); y <= std::min(h - 1, static_cast<int>(cy + r)); ++y)
            for (int x = std::max(0, static_cast<int>(cx - r)); x <= std::min(w - 1, static_cast<int>(cx + r)); ++x) {
                const float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
                const float d = std::sqrt(dx * dx + dy * dy) * inv;
                if (d < 1.0f)
                    add(x, y, c, strength * (1.0f - d) * (1.0f - d));
            }
    }
    // Vertical three-stop gradient over the whole canvas.
    void fill_gradient(Rgb top, Rgb mid, Rgb bottom, float mid_at)
    {
        for (int y = 0; y < h; ++y) {
            const float v = static_cast<float>(y) / std::max(1, h - 1);
            const Rgb c = v < mid_at ? mix(top, mid, v / mid_at) : mix(mid, bottom, (v - mid_at) / (1.0f - mid_at));
            for (int x = 0; x < w; ++x) {
                float* p = at(x, y);
                p[0] = c.r;
                p[1] = c.g;
                p[2] = c.b;
            }
        }
    }
    // Fills everything below the curve top(x) (one anti-aliased edge pixel).
    template <typename F>
    void fill_below(F&& top, Rgb c)
    {
        for (int x = 0; x < w; ++x) {
            const float t = top(static_cast<float>(x) + 0.5f);
            const int y0 = std::max(0, static_cast<int>(std::floor(t)));
            for (int y = y0; y < h; ++y) blend(x, y, c, y == y0 ? 1.0f - (t - std::floor(t)) : 1.0f);
        }
    }
};

// Rolling hill silhouette height (0..1) from layered sines.
inline float hill(float x, float seed)
{
    return 0.5f + 0.28f * std::sin(x * 0.045f + seed) + 0.17f * std::sin(x * 0.11f + seed * 2.3f) + 0.05f * std::sin(x * 0.27f + seed * 4.1f);
}
inline void hills(Canvas& cv, float scroll, float base_y, float amp, float seed, Rgb col)
{
    cv.fill_below([&](float x) { return base_y - hill(x + scroll, seed) * amp; }, col);
}

}  // namespace Hobbies
