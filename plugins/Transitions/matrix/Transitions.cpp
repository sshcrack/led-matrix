#include "Transitions.h"
#include "shared/common/plugin_macros.h"
#include <algorithm>
#include <cmath>
#include <cstddef>

// ─── Helpers ───────────────────────────────────────────────────────────────────
static inline uint8_t lerp_u8(uint8_t a, uint8_t b, float t)
{
    return static_cast<uint8_t>(std::round(a + (b - a) * t));
}

static inline std::size_t px(int x, int y, int width)
{
    return (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)) * 3;
}

static inline void put(uint8_t* dst, std::size_t i, uint8_t r, uint8_t g, uint8_t b)
{
    dst[i] = r;
    dst[i + 1] = g;
    dst[i + 2] = b;
}

// `mask_fn(x, y, to_pixel, alpha)` returns how much of the incoming pixel shows.
template<typename F>
static void apply_pixel_loop(uint8_t* dst, const uint8_t* from, const uint8_t* to,
                             float alpha, int width, int height, F&& mask_fn)
{
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            const std::size_t i = px(x, y, width);
            const uint8_t* t = to + i;
            const float la = mask_fn(x, y, t, alpha);
            if (la >= 1.0f) {
                put(dst, i, t[0], t[1], t[2]);
                continue;
            }
            const uint8_t* f = from + i;
            if (la <= 0.0f) {
                put(dst, i, f[0], f[1], f[2]);
                continue;
            }
            put(dst, i, lerp_u8(f[0], t[0], la), lerp_u8(f[1], t[1], la), lerp_u8(f[2], t[2], la));
        }
    }
}

static inline float hash01(int x, int y)
{
    uint32_t h = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(y) * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= (h >> 16);
    return static_cast<float>(h & 0x00FFFFFFu) / 16777215.0f;
}

static constexpr uint8_t BAYER_8X8[8][8] = {
    {0, 48, 12, 60, 3, 51, 15, 63},
    {32, 16, 44, 28, 35, 19, 47, 31},
    {8, 56, 4, 52, 11, 59, 7, 55},
    {40, 24, 36, 20, 43, 27, 39, 23},
    {2, 50, 14, 62, 1, 49, 13, 61},
    {34, 18, 46, 30, 33, 17, 45, 29},
    {10, 58, 6, 54, 9, 57, 5, 53},
    {42, 26, 38, 22, 41, 25, 37, 21}};

// ─── BlendTransition ─────────────────────────────────────────────────────────
void BlendTransition::apply(uint8_t *dst, const uint8_t *from, const uint8_t *to,
                            float alpha, int width, int height)
{
    apply_pixel_loop(dst, from, to, alpha, width, height,
        [](int, int, const uint8_t*, float a) {
            return a;
        });
}

// ─── SwipeTransition ─────────────────────────────────────────────────────────
void SwipeTransition::apply(uint8_t *dst, const uint8_t *from, const uint8_t *to,
                            float alpha, int width, int height)
{
    const float edge_px = std::max(1.0f, width * 0.10f);
    const float pivot = alpha * static_cast<float>(width);
    apply_pixel_loop(dst, from, to, alpha, width, height,
        [edge_px, pivot](int x, int, const uint8_t*, float a) {
            if (a >= 1.0f) return 1.0f;
            const float dist = pivot - static_cast<float>(x);
            return std::clamp(dist / edge_px * 0.5f + 0.5f, 0.0f, 1.0f);
        });
}

// ─── MorphTransition ─────────────────────────────────────────────────────────
void MorphTransition::apply(uint8_t *dst, const uint8_t *from, const uint8_t *to,
                            float alpha, int width, int height)
{
    apply_pixel_loop(dst, from, to, alpha, width, height,
        [](int, int, const uint8_t* t, float a) {
            if (a >= 1.0f) return 1.0f;
            const float lum = (0.299f * t[0] + 0.587f * t[1] + 0.114f * t[2]) / 255.0f;
            const float shift = lum * 0.5f;
            return std::clamp((a - (1.0f - lum) * 0.5f) / (1.0f - shift * 0.5f + 0.001f), 0.0f, 1.0f);
        });
}

