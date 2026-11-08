#pragma once

// Weather and sky painting shared by scenery scenes. Works on any float RGB
// buffer with 3 floats per pixel, whatever its unit (Hobby scenes use 0..1,
// AudioKit buffers 0..255): wrap it in a Surface.
//
// Typical frame:
//     paint_sky(s, look, horizon);
//     paint_stars(s, look, horizon, t);
//     paint_sun(s, look, sun_point(look, w, horizon), r);
//     paint_moon(s, look, moon_point(look, w, horizon), r);
//     paint_clouds(s, look, t, 0, horizon * 0.6f, scroll);
//     ... scene draws land/figures with look.lit(...) ...
//     ... scene draws lamps/windows scaled by look.lamp() ...
//     paint_fog(s, look, t, horizon - 10, h);
//     precipitation.update(dt, look, ground_fn); precipitation.draw(s, look);
//     lightning.update_and_draw(s, look, dt, horizon);

#include <cstdint>
#include <functional>
#include <random>
#include <vector>

#include <shared/matrix/environment/look.h>

namespace Environment {

struct Point2 {
    float x = 0, y = 0;
};

struct Surface {
    float *px = nullptr;
    int w = 0, h = 0;
    float unit = 1.0f;  // 1 for 0..1 buffers, 255 for 0..255 buffers

    [[nodiscard]] bool inside(int x, int y) const { return x >= 0 && y >= 0 && x < w && y < h; }
    [[nodiscard]] float *at(int x, int y) const { return px + (static_cast<std::size_t>(y) * w + x) * 3; }
    [[nodiscard]] Rgb get(int x, int y) const
    {
        const float *p = at(x, y);
        return {p[0] / unit, p[1] / unit, p[2] / unit};
    }
    void set(int x, int y, Rgb c) const
    {
        if (!inside(x, y))
            return;
        float *p = at(x, y);
        p[0] = c.r * unit;
        p[1] = c.g * unit;
        p[2] = c.b * unit;
    }
    void blend(int x, int y, Rgb c, float a) const
    {
        if (!inside(x, y) || a <= 0.0f)
            return;
        a = a > 1.0f ? 1.0f : a;
        float *p = at(x, y);
        p[0] += (c.r * unit - p[0]) * a;
        p[1] += (c.g * unit - p[1]) * a;
        p[2] += (c.b * unit - p[2]) * a;
    }
    void add(int x, int y, Rgb c, float a = 1.0f) const
    {
        if (!inside(x, y) || a <= 0.0f)
            return;
        float *p = at(x, y);
        p[0] += c.r * unit * a;
        p[1] += c.g * unit * a;
        p[2] += c.b * unit * a;
    }
};

/// Where the sun sits in a view whose horizon is at `horizon_y`.
[[nodiscard]] Point2 sun_point(const Look &look, float width, float horizon_y);
/// Where the moon sits (a fixed spot opposite-ish to the sun's path).
[[nodiscard]] Point2 moon_point(const Look &look, float width, float horizon_y);

/// Sky gradient over rows [0, horizon_y) plus the sun's halo.
void paint_sky(const Surface &s, const Look &look, float horizon_y);
/// Twinkling stars above `max_y`, visible with night and clear skies.
void paint_stars(const Surface &s, const Look &look, float max_y, float t, uint32_t seed = 7, int count = 90);
void paint_sun(const Surface &s, const Look &look, Point2 p, float radius);
/// Moon with its real phase (lit side faces the sun's side of the sky).
void paint_moon(const Surface &s, const Look &look, Point2 p, float radius);
/// Drifting soft clouds between y0 and y1; amount and colour from the look.
/// `scroll` adds parallax for side-scrolling scenes.
void paint_clouds(const Surface &s, const Look &look, float t, float y0, float y1, float scroll = 0.0f,
                  uint32_t seed = 11);
/// Drifting fog banks over rows [y0, y1); strength from look.fog (plus `base`
/// for scenes that always have a little mist).
void paint_fog(const Surface &s, const Look &look, float t, float y0, float y1, float base = 0.0f);
/// Soft anti-aliased ellipse (blend), handy for scene-specific weather bits.
void soft_ellipse(const Surface &s, float cx, float cy, float rx, float ry, Rgb c, float alpha);

/// Rain and snow particles with ground splashes.
class Precipitation {
public:
    using GroundFn = std::function<float(float x)>;  // surface y at x (h when none)

    void reset()
    {
        drops_.clear();
        warm_ = false;
        carry_ = 0.0f;
    }
    void set_seed(uint32_t seed) { rng_.seed(seed); }
    /// `scale` = width/128 so density and speed match any panel size.
    void update(float dt, const Look &look, int width, int height, const GroundFn &ground = {});
    void draw(const Surface &s, const Look &look) const;
    /// Limit the count (render quality / Pi budget). Default 260.
    void set_max(int max_particles) { max_ = max_particles; }

private:
    struct Drop {
        float x, y, vx, vy, life, size;
        bool snow;
        bool splash;
    };
    std::vector<Drop> drops_;
    std::mt19937 rng_{0x5EEDu};
    float carry_ = 0.0f;
    int max_ = 260;
    bool warm_ = false;  // pre-filled the panel when precipitation started
};

/// Falling leaves in autumn, blossom petals in spring and fireflies on warm
/// summer nights; nothing in winter. Amounts follow the season weights.
class SeasonalDrift {
public:
    using GroundFn = std::function<float(float x)>;
    void reset() { bits_.clear(); }
    void set_seed(uint32_t seed) { rng_.seed(seed); }
    /// `density` scales how much falls (1 = a gentle few per second).
    void update(float dt, const Look &look, int width, int height, const GroundFn &ground = {}, float density = 1.0f);
    void draw(const Surface &s, const Look &look) const;

private:
    enum class Kind : uint8_t { Leaf, Petal, Firefly };
    struct Bit {
        float x, y, vx, vy, phase, life;
        Rgb color;
        Kind kind;
        bool landed;
    };
    std::vector<Bit> bits_;
    std::mt19937 rng_{0x1EAFu};
    float carry_ = 0.0f, flies_ = 0.0f;
};

/// A window: dark glass reflecting the sky by day, warm light when look.lamp()
/// is on. `seed` varies the colour and leaves some rooms dark at night.
void paint_window(const Surface &s, const Look &look, int x, int y, int w, int h, uint32_t seed, float t = 0.0f);

/// A sagging string of coloured bulbs from a to b (party/holiday lights).
/// Glows at night, faint by day.
void paint_string_lights(const Surface &s, const Look &look, Point2 a, Point2 b, float sag, float t, int bulbs,
                         uint32_t seed = 3);

/// Jagged lightning bolts drawn on Look::bolt (storms only).
class Lightning {
public:
    void update_and_draw(const Surface &s, const Look &look, float dt, float horizon_y);
    void set_seed(uint32_t seed) { rng_.seed(seed); }

private:
    std::vector<Point2> path_;
    float life_ = 0.0f;
    std::mt19937 rng_{0xB017u};
};

} // namespace Environment
