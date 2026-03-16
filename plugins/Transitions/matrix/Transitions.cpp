#include "Transitions.h"
#include "shared/common/plugin_macros.h"
#include <algorithm>
#include <cmath>

// ─── Helper ───────────────────────────────────────────────────────────────────
static inline uint8_t lerp_u8(uint8_t a, uint8_t b, float t)
{
    return static_cast<uint8_t>(std::round(a + (b - a) * t));
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
void BlendTransition::apply(FrameCanvas *dst, FrameCanvas *from, FrameCanvas *to,
                            float alpha, int width, int height)
{
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            uint8_t fr = 0, fg = 0, fb = 0;
            uint8_t tr = 0, tg = 0, tb = 0;
            from->GetPixel(x, y, &fr, &fg, &fb);
            to->GetPixel(x, y, &tr, &tg, &tb);
            dst->SetPixel(x, y, lerp_u8(fr, tr, alpha),
                          lerp_u8(fg, tg, alpha),
                          lerp_u8(fb, tb, alpha));
        }
    }
}

// ─── SwipeTransition ─────────────────────────────────────────────────────────
// The incoming scene slides in column by column from the left.
// A soft blend edge (width = ~10% of total width) smooths the boundary.
void SwipeTransition::apply(FrameCanvas *dst, FrameCanvas *from, FrameCanvas *to,
                            float alpha, int width, int height)
{
    const float edge_px = std::max(1.0f, width * 0.10f);
    const float pivot = alpha * static_cast<float>(width); // leading edge of incoming scene

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            // t = 0 → fully 'from'  t = 1 → fully 'to'
            const float dist = pivot - static_cast<float>(x);
            const float t = std::clamp(dist / edge_px * 0.5f + 0.5f, 0.0f, 1.0f);

            uint8_t fr = 0, fg = 0, fb = 0;
            uint8_t tr = 0, tg = 0, tb = 0;
            from->GetPixel(x, y, &fr, &fg, &fb);
            to->GetPixel(x, y, &tr, &tg, &tb);
            dst->SetPixel(x, y, lerp_u8(fr, tr, t),
                          lerp_u8(fg, tg, t),
                          lerp_u8(fb, tb, t));
        }
    }
}

// ─── MorphTransition ─────────────────────────────────────────────────────────
// Pixels of the incoming scene with higher luminance appear earlier.
// Each pixel's local alpha is shifted by its brightness so bright areas
// transition first and dark areas last — giving an organic dissolve.
void MorphTransition::apply(FrameCanvas *dst, FrameCanvas *from, FrameCanvas *to,
                            float alpha, int width, int height)
{
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            uint8_t fr = 0, fg = 0, fb = 0;
            uint8_t tr = 0, tg = 0, tb = 0;
            from->GetPixel(x, y, &fr, &fg, &fb);
            to->GetPixel(x, y, &tr, &tg, &tb);

            // Luminance of the incoming pixel [0..1]
            const float lum = (0.299f * tr + 0.587f * tg + 0.114f * tb) / 255.0f;

            // Shift alpha by luminance: bright pixels of 'to' lead the transition
            // local_alpha remapped so that a fully-bright pixel transitions first
            // and a fully-dark pixel transitions last, by half the total range.
            const float shift = lum * 0.5f;
            const float local_alpha = std::clamp((alpha - (1.0f - lum) * 0.5f) / (1.0f - shift * 0.5f + 0.001f), 0.0f, 1.0f);

            dst->SetPixel(x, y, lerp_u8(fr, tr, local_alpha),
                          lerp_u8(fg, tg, local_alpha),
                          lerp_u8(fb, tb, local_alpha));
        }
    }
}

// ─── RadialRevealTransition ──────────────────────────────────────────────────
void RadialRevealTransition::apply(FrameCanvas *dst, FrameCanvas *from, FrameCanvas *to,
                                   float alpha, int width, int height)
{
    const float cx = (static_cast<float>(width) - 1.0f) * 0.5f;
    const float cy = (static_cast<float>(height) - 1.0f) * 0.5f;
    const float max_dx = std::max(cx, static_cast<float>(width) - 1.0f - cx);
    const float max_dy = std::max(cy, static_cast<float>(height) - 1.0f - cy);
    const float max_dist = std::sqrt(max_dx * max_dx + max_dy * max_dy) + 0.0001f;
    const float soft = 0.10f;

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            uint8_t fr = 0, fg = 0, fb = 0;
            uint8_t tr = 0, tg = 0, tb = 0;
            from->GetPixel(x, y, &fr, &fg, &fb);
            to->GetPixel(x, y, &tr, &tg, &tb);

            const float dx = static_cast<float>(x) - cx;
            const float dy = static_cast<float>(y) - cy;
            const float threshold = std::sqrt(dx * dx + dy * dy) / max_dist;
            const float local_alpha = std::clamp((alpha - threshold + soft) / (2.0f * soft), 0.0f, 1.0f);

            dst->SetPixel(x, y,
                          lerp_u8(fr, tr, local_alpha),
                          lerp_u8(fg, tg, local_alpha),
                          lerp_u8(fb, tb, local_alpha));
        }
    }
}

