#include "NixieArt.h"

#include <shared/matrix/audio_kit.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <map>
#include <mutex>
#include <tuple>

namespace AmbientScenes::Nixie {

struct P {
    float x = 0, y = 0;
};
struct C {
    float r = 0, g = 0, b = 0;
};

namespace {

C operator*(C c, float s)
{
    return {c.r * s, c.g * s, c.b * s};
}
C operator+(C a, C b)
{
    return {a.r + b.r, a.g + b.g, a.b + b.b};
}
C mix(C a, C b, float t)
{
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}

constexpr C Orange{1.0f, 0.36f, 0.06f};
constexpr C Hot{1.0f, 0.66f, 0.30f};

float fract(float x)
{
    return x - std::floor(x);
}
float clamp01(float x)
{
    return std::clamp(x, 0.0f, 1.0f);
}
// GLSL smoothstep, edges may be reversed.
float smooth(float e0, float e1, float x)
{
    const float t = clamp01((x - e0) / (e1 - e0));
    return t * t * (3.0f - 2.0f * t);
}
float hash11(float p)
{
    p = fract(p * 0.1031f);
    p *= p + 33.33f;
    p *= p + p;
    return fract(p);
}
float len(P p)
{
    return AudioKit::len(p.x, p.y);
}
float sd_segment(P p, P a, P b)
{
    const P pa{p.x - a.x, p.y - a.y}, ba{b.x - a.x, b.y - a.y};
    const float h = clamp01((pa.x * ba.x + pa.y * ba.y) / (ba.x * ba.x + ba.y * ba.y));
    return len({pa.x - ba.x * h, pa.y - ba.y * h});
}
float sd_round_box(P p, P b, float r)
{
    const float qx = std::fabs(p.x) - b.x + r, qy = std::fabs(p.y) - b.y + r;
    return len({std::max(qx, 0.0f), std::max(qy, 0.0f)}) + std::min(std::max(qx, qy), 0.0f) - r;
}
float gl_mod(float x, float y)
{
    return x - y * std::floor(x / y);
}
// Hex mesh: 1 on the wires, 0 in the holes.
float hex_mesh(P p)
{
    const P s{1.0f, 1.7320508f};
    const P a{gl_mod(p.x, s.x) - s.x * 0.5f, gl_mod(p.y, s.y) - s.y * 0.5f};
    const P b{gl_mod(p.x - s.x * 0.5f, s.x) - s.x * 0.5f, gl_mod(p.y - s.y * 0.5f, s.y) - s.y * 0.5f};
    P g = (a.x * a.x + a.y * a.y) < (b.x * b.x + b.y * b.y) ? a : b;
    g = {std::fabs(g.x), std::fabs(g.y)};
    const float edge = std::max(g.x * s.x * 0.5f + g.y * s.y * 0.5f, g.x);
    return smooth(0.36f, 0.48f, edge);
}

// Numeral strokes in glyph space (x -0.5..0.5, y -0.8..0.8, y up), the same
// shapes as digitDist() in the shader.
struct Stroke {
    enum Kind { Seg, Ellipse, Arc } kind;
    P a, b;  // Seg: ends. Ellipse/Arc: centre, radii.
    P n{};   // Arc: keeps the side where dot(q, n) > 0.
};

std::vector<Stroke> strokes(int d)
{
    using K = Stroke::Kind;
    switch (d) {
        case 0:
            return {{K::Ellipse, {0, 0}, {0.42f, 0.74f}}};
        case 1:
            return {{K::Seg, {0.05f, -0.74f}, {0.05f, 0.74f}}, {K::Seg, {0.05f, 0.74f}, {-0.18f, 0.52f}}};
        case 2:
            return {{K::Arc, {0, 0.36f}, {0.38f, 0.36f}, {0.15f, 1.0f}},
                    {K::Seg, {0.34f, 0.24f}, {-0.40f, -0.74f}},
                    {K::Seg, {-0.40f, -0.74f}, {0.42f, -0.74f}}};
        case 3:
            return {{K::Arc, {0, 0.38f}, {0.36f, 0.36f}, {1.0f, 0.25f}}, {K::Arc, {0, -0.36f}, {0.40f, 0.38f}, {1.0f, -0.25f}}};
        case 4:
            return {{K::Seg, {0.20f, -0.74f}, {0.20f, 0.74f}},
                    {K::Seg, {0.20f, 0.74f}, {-0.42f, -0.30f}},
                    {K::Seg, {-0.42f, -0.30f}, {0.44f, -0.30f}}};
        case 5:
            return {{K::Seg, {0.38f, 0.74f}, {-0.30f, 0.74f}},
                    {K::Seg, {-0.30f, 0.74f}, {-0.36f, 0.06f}},
                    {K::Arc, {0, -0.30f}, {0.40f, 0.42f}, {1.0f, 0.0f}},
                    {K::Arc, {0, -0.30f}, {0.40f, 0.42f}, {0.0f, 1.0f}}};
        case 6:
            return {{K::Ellipse, {0, -0.36f}, {0.40f, 0.38f}}, {K::Arc, {0.30f, -0.10f}, {0.70f, 0.84f}, {-0.5f, 1.0f}}};
        case 7:
            return {{K::Seg, {-0.42f, 0.74f}, {0.42f, 0.74f}}, {K::Seg, {0.42f, 0.74f}, {-0.12f, -0.74f}}};
        case 8:
            return {{K::Ellipse, {0, 0.40f}, {0.32f, 0.33f}}, {K::Ellipse, {0, -0.36f}, {0.40f, 0.38f}}};
        default:
            return {{K::Ellipse, {0, 0.36f}, {0.40f, 0.38f}}, {K::Arc, {-0.30f, 0.10f}, {0.70f, 0.84f}, {0.5f, -1.0f}}};
    }
}

// Points along numeral d in pixels (y up), glyph scaled by (sx, sy) px and
// shrunk by `shrink` (the stacked cathodes sit slightly smaller behind).
void sample_digit(int d, float sx, float sy, float shrink, std::vector<P>& out)
{
    // ~0.6 px between points: off the stroke the error is far below a pixel,
    // on it the lit core (0.85 px) still covers the gaps.
    const float step = 0.6f / std::max(sx, sy);
    const auto add = [&](float gx, float gy) {
        // The long tails of 6 and 9 leave the glyph box; the shader clips
        // them at the glass, a real cathode stops at its frame.
        if (std::fabs(gx) > 0.47f || std::fabs(gy) > 0.78f)
            return;
        out.push_back({gx * sx / shrink, gy * sy / shrink});
    };
    for (const auto& s : strokes(d)) {
        if (s.kind == Stroke::Seg) {
            const float l = AudioKit::len(s.b.x - s.a.x, s.b.y - s.a.y);
            const int n = std::max(1, static_cast<int>(std::ceil(l / step)));
            for (int i = 0; i <= n; ++i) {
                const float t = static_cast<float>(i) / static_cast<float>(n);
                add(s.a.x + (s.b.x - s.a.x) * t, s.a.y + (s.b.y - s.a.y) * t);
            }
            continue;
        }
        const float perimeter = AudioKit::Tau * std::sqrt(0.5f * (s.b.x * s.b.x + s.b.y * s.b.y));
        const int n = std::max(8, static_cast<int>(std::ceil(perimeter / step)));
        for (int i = 0; i < n; ++i) {
            const float th = AudioKit::Tau * static_cast<float>(i) / static_cast<float>(n);
            const P q{s.b.x * std::cos(th), s.b.y * std::sin(th)};
            if (s.kind == Stroke::Arc && q.x * s.n.x + q.y * s.n.y <= 0.0f)
                continue;
            add(s.a.x + q.x, s.a.y + q.y);
        }
    }
}

// Points bucketed into square cells; nearest() searches outward ring by
// ring and stops once no unseen cell can be closer. Keeps the one-off bake
// cheap on the Pi.
class PointGrid {
public:
    PointGrid(const std::vector<P>& pts, float cell) : cell_(cell)
    {
        float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
        for (const auto& p : pts) {
            x0 = std::min(x0, p.x);
            y0 = std::min(y0, p.y);
            x1 = std::max(x1, p.x);
            y1 = std::max(y1, p.y);
        }
        ox_ = x0;
        oy_ = y0;
        nx_ = std::max(1, static_cast<int>((x1 - x0) / cell) + 1);
        ny_ = std::max(1, static_cast<int>((y1 - y0) / cell) + 1);
        cells_.resize(static_cast<std::size_t>(nx_ * ny_));
        for (const auto& p : pts) cells_[static_cast<std::size_t>(index(p))].push_back(p);
    }

