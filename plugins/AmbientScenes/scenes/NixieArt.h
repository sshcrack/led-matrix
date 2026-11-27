#pragma once

// Baked layers of the nixie clock and the per-rectangle compositor.
//
// The geometry follows plugins/Shadertoy/shaders/music_tube_amp.frag (same
// units: uv = (pixel - size / 2) / height). Tube centres sit on whole pixels,
// so every tube shares one set of baked fields:
//   - the static frame (room, bokeh, plinth, ceramic bases, indicator bulbs),
//   - the tube interior (unlit cathode stack, leads, spacer, mesh, glass),
//   - per numeral: lit core, near glow and wide glow,
//   - the light a tube throws onto its surroundings and the plinth,
//   - the colon dots.
// A bake is shared by every scene instance with the same size and tube count.

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace AmbientScenes::Nixie {

struct Rect {
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;  // [x0, x1) x [y0, y1)
    [[nodiscard]] bool empty() const { return x0 >= x1 || y0 >= y1; }
    void unite(const Rect& o)
    {
        if (o.empty())
            return;
        if (empty()) {
            *this = o;
            return;
        }
        x0 = x0 < o.x0 ? x0 : o.x0;
        y0 = y0 < o.y0 ? y0 : o.y0;
        x1 = x1 > o.x1 ? x1 : o.x1;
        y1 = y1 > o.y1 ? y1 : o.y1;
    }
};

/// What one tube shows: numeral a fading into numeral b (-1 = none lit).
struct TubeLook {
    int a = -1, b = -1;
    float mix = 1.0f;    // 0 shows a, 1 shows b
    float heat = 0.72f;  // neon drive, ~0.6..1.1
};

struct Baked;

class Art {
public:
    /// Bakes (or reuses a bake) for a canvas and 4 or 6 tubes.
    void bake(int width, int height, int tubes);
    [[nodiscard]] bool ready() const { return baked_ != nullptr; }
    [[nodiscard]] int tubes() const;
    [[nodiscard]] int colons() const;

    [[nodiscard]] Rect full() const;
    /// Everything tube i lights: its glass and the glow around it.
    [[nodiscard]] Rect tube_rect(int i) const;
    /// Only tube i's glass (enough when its numeral changes but not its heat).
    [[nodiscard]] Rect box_rect(int i) const;
    [[nodiscard]] Rect colon_rect() const;

    /// Recomposes the pixels in r. glow_gain scales the light thrown outside
    /// the glass (music swells it a little); colon is the dots' level 0..1.
    void compose(Rect r, const std::vector<TubeLook>& looks, float colon, float glow_gain);
    [[nodiscard]] const std::uint8_t* rgb() const { return rgb_.data(); }

    /// Milliseconds the last bake took (0 when it came from the cache).
    [[nodiscard]] double last_bake_ms() const { return last_bake_ms_; }

private:
    std::shared_ptr<const Baked> baked_;
    std::vector<std::uint8_t> rgb_;
    double last_bake_ms_ = 0.0;
};

}  // namespace AmbientScenes::Nixie