// ─── CheckerRevealTransition ─────────────────────────────────────────────────
void CheckerRevealTransition::apply(FrameCanvas *dst, FrameCanvas *from, FrameCanvas *to,
                                    float alpha, int width, int height)
{
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            uint8_t fr = 0, fg = 0, fb = 0;
            uint8_t tr = 0, tg = 0, tb = 0;
            from->GetPixel(x, y, &fr, &fg, &fb);
            to->GetPixel(x, y, &tr, &tg, &tb);

            const int tile_phase = ((x >> 3) + (y >> 3)) & 1;
            const float stage_start = tile_phase == 0 ? 0.0f : 0.5f;
            const float stage_alpha = std::clamp((alpha - stage_start) * 2.0f, 0.0f, 1.0f);

            dst->SetPixel(x, y,
                          lerp_u8(fr, tr, stage_alpha),
                          lerp_u8(fg, tg, stage_alpha),
                          lerp_u8(fb, tb, stage_alpha));
        }
    }
}

// ─── OrderedDissolveTransition ───────────────────────────────────────────────
void OrderedDissolveTransition::apply(FrameCanvas *dst, FrameCanvas *from, FrameCanvas *to,
                                      float alpha, int width, int height)
{
    const float progress = std::clamp(alpha, 0.0f, 1.0f) * 64.0f;

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            uint8_t fr = 0, fg = 0, fb = 0;
            uint8_t tr = 0, tg = 0, tb = 0;
            from->GetPixel(x, y, &fr, &fg, &fb);
            to->GetPixel(x, y, &tr, &tg, &tb);

            const float threshold = static_cast<float>(BAYER_8X8[y & 7][x & 7]);
            const float local_alpha = std::clamp(progress - threshold, 0.0f, 1.0f);

            dst->SetPixel(x, y,
                          lerp_u8(fr, tr, local_alpha),
                          lerp_u8(fg, tg, local_alpha),
                          lerp_u8(fb, tb, local_alpha));
        }
    }
}

// ─── RandomDissolveTransition ────────────────────────────────────────────────
void RandomDissolveTransition::apply(FrameCanvas *dst, FrameCanvas *from, FrameCanvas *to,
                                     float alpha, int width, int height)
{
    const float soft = 0.06f;

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            uint8_t fr = 0, fg = 0, fb = 0;
            uint8_t tr = 0, tg = 0, tb = 0;
            from->GetPixel(x, y, &fr, &fg, &fb);
            to->GetPixel(x, y, &tr, &tg, &tb);

            const float threshold = hash01(x, y);
            const float local_alpha = std::clamp((alpha - threshold + soft) / (2.0f * soft), 0.0f, 1.0f);

            dst->SetPixel(x, y,
                          lerp_u8(fr, tr, local_alpha),
                          lerp_u8(fg, tg, local_alpha),
                          lerp_u8(fb, tb, local_alpha));
        }
    }
}

// ─── ZoomBlendTransition ─────────────────────────────────────────────────────
void ZoomBlendTransition::apply(FrameCanvas *dst, FrameCanvas *from, FrameCanvas *to,
                                float alpha, int width, int height)
{
    const float cx = (static_cast<float>(width) - 1.0f) * 0.5f;
    const float cy = (static_cast<float>(height) - 1.0f) * 0.5f;
    const float zoom = 1.25f - 0.25f * std::clamp(alpha, 0.0f, 1.0f);

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            uint8_t fr = 0, fg = 0, fb = 0;
            from->GetPixel(x, y, &fr, &fg, &fb);

            const float sample_xf = (static_cast<float>(x) - cx) / zoom + cx;
            const float sample_yf = (static_cast<float>(y) - cy) / zoom + cy;
            const int sx = std::clamp(static_cast<int>(std::round(sample_xf)), 0, width - 1);
            const int sy = std::clamp(static_cast<int>(std::round(sample_yf)), 0, height - 1);

            uint8_t tr = 0, tg = 0, tb = 0;
            to->GetPixel(sx, sy, &tr, &tg, &tb);

            dst->SetPixel(x, y,
                          lerp_u8(fr, tr, alpha),
                          lerp_u8(fg, tg, alpha),
                          lerp_u8(fb, tb, alpha));
        }
    }
}

// ─── Factory ─────────────────────────────────────────────────────────────────
namespace Plugins
{
    vector<std::unique_ptr<TransitionEffect, void (*)(TransitionEffect *)>>
    Transitions::create_transitions()
    {
        vector<std::unique_ptr<TransitionEffect, void (*)(TransitionEffect *)>> transitions;

        auto add = [&]<typename T>()
        {
            transitions.push_back({new T(), [](TransitionEffect *p)
                                   { delete p; }});
        };

        add.template operator()<BlendTransition>();
        add.template operator()<SwipeTransition>();
        add.template operator()<MorphTransition>();
        add.template operator()<RadialRevealTransition>();
        add.template operator()<CheckerRevealTransition>();
        add.template operator()<OrderedDissolveTransition>();
        add.template operator()<RandomDissolveTransition>();
        add.template operator()<ZoomBlendTransition>();

        return transitions;
    }
}

extern "C" PLUGIN_EXPORT Plugins::Transitions *createTransitions()
{
    return new Plugins::Transitions();
}

extern "C" PLUGIN_EXPORT void destroyTransitions(Plugins::Transitions *p)
{
    delete p;
}