// ─── RadialRevealTransition ──────────────────────────────────────────────────
void RadialRevealTransition::apply(uint8_t *dst, const uint8_t *from, const uint8_t *to,
                                   float alpha, int width, int height)
{
    const float cx = (static_cast<float>(width) - 1.0f) * 0.5f;
    const float cy = (static_cast<float>(height) - 1.0f) * 0.5f;
    const float max_dx = std::max(cx, static_cast<float>(width) - 1.0f - cx);
    const float max_dy = std::max(cy, static_cast<float>(height) - 1.0f - cy);
    const float max_dist = std::sqrt(max_dx * max_dx + max_dy * max_dy) + 0.0001f;
    const float soft = 0.10f;

    apply_pixel_loop(dst, from, to, alpha, width, height,
        [cx, cy, max_dist, soft](int x, int y, const uint8_t*, float a) {
            if (a >= 1.0f) return 1.0f;
            const float dx = static_cast<float>(x) - cx;
            const float dy = static_cast<float>(y) - cy;
            const float threshold = std::sqrt(dx * dx + dy * dy) / max_dist;
            return std::clamp((a - threshold + soft) / (2.0f * soft), 0.0f, 1.0f);
        });
}

// ─── CheckerRevealTransition ─────────────────────────────────────────────────
void CheckerRevealTransition::apply(uint8_t *dst, const uint8_t *from, const uint8_t *to,
                                    float alpha, int width, int height)
{
    apply_pixel_loop(dst, from, to, alpha, width, height,
        [](int x, int y, const uint8_t*, float a) {
            const int tile_phase = ((x >> 3) + (y >> 3)) & 1;
            const float stage_start = tile_phase == 0 ? 0.0f : 0.5f;
            return std::clamp((a - stage_start) * 2.0f, 0.0f, 1.0f);
        });
}

// ─── OrderedDissolveTransition ───────────────────────────────────────────────
void OrderedDissolveTransition::apply(uint8_t *dst, const uint8_t *from, const uint8_t *to,
                                      float alpha, int width, int height)
{
    const float progress = std::clamp(alpha, 0.0f, 1.0f) * 64.0f;
    apply_pixel_loop(dst, from, to, alpha, width, height,
        [progress](int x, int y, const uint8_t*, float) {
            return std::clamp(progress - static_cast<float>(BAYER_8X8[y & 7][x & 7]), 0.0f, 1.0f);
        });
}

// ─── RandomDissolveTransition ────────────────────────────────────────────────
void RandomDissolveTransition::apply(uint8_t *dst, const uint8_t *from, const uint8_t *to,
                                     float alpha, int width, int height)
{
    const float soft = 0.06f;
    apply_pixel_loop(dst, from, to, alpha, width, height,
        [soft](int x, int y, const uint8_t*, float a) {
            if (a >= 1.0f) return 1.0f;
            const float threshold = hash01(x, y);
            return std::clamp((a - threshold + soft) / (2.0f * soft), 0.0f, 1.0f);
        });
}

// ─── ZoomBlendTransition ─────────────────────────────────────────────────────
void ZoomBlendTransition::apply(uint8_t *dst, const uint8_t *from, const uint8_t *to,
                                float alpha, int width, int height)
{
    const float cx = (static_cast<float>(width) - 1.0f) * 0.5f;
    const float cy = (static_cast<float>(height) - 1.0f) * 0.5f;
    const float zoom = 1.25f - 0.25f * std::clamp(alpha, 0.0f, 1.0f);

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            const std::size_t i = px(x, y, width);
            const uint8_t* f = from + i;

            const float sample_xf = (static_cast<float>(x) - cx) / zoom + cx;
            const float sample_yf = (static_cast<float>(y) - cy) / zoom + cy;
            const int sx = std::clamp(static_cast<int>(std::round(sample_xf)), 0, width - 1);
            const int sy = std::clamp(static_cast<int>(std::round(sample_yf)), 0, height - 1);

            const uint8_t* t = to + px(sx, sy, width);

            put(dst, i, lerp_u8(f[0], t[0], alpha), lerp_u8(f[1], t[1], alpha), lerp_u8(f[2], t[2], alpha));
        }
    }
}