    /// Distance to the nearest point, or `limit` when none is that close.
    [[nodiscard]] float nearest(P p, float limit) const
    {
        const int cx = std::clamp(static_cast<int>(std::floor((p.x - ox_) / cell_)), 0, nx_ - 1);
        const int cy = std::clamp(static_cast<int>(std::floor((p.y - oy_) / cell_)), 0, ny_ - 1);
        float best = limit * limit;
        const int max_r = std::max(nx_, ny_);
        for (int r = 0; r <= max_r; ++r) {
            for (int y = cy - r; y <= cy + r; ++y) {
                if (y < 0 || y >= ny_)
                    continue;
                const bool edge_row = y == cy - r || y == cy + r;
                for (int x = cx - r; x <= cx + r; x += edge_row ? 1 : 2 * std::max(r, 1)) {
                    if (x < 0 || x >= nx_)
                        continue;
                    for (const auto& q : cells_[static_cast<std::size_t>(y * nx_ + x)]) {
                        const float dx = q.x - p.x, dy = q.y - p.y;
                        best = std::min(best, dx * dx + dy * dy);
                    }
                }
            }
            // Cells beyond ring r are at least r cells away.
            const float bound = static_cast<float>(r) * cell_;
            if (best <= bound * bound || bound > limit)
                break;
        }
        return std::sqrt(best);
    }

private:
    [[nodiscard]] int index(P p) const
    {
        const int x = std::clamp(static_cast<int>((p.x - ox_) / cell_), 0, nx_ - 1);
        const int y = std::clamp(static_cast<int>((p.y - oy_) / cell_), 0, ny_ - 1);
        return y * nx_ + x;
    }
    float cell_, ox_ = 0, oy_ = 0;
    int nx_ = 1, ny_ = 1;
    std::vector<std::vector<P>> cells_;
};

struct TonemapLut {
    static constexpr int Size = 4096;
    static constexpr float Range = 5.0f;
    std::array<std::uint8_t, Size + 1> v{};
    TonemapLut()
    {
        for (int i = 0; i <= Size; ++i) {
            const float x = Range * static_cast<float>(i) / Size;
            v[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(std::lround(255.0f * (1.0f - std::exp(-x * 1.5f))));
        }
    }
    [[nodiscard]] std::uint8_t operator()(float x) const
    {
        const int i = static_cast<int>(std::clamp(x, 0.0f, Range) * (Size / Range) + 0.5f);
        return v[static_cast<std::size_t>(i)];
    }
};
const TonemapLut& tonemap()
{
    static const TonemapLut lut;
    return lut;
}

}  // namespace

// Interior of one tube, per pixel of its box (shared by all tubes).
struct TubePixel {
    float ins = 0;   // glass coverage 0..1 (0 under the ceramic base)
    C t0;            // unlit interior: cathode stack, leads, spacer
    float lead = 0;  // lead-in wires, brightened by heat
    float mesh = 0;  // anode mesh weight
    C glass;         // rim + specular streak
    float rim = 0;   // rim catches the numeral's glow
};

struct Baked {
    int w = 0, h = 0, tubes = 0;
    std::vector<C> bg;        // static frame, linear (before tone mapping)
    std::vector<int> centre;  // tube centre column (pixel boundary)
    std::vector<std::int8_t> tube_of_col;
    int half = 0, box_y0 = 0, box_y1 = 0;  // tube box: [centre - half, centre + half) x [box_y0, box_y1)
    std::vector<TubePixel> interior;
    std::array<std::vector<float>, 10> lit, glow, wide;  // per numeral, per box pixel
    int glow_half = 0, glow_y0 = 0;                      // thrown light: [centre - glow_half, centre + glow_half) x [glow_y0, h)
    std::vector<float> thrown;
    Rect colon;  // colon dots field
    std::vector<float> colon_core, colon_glow;
    int colons = 0;

