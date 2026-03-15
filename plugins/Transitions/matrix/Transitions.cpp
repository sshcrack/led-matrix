#include "Transitions.h"
#include "shared/common/plugin_macros.h"
#include <algorithm>
#include <cmath>

// ─── Helper ───────────────────────────────────────────────────────────────────
static inline uint8_t lerp_u8(uint8_t a, uint8_t b, float t) {
    return static_cast<uint8_t>(std::round(a + (b - a) * t));
}

// ─── BlendTransition ─────────────────────────────────────────────────────────
void BlendTransition::apply(FrameCanvas *dst, FrameCanvas *from, FrameCanvas *to,
                             float alpha, int width, int height) {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
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
                             float alpha, int width, int height) {
    const float edge_px = std::max(1.0f, width * 0.10f);
    const float pivot = alpha * static_cast<float>(width); // leading edge of incoming scene

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
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
                             float alpha, int width, int height) {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
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

// ─── Factory ─────────────────────────────────────────────────────────────────
namespace Plugins {
    vector<std::unique_ptr<TransitionEffect, void (*)(TransitionEffect *)>>
    Transitions::create_transitions() {
        vector<std::unique_ptr<TransitionEffect, void(*)(TransitionEffect*)>> transitions;

        auto add = [&]<typename T>() {
            transitions.push_back({new T(), [](TransitionEffect* p) { delete p; }});
        };

        add.template operator()<BlendTransition>();
        add.template operator()<SwipeTransition>();
        add.template operator()<MorphTransition>();

        return transitions;
    }
}

extern "C" PLUGIN_EXPORT Plugins::Transitions *createTransitions() {
    return new Plugins::Transitions();
}

extern "C" PLUGIN_EXPORT void destroyTransitions(Plugins::Transitions *p) {
    delete p;
}