// ─── Glitch Cut ──────────────────────────────────────────────────────────────
void GlitchTransition::apply(uint8_t *dst, const uint8_t *from, const uint8_t *to,
                             float alpha, int width, int height)
{
    const float a = std::clamp(alpha, 0.0f, 1.0f);
    const int phase = static_cast<int>(a * 31.0f);
    for (int y = 0; y < height; ++y)
    {
        const uint32_t row_hash = static_cast<uint32_t>((y + 17) * 2654435761U) ^ static_cast<uint32_t>(phase * 2246822519U);
        const int jitter = ((row_hash >> 8) & 7U) < 3U
            ? static_cast<int>(row_hash % 9U) - 4
            : 0;
        for (int x = 0; x < width; ++x)
        {
            const float threshold = hash01(x >> 2, y >> 2);
            const bool incoming = a >= threshold;
            const uint8_t* src = incoming ? to : from;
            const int sx = std::clamp(x + (incoming ? -jitter : jitter), 0, width - 1);
            const uint8_t* s = src + px(sx, y, width);
            uint8_t r = s[0], g = s[1], b = s[2];
            if (jitter != 0) {
                r = src[px(std::clamp(sx - 1, 0, width - 1), y, width)];
                b = src[px(std::clamp(sx + 1, 0, width - 1), y, width) + 2];
            }
            put(dst, px(x, y, width), r, g, b);
        }
    }
}

// ─── CRT Collapse ────────────────────────────────────────────────────────────
void CrtCollapseTransition::apply(uint8_t *dst, const uint8_t *from, const uint8_t *to,
                                  float alpha, int width, int height)
{
    const float a = std::clamp(alpha, 0.0f, 1.0f);
    const float half_h = std::max(1.0f, (height - 1) * 0.5f);
    const float cy = (height - 1) * 0.5f;
    const bool incoming = a >= 0.5f;
    const float local = incoming ? (a - 0.5f) * 2.0f : a * 2.0f;
    const float scale = incoming ? std::max(0.01f, local) : std::max(0.01f, 1.0f - local);
    const uint8_t* src = incoming ? to : from;

    std::fill(dst, dst + px(0, height, width), uint8_t{0});
    for (int y = 0; y < height; ++y)
    {
        const float normalized_y = (static_cast<float>(y) - cy) / half_h;
        if (std::abs(normalized_y) > scale) continue;
        const float source_normalized = normalized_y / scale;
        const int sy = std::clamp(static_cast<int>(std::lround(cy + source_normalized * half_h)), 0, height - 1);
        const float edge = 1.0f - std::clamp(std::abs(normalized_y) / scale, 0.0f, 1.0f);
        const float line_glow = std::clamp((0.16f - scale) / 0.16f, 0.0f, 1.0f) * edge;
        for (int x = 0; x < width; ++x)
        {
            const uint8_t* s = src + px(x, sy, width);
            uint8_t r = s[0], g = s[1], b = s[2];
            r = static_cast<uint8_t>(std::min(255.0f, r + line_glow * (255.0f - r)));
            g = static_cast<uint8_t>(std::min(255.0f, g + line_glow * (255.0f - g)));
            b = static_cast<uint8_t>(std::min(255.0f, b + line_glow * (255.0f - b)));
            put(dst, px(x, y, width), r, g, b);
        }
    }
}

// ─── Block Dissolve ──────────────────────────────────────────────────────────
void BlockDissolveTransition::apply(uint8_t *dst, const uint8_t *from, const uint8_t *to,
                                    float alpha, int width, int height)
{
    const float a = std::clamp(alpha, 0.0f, 1.0f);
    apply_pixel_loop(dst, from, to, a, width, height,
        [](int x, int y, const uint8_t*, float progress) {
            const float threshold = hash01(x >> 3, y >> 3);
            return progress >= threshold ? 1.0f : 0.0f;
        });
}

// ─── Factory ─────────────────────────────────────────────────────────────────
namespace Plugins
{
    vector<std::unique_ptr<TransitionEffect>>
    Transitions::create_transitions()
    {
        vector<std::unique_ptr<TransitionEffect>> transitions;

        auto add = [&]<typename T>()
        {
            transitions.push_back(std::make_unique<T>());
        };

        add.template operator()<BlendTransition>();
        add.template operator()<SwipeTransition>();
        add.template operator()<MorphTransition>();
        add.template operator()<RadialRevealTransition>();
        add.template operator()<CheckerRevealTransition>();
        add.template operator()<OrderedDissolveTransition>();
        add.template operator()<RandomDissolveTransition>();
        add.template operator()<ZoomBlendTransition>();
        add.template operator()<GlitchTransition>();
        add.template operator()<CrtCollapseTransition>();
        add.template operator()<BlockDissolveTransition>();

        return transitions;
    }
}

REGISTER_PLUGIN(Transitions, Plugins::Transitions)