    [[nodiscard]] int box_w() const { return 2 * half; }
    [[nodiscard]] int glow_w() const { return 2 * glow_half; }
};

namespace {

Rect clip(Rect r, int w, int h)
{
    return {std::max(0, r.x0), std::max(0, r.y0), std::min(w, r.x1), std::min(h, r.y1)};
}

std::shared_ptr<const Baked> build(int W, int H, int tubes)
{
    auto out = std::make_shared<Baked>();
    Baked& b = *out;
    b.w = W;
    b.h = H;
    b.tubes = tubes;
    const float Hf = static_cast<float>(H);
    const float px = 1.0f / Hf;
    const bool six = tubes == 6;

    // Layout in uv. Six tubes are narrower (IN-14 style) so hh:mm:ss fits.
    const float hw = six ? 0.060f : 0.092f;  // glass half width
    const float k = hw / 0.092f;             // horizontal scale against the shader
    const float corner = six ? 0.056f : 0.085f;
    const float gsx = six ? 0.092f : 0.135f;  // numeral scale
    const float gsy = six ? 0.130f : 0.135f;
    const float pitch = six ? 18.0f / 128.0f : 30.0f / 128.0f;
    const float extra = six ? 5.0f / 128.0f : 6.0f / 128.0f;
    std::vector<float> offs;
    if (six)
        offs = {-2.5f * pitch - extra, -1.5f * pitch - extra, -0.5f * pitch, 0.5f * pitch, 1.5f * pitch + extra, 2.5f * pitch + extra};
    else
        offs = {-1.5f * pitch - extra * 0.5f, -0.5f * pitch - extra * 0.5f, 0.5f * pitch + extra * 0.5f, 1.5f * pitch + extra * 0.5f};
    for (float o : offs) b.centre.push_back(static_cast<int>(std::lround(0.5f * static_cast<float>(W) + o * Hf)));

    const auto uv_of = [&](float x, float y) {  // pixel-space point -> uv
        return P{(x - 0.5f * static_cast<float>(W)) / Hf, (0.5f * Hf - y) / Hf};
    };
    const float plinth_top = -0.30f;
    const float tube_y = 0.02f;

    // --- Static frame ------------------------------------------------------
    b.bg.resize(static_cast<std::size_t>(W * H));
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const P uv = uv_of(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f);
            C col = mix({0.016f, 0.010f, 0.012f}, {0.045f, 0.024f, 0.018f}, smooth(-0.5f, 0.4f, uv.y));
            // Out-of-focus lamps far behind the tubes.
            for (int i = 0; i < 5; ++i) {
                const float fi = static_cast<float>(i);
                const P c{-0.46f + fi * 0.23f + 0.02f * std::sin(fi), 0.38f - 0.05f * hash11(fi * 7.1f)};
                const float bk = smooth(0.075f, 0.055f, len({uv.x - c.x, uv.y - c.y}));
                col = col +
                      mix({0.20f, 0.08f, 0.03f}, {0.06f, 0.05f, 0.12f}, hash11(fi + 3.0f)) * (bk * (0.30f + 0.20f * std::sin(fi * 2.0f)));
            }
            // Walnut plinth with a lit front edge.
            if (uv.y < plinth_top) {
                const float grain = 0.5f + 0.5f * std::sin(uv.x * 40.0f + 6.0f * std::sin(uv.y * 30.0f + uv.x * 3.0f));
                col = mix({0.075f, 0.034f, 0.016f}, {0.13f, 0.062f, 0.028f}, grain) * (smooth(-0.56f, plinth_top, uv.y) * 0.6f + 0.4f);
                col = col + C{0.30f, 0.16f, 0.08f} * (smooth(1.5f * px, 0.0f, std::fabs(uv.y - plinth_top + px)) * 0.8f);
            }
            // Ceramic-and-steel sockets the pins enter.
            for (int c : b.centre) {
                const P p{(static_cast<float>(x) + 0.5f - static_cast<float>(c)) / Hf, uv.y - tube_y};
                if (std::fabs(p.x) <= 0.095f * k && uv.y >= plinth_top - 0.02f && uv.y <= plinth_top + 0.045f)
                    col = C{0.09f, 0.08f, 0.075f} + C{0.22f, 0.18f, 0.15f} * (smooth(0.03f * k, 0.0f, std::fabs(p.x + 0.05f * k)) * 0.6f);
            }
            // Two indicator bulbs on the plinth, softly lit.
            for (int j = 0; j < 2; ++j) {
                const float d = len({uv.x - (j == 0 ? -0.43f : 0.43f), uv.y + 0.41f});
                const float on = 0.27f;
                col = col + C{1.0f, 0.30f, 0.08f} * (smooth(0.020f, 0.012f, d) * on + std::exp(-d * 30.0f) * on * 0.25f);
            }
            b.bg[static_cast<std::size_t>(y * W + x)] = col;
        }

