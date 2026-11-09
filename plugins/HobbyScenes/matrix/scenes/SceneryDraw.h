#pragma once
// Scenery pieces shared by the seasonal scenes: a broadleaf tree that loses
// its leaves in winter and a side-view deer.
#include "Draw.h"

namespace Hobbies {

// Broadleaf tree standing on (x, base). `crown` is the already lit and season
// tinted leaf colour, `bare` 0..1 how much of the crown is gone (winter),
// `light` the direction the light comes from (-1 left .. 1 right).
inline void broadleaf(Canvas& cv, float x, float base, float height, Rgb crown, Rgb bark, float bare, uint32_t seed,
                      float light = 0.5f, Rgb snow = {}, float snow_amount = 0.0f, float blossom = 0.0f)
{
    const float trunkW = std::max(0.6f, height * 0.045f);
    const float top = base - height;
    cv.capsule(x, base + 0.5f, trunkW * 1.3f, x, base - height * 0.55f, trunkW, bark);
    // Branches: always drawn, visible once the leaves thin out.
    for (int b = 0; b < 5; ++b) {
        const float side = (b % 2 == 0 ? -1.0f : 1.0f) * (0.6f + 0.6f * hash01(seed * 31u + b));
        const float y0 = base - height * (0.42f + 0.1f * b);
        const float y1 = top + height * (0.08f + 0.18f * hash01(seed * 17u + b));
        const float x1 = x + side * height * (0.16f + 0.10f * hash01(seed * 7u + b * 3u));
        cv.capsule(x, y0, trunkW * 0.6f, x1, y1, trunkW * 0.2f, bark);
        // Two twigs off each branch so a bare crown reads as a tree.
        for (int t = 0; t < 2; ++t) {
            const float f = 0.45f + 0.25f * t;
            const float mx = x + (x1 - x) * f, my = y0 + (y1 - y0) * f;
            const float tx = mx + side * height * (0.06f + 0.05f * hash01(seed * 3u + b * 5u + t)) * (t ? -0.6f : 1.0f);
            cv.capsule(mx, my, trunkW * 0.25f, tx, my - height * (0.10f + 0.06f * t), 0.25f, bark, 0.85f);
        }
        if (snow_amount > 0.0f)
            cv.capsule(x, y0 - 0.6f, trunkW * 0.3f, x1, y1 - 0.6f, trunkW * 0.12f, snow, snow_amount * 0.8f);
    }
    const float leafy = 1.0f - std::clamp(bare, 0.0f, 1.0f);
    if (leafy <= 0.02f)
        return;
    // Crown: a cluster of blobs, dark underneath, lit on the light side.
    const float cy = top + height * 0.32f, r = height * 0.24f;
    const Rgb shade = scale(crown, 0.62f), hi = mix(crown, {1.0f, 1.0f, 0.92f}, 0.12f);
    struct Blob {
        float dx, dy, r;
    };
    const Blob blobs[] = {{-0.55f, 0.25f, 0.85f}, {0.55f, 0.25f, 0.85f}, {0.0f, -0.35f, 1.0f}, {-0.3f, 0.45f, 0.8f}, {0.35f, 0.5f, 0.75f}};
    for (const auto& bl : blobs) {
        const float jx = (hash01(seed * 5u + static_cast<uint32_t>(bl.dx * 10 + 20)) - 0.5f) * 0.3f;
        cv.ellipse(x + (bl.dx + jx) * r, cy + bl.dy * r + r * 0.12f, bl.r * r, bl.r * r * 0.85f, 0, shade, leafy);
    }
    for (const auto& bl : blobs)
        cv.ellipse(x + bl.dx * r - light * 0.12f * r, cy + bl.dy * r - 0.08f * r, bl.r * r * 0.86f, bl.r * r * 0.72f, 0, crown,
                   leafy);
    for (int i = 0; i < 3; ++i)
        cv.ellipse(x + (light * 0.35f + (i - 1) * 0.35f) * r, cy - r * (0.45f + 0.12f * (i % 2)), r * 0.38f, r * 0.26f, 0, hi,
                   leafy * 0.8f);
    // Spring blossom: pink and white flecks over the crown.
    if (blossom > 0.02f)
        for (int i = 0; i < static_cast<int>(r * r * 0.5f) + 4; ++i) {
            const float a = kTau * hash01(seed * 41u + i * 3u), d = std::sqrt(hash01(seed * 43u + i * 7u)) * r * 1.1f;
            const Rgb c = hash01(seed + i * 13u) < 0.5f ? Rgb{1.0f, 0.80f, 0.88f} : Rgb{1.0f, 0.95f, 0.97f};
            cv.blend(static_cast<int>(x + std::cos(a) * d * 1.1f), static_cast<int>(cy + std::sin(a) * d * 0.8f), mix(c, crown, 0.25f),
                     blossom * leafy);
        }
    if (snow_amount > 0.0f)
        cv.ellipse(x, cy - r * 0.95f, r * 0.9f, r * 0.28f, 0, snow, snow_amount * leafy);
}

// Side-view deer; (x, ground) under the body, `s` 1 = about 16 px long.
// `phase` drives the walk, `graze` 0..1 lowers the head to the grass.
inline void deer(Canvas& cv, float x, float ground, float s, float facing, float phase, float walking, float graze, Rgb coat,
                 Rgb belly, Rgb dark)
{
    const float bodyY = ground - 9.0f * s;
    auto leg = [&](float hx, float ph, Rgb c) {
        const float swing = walking * std::sin(kTau * ph) * 0.45f;
        const float lift = walking * std::max(0.0f, std::cos(kTau * ph)) * 1.4f * s;
        const float kx = x + hx * facing + std::sin(swing) * 3.5f * s * facing;
        const float ky = bodyY + 4.2f * s;
        const float fx = kx + std::sin(swing * 0.4f) * 1.5f * s * facing;
        cv.capsule(x + hx * facing, bodyY + 1.0f * s, 0.85f * s, kx, ky, 0.55f * s, c);
        cv.capsule(kx, ky, 0.5f * s, fx, ground - 0.4f * s - lift, 0.4f * s, c);
    };
    const Rgb farC = scale(coat, 0.72f);
    leg(-4.6f * s, phase + 0.5f, farC);
    leg(4.0f * s, phase, farC);
    // Body, lighter belly, white rump patch and tail.
    cv.ellipse(x, bodyY, 6.4f * s, 3.0f * s, 0.04f * facing, coat);
    cv.ellipse(x + 0.4f * s * facing, bodyY + 1.7f * s, 5.0f * s, 1.2f * s, 0, belly, 0.8f);
    cv.ellipse(x - 5.6f * s * facing, bodyY - 0.6f * s, 1.3f * s, 1.6f * s, 0, mix(belly, {1, 1, 1}, 0.5f));
    cv.capsule(x - 6.3f * s * facing, bodyY - 1.8f * s, 0.6f * s, x - 6.9f * s * facing, bodyY - 0.6f * s, 0.45f * s,
               {0.95f, 0.93f, 0.88f});
    leg(-4.0f * s, phase, coat);
    leg(4.6f * s, phase + 0.5f, coat);
    // Neck and head: up and alert, or down grazing.
    const float g = std::clamp(graze, 0.0f, 1.0f);
    const Point neckBase{x + 5.0f * s * facing, bodyY - 1.2f * s};
    const Point headUp{x + 8.2f * s * facing, bodyY - 7.6f * s}, headDown{x + 9.6f * s * facing, ground - 1.6f * s};
    const Point head{headUp.x + (headDown.x - headUp.x) * g, headUp.y + (headDown.y - headUp.y) * g};
    cv.capsule(neckBase.x, neckBase.y, 1.7f * s, head.x - 0.8f * s * facing, head.y + 0.6f * s, 1.0f * s, coat);
    const float tilt = (0.35f + 1.0f * g) * facing;
    cv.ellipse(head.x + 0.6f * s * facing, head.y, 2.2f * s, 1.15f * s, tilt, coat);
    cv.ellipse(head.x + 2.4f * s * facing * (1 - 0.3f * g), head.y + (0.5f + 1.2f * g) * s, 0.7f * s, 0.6f * s, 0, dark);
    // Ears.
    const Point ear{head.x - 0.6f * s * facing, head.y - 1.0f * s + g * 0.6f * s};
    cv.capsule(ear.x, ear.y, 0.45f * s, ear.x - 1.6f * s * facing, ear.y - 1.5f * s * (1 - 0.5f * g), 0.5f * s, coat);
    cv.set(static_cast<int>(head.x + 0.7f * s * facing), static_cast<int>(head.y - 0.3f * s), dark);
}

}  // namespace Hobbies