    // --- Tube interior (shared by all tubes) ---------------------------------
    b.half = static_cast<int>(std::ceil(hw * Hf)) + 2;
    b.box_y0 = std::max(0, static_cast<int>(std::floor(0.5f * Hf - (tube_y + 0.345f) * Hf)) - 1);
    b.box_y1 = std::min(H, static_cast<int>(std::ceil(0.5f * Hf - (plinth_top - 0.03f) * Hf)) + 1);
    const int bw = b.box_w(), bh = b.box_y1 - b.box_y0;
    b.interior.resize(static_cast<std::size_t>(bw * bh));
    for (auto& f : b.lit) f.assign(static_cast<std::size_t>(bw * bh), 0.0f);
    for (auto& f : b.glow) f.assign(static_cast<std::size_t>(bw * bh), 0.0f);
    for (auto& f : b.wide) f.assign(static_cast<std::size_t>(bw * bh), 0.0f);

    std::array<std::vector<P>, 10> digits;
    std::vector<P> stack;
    for (int d = 0; d < 10; ++d) {
        sample_digit(d, gsx * Hf, gsy * Hf, 1.0f, digits[static_cast<std::size_t>(d)]);
        sample_digit(d, gsx * Hf, gsy * Hf, 1.0f + 0.025f * static_cast<float>(d), stack);
    }
    const float cell = 3.0f * Hf / 128.0f;
    const PointGrid stack_grid(stack, cell);
    std::vector<PointGrid> digit_grids;
    for (const auto& d : digits) digit_grids.emplace_back(d, cell);

    for (int ly = 0; ly < bh; ++ly)
        for (int lx = 0; lx < bw; ++lx) {
            const float fx = static_cast<float>(lx - b.half) + 0.5f;  // px from the tube centre
            const float fy = static_cast<float>(b.box_y0 + ly) + 0.5f;
            const P uv = uv_of(0.5f * static_cast<float>(W), fy);
            const P p{fx / Hf, uv.y - tube_y};
            const std::size_t i = static_cast<std::size_t>(ly * bw + lx);
            TubePixel& t = b.interior[i];

            float glass_d = sd_round_box(p, {hw, 0.30f}, corner);
            glass_d = std::min(glass_d, sd_segment(p, {0.0f, 0.29f}, {0.0f, 0.32f}) - 0.012f);
            t.ins = smooth(px, -px, glass_d);
            const bool socket = std::fabs(p.x) <= 0.095f * k && uv.y >= plinth_top - 0.02f && uv.y <= plinth_top + 0.045f;
            if (socket || t.ins <= 0.0f) {
                t.ins = 0.0f;
                continue;
            }

            const P gq{p.x, p.y - 0.04f};
            const P g{gq.x * Hf, gq.y * Hf};  // numeral space in px
            // Unlit cathodes are drawn within ~1 px only.
            const float cath = stack_grid.nearest(g, 2.0f);
            t.t0 = C{0.020f, 0.014f, 0.014f} + C{0.11f, 0.09f, 0.085f} * (smooth(1.2f, 0.2f, cath) * 0.55f);
            const float leads = std::min(std::fabs(std::fabs(p.x) - 0.045f * k), std::fabs(p.x));
            const float lead_mask = (p.y <= -0.08f && p.y >= -0.29f) ? 1.0f : 0.0f;
            t.lead = smooth(px, 0.0f, leads) * lead_mask;
            t.t0 = t.t0 + C{0.16f, 0.12f, 0.10f} * (t.lead * 0.5f);
            t.t0 = t.t0 +
                   C{0.10f, 0.08f, 0.07f} * (smooth(1.2f * px, 0.0f, std::fabs(p.y + 0.15f)) * (std::fabs(p.x) <= 0.07f * k ? 1.0f : 0.0f));

            t.mesh = 0.55f * hex_mesh({gq.x / (3.0f * px), gq.y / (3.0f * px)}) *
                     smooth(0.012f, 0.0f, sd_round_box(gq, {0.075f * k, 0.13f}, 0.02f));
            t.rim = smooth(1.6f * px, 0.0f, std::fabs(glass_d));
            t.glass = C{0.30f, 0.22f, 0.18f} * (t.rim * 0.35f) +
                      C{0.55f, 0.50f, 0.45f} *
                          (smooth(1.6f * px, 0.0f, std::fabs(p.x + 0.060f * k)) * smooth(0.24f, 0.05f, std::fabs(p.y - 0.06f)) * 0.30f);

            for (int d = 0; d < 10; ++d) {
                // Past ~3 wide-glow lengths the halo is flat; stop searching there.
                const float dist = digit_grids[static_cast<std::size_t>(d)].nearest(g, 30.0f * Hf / 128.0f);
                b.lit[static_cast<std::size_t>(d)][i] = smooth(1.85f, 0.85f, dist);
                b.glow[static_cast<std::size_t>(d)][i] = std::exp(-dist / 3.2f);
                b.wide[static_cast<std::size_t>(d)][i] = std::exp(-dist / 11.0f);
            }
        }

    b.tube_of_col.assign(static_cast<std::size_t>(W), static_cast<std::int8_t>(-1));
    for (int t = 0; t < tubes; ++t)
        for (int x = std::max(0, b.centre[static_cast<std::size_t>(t)] - b.half);
             x < std::min(W, b.centre[static_cast<std::size_t>(t)] + b.half); ++x)
            b.tube_of_col[static_cast<std::size_t>(x)] = static_cast<std::int8_t>(t);

    // --- Light thrown around the glass and onto the plinth ---------------------
    b.glow_half = b.half + static_cast<int>(std::ceil(0.25f * Hf));
    b.glow_y0 = std::max(0, static_cast<int>(std::floor(0.5f * Hf - (tube_y + 0.50f) * Hf)));
    const int gw = b.glow_w(), gh = H - b.glow_y0;
    b.thrown.assign(static_cast<std::size_t>(gw * gh), 0.0f);
    for (int ly = 0; ly < gh; ++ly)
        for (int lx = 0; lx < gw; ++lx) {
            const float fx = static_cast<float>(lx - b.glow_half) + 0.5f;
            const P uv = uv_of(0.5f * static_cast<float>(W), static_cast<float>(b.glow_y0 + ly) + 0.5f);
            const P p{fx / Hf, uv.y - tube_y};
            float glass_d = sd_round_box(p, {hw, 0.30f}, corner);
            glass_d = std::min(glass_d, sd_segment(p, {0.0f, 0.29f}, {0.0f, 0.32f}) - 0.012f);
            const float outside = 1.0f - smooth(px, -px, glass_d);
            float v = std::exp(-std::max(glass_d, 0.0f) / 0.05f) * outside * 0.10f;
            if (uv.y <= plinth_top)
                v += std::exp(-std::fabs(p.x) * 14.0f) * smooth(plinth_top, plinth_top - 0.14f, uv.y) * 0.22f;
            b.thrown[static_cast<std::size_t>(ly * gw + lx)] = v;
        }

    // --- Colon: two neon dots between each pair of tubes ----------------------
    std::vector<P> dots;
    const auto add_colon = [&](int left) {
        const float cx = 0.5f * static_cast<float>(b.centre[static_cast<std::size_t>(left)] + b.centre[static_cast<std::size_t>(left + 1)]);
        for (float s : {-1.0f, 1.0f}) dots.push_back({cx, 0.5f * Hf - (tube_y + 0.04f + s * 0.058f) * Hf});
        ++b.colons;
    };
    if (six) {
        add_colon(1);
        add_colon(3);
    }
    else {
        add_colon(1);
    }
    const float radius = (six ? 1.0f : 1.2f) * Hf / 128.0f;
    const int reach = static_cast<int>(std::ceil(8.0f * Hf / 128.0f));
    for (const auto& d : dots)
        b.colon.unite(clip({static_cast<int>(d.x) - reach, static_cast<int>(d.y) - reach, static_cast<int>(d.x) + reach + 1,
                            static_cast<int>(d.y) + reach + 1},
                           W, H));
    const int cw = b.colon.x1 - b.colon.x0, ch = b.colon.y1 - b.colon.y0;
    b.colon_core.assign(static_cast<std::size_t>(std::max(0, cw * ch)), 0.0f);
    b.colon_glow.assign(b.colon_core.size(), 0.0f);
    for (int y = b.colon.y0; y < b.colon.y1; ++y)
        for (int x = b.colon.x0; x < b.colon.x1; ++x) {
            float core = 0, glow = 0;
            for (const auto& d : dots) {
                const float dist = AudioKit::len(static_cast<float>(x) + 0.5f - d.x, static_cast<float>(y) + 0.5f - d.y);
                core += smooth(radius + 0.6f, radius - 0.4f, dist);
                glow += std::exp(-dist / (2.0f * Hf / 128.0f));
            }
            const std::size_t i = static_cast<std::size_t>((y - b.colon.y0) * cw + (x - b.colon.x0));
            b.colon_core[i] = std::min(core, 1.0f);
            b.colon_glow[i] = glow;
        }
    return out;
}

}  // namespace

void Art::bake(int width, int height, int tubes)
{
    static std::mutex mutex;
    static std::map<std::tuple<int, int, int>, std::shared_ptr<const Baked>> cache;
    const auto key = std::make_tuple(width, height, tubes);
    last_bake_ms_ = 0.0;
    {
        std::lock_guard lock(mutex);
        if (auto it = cache.find(key); it != cache.end())
            baked_ = it->second;
        else
            baked_.reset();
    }
    if (!baked_) {
        const auto start = std::chrono::steady_clock::now();
        auto fresh = build(width, height, tubes);
        last_bake_ms_ = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        std::lock_guard lock(mutex);
        // Keep only the latest few layouts (4 and 6 tubes at one panel size).
        if (cache.size() >= 4)
            cache.clear();
        baked_ = cache.emplace(key, std::move(fresh)).first->second;
    }
    rgb_.assign(static_cast<std::size_t>(width * height * 3), 0);
}

int Art::tubes() const
{
    return baked_ ? baked_->tubes : 0;
}
int Art::colons() const
{
    return baked_ ? baked_->colons : 0;
}
Rect Art::full() const
{
    return baked_ ? Rect{0, 0, baked_->w, baked_->h} : Rect{};
}

Rect Art::tube_rect(int i) const
{
    if (!baked_ || i < 0 || i >= baked_->tubes)
        return {};
    const int c = baked_->centre[static_cast<std::size_t>(i)];
    return clip({c - baked_->glow_half, baked_->glow_y0, c + baked_->glow_half, baked_->h}, baked_->w, baked_->h);
}

Rect Art::box_rect(int i) const
{
    if (!baked_ || i < 0 || i >= baked_->tubes)
        return {};
    const int c = baked_->centre[static_cast<std::size_t>(i)];
    return clip({c - baked_->half, baked_->box_y0, c + baked_->half, baked_->box_y1}, baked_->w, baked_->h);
}

Rect Art::colon_rect() const
{
    return baked_ ? baked_->colon : Rect{};
}

void Art::compose(Rect r, const std::vector<TubeLook>& looks, float colon, float glow_gain)
{
    if (!baked_)
        return;
    const Baked& b = *baked_;
    r = clip(r, b.w, b.h);
    if (r.empty() || static_cast<int>(looks.size()) < b.tubes)
        return;
    const auto& lut = tonemap();
    const int bw = b.box_w(), gw = b.glow_w();
    const int cw = b.colon.x1 - b.colon.x0;
    const C lead_tint{0.16f * 0.3f, 0.12f * 0.3f, 0.10f * 0.3f};

    for (int y = r.y0; y < r.y1; ++y) {
        const bool in_box_rows = y >= b.box_y0 && y < b.box_y1;
        const bool in_glow_rows = y >= b.glow_y0;
        const bool in_colon_rows = y >= b.colon.y0 && y < b.colon.y1;
        for (int x = r.x0; x < r.x1; ++x) {
            C c = b.bg[static_cast<std::size_t>(y * b.w + x)];

            const int t = b.tube_of_col[static_cast<std::size_t>(x)];
            if (t >= 0 && in_box_rows) {
                const std::size_t li =
                    static_cast<std::size_t>((y - b.box_y0) * bw + (x - (b.centre[static_cast<std::size_t>(t)] - b.half)));
                const TubePixel& tp = b.interior[li];
                if (tp.ins > 0.0f) {
                    const TubeLook& lk = looks[static_cast<std::size_t>(t)];
                    float lit = 0, glow = 0, wide = 0;
                    const auto take = [&](int d, float w) {
                        if (d < 0 || d > 9 || w <= 0.0f)
                            return;
                        lit += b.lit[static_cast<std::size_t>(d)][li] * w;
                        glow += b.glow[static_cast<std::size_t>(d)][li] * w;
                        wide += b.wide[static_cast<std::size_t>(d)][li] * w;
                    };
                    take(lk.a, 1.0f - lk.mix);
                    take(lk.b, lk.mix);
                    const float heat = lk.heat;
                    const float L = clamp01(lit * heat);
                    C tube = tp.t0 + lead_tint * (tp.lead * heat);
                    tube = tube * (1.0f - L) + Hot * L + Orange * (heat * (0.75f * glow + 0.22f * wide));
                    tube = tube * (1.0f - 0.45f * tp.mesh) + Orange * (0.10f * heat * wide * tp.mesh);
                    tube = tube + tp.glass + Orange * (tp.rim * heat * 0.18f);
                    c = mix(c, tube, tp.ins);
                }
            }

            if (in_glow_rows) {
                float thrown = 0;
                for (int j = 0; j < b.tubes; ++j) {
                    const int lx = x - (b.centre[static_cast<std::size_t>(j)] - b.glow_half);
                    if (lx >= 0 && lx < gw)
                        thrown += b.thrown[static_cast<std::size_t>((y - b.glow_y0) * gw + lx)] * looks[static_cast<std::size_t>(j)].heat;
                }
                c = c + Orange * (thrown * glow_gain);
            }

            if (colon > 0.0f && in_colon_rows && x >= b.colon.x0 && x < b.colon.x1) {
                const std::size_t ci = static_cast<std::size_t>((y - b.colon.y0) * cw + (x - b.colon.x0));
                c = c + Hot * (b.colon_core[ci] * colon * 0.85f) + Orange * (b.colon_glow[ci] * colon * 0.45f);
            }

            std::uint8_t* o = &rgb_[static_cast<std::size_t>((y * b.w + x) * 3)];
            o[0] = lut(c.r);
            o[1] = lut(c.g);
            o[2] = lut(c.b);
        }
    }
}

}  // namespace AmbientScenes::Nixie
