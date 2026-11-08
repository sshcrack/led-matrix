#pragma once
// Side-view human figure built from capsules. Angles are radians measured from
// straight down; positive swings toward the facing direction (so a thigh at
// +pi/2 points forward horizontally, an arm at +pi points straight up).
#include "Draw.h"

namespace Hobbies {

struct Pose {
    float lean = 0.0f;                         // torso tilt toward facing
    float thigh[2] = {0, 0}, shin[2] = {0, 0}; // [0] far leg, [1] near leg
    float upperArm[2] = {0, 0}, foreArm[2] = {0, 0};
    float headTilt = 0.0f;                     // + looks up
};

struct Look {
    Rgb skin{0.80f, 0.56f, 0.42f};
    Rgb hair{0.20f, 0.12f, 0.08f};
    Rgb shirt{0.25f, 0.45f, 0.70f};
    Rgb pants{0.20f, 0.22f, 0.28f};
    Rgb shoes{0.10f, 0.08f, 0.08f};
    bool backpack = false;
    Rgb pack{0.75f, 0.30f, 0.15f};
    bool cap = false;
    Rgb capColor{0.85f, 0.25f, 0.20f};
    float shade = 0.75f;  // brightness of the far limbs
};

struct FigureOut {
    Point hand[2]{};  // wrist positions in screen space
    Point head{};
    Point hip{};
};

constexpr float kThigh = 4.2f, kShin = 4.2f, kTorso = 5.8f, kUpperArm = 3.0f, kForeArm = 2.8f, kHeadR = 1.75f;

inline Pose blend(const Pose& a, const Pose& b, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    auto l = [t](float x, float y) { return x + (y - x) * t; };
    Pose p;
    p.lean = l(a.lean, b.lean);
    p.headTilt = l(a.headTilt, b.headTilt);
    for (int i = 0; i < 2; ++i) {
        p.thigh[i] = l(a.thigh[i], b.thigh[i]);
        p.shin[i] = l(a.shin[i], b.shin[i]);
        p.upperArm[i] = l(a.upperArm[i], b.upperArm[i]);
        p.foreArm[i] = l(a.foreArm[i], b.foreArm[i]);
    }
    return p;
}

// One walk cycle per phase 0..1 (two steps). `stride` ~0.35 stroll, 0.5 brisk.
inline Pose walk_pose(float phase, float stride = 0.4f, float lean = 0.05f)
{
    Pose p;
    p.lean = lean;
    for (int i = 0; i < 2; ++i) {
        const float a = kTau * (phase + 0.5f * i);
        p.thigh[i] = stride * std::sin(a);
        // The swinging leg (thigh moving forward) folds its knee.
        const float swing = std::max(0.0f, std::cos(a));
        p.shin[i] = p.thigh[i] - 1.1f * stride * 2.0f * swing * swing;
        // Arms swing against the opposite leg.
        p.upperArm[i] = -0.8f * stride * std::sin(a + kPi);
        p.foreArm[i] = p.upperArm[i] + 0.35f;
    }
    // Arms are indexed like the legs: arm i swings with leg 1-i.
    std::swap(p.upperArm[0], p.upperArm[1]);
    std::swap(p.foreArm[0], p.foreArm[1]);
    return p;
}

// (x, ground) is the ground point under the hips. `lift` raises the figure off
// the ground (jumps); otherwise the lower foot rests on the ground.
inline FigureOut figure(Canvas& cv, float x, float ground, float s, float facing, const Pose& p, const Look& look, float lift = 0.0f)
{
    auto dir = [&](float a) -> Point { return {std::sin(a) * facing, std::cos(a)}; };
    float drop = 0.0f;
    for (int i = 0; i < 2; ++i) drop = std::max(drop, std::cos(p.thigh[i]) * kThigh + std::cos(p.shin[i]) * kShin);
    FigureOut out;
    const Point hip{x, ground - (drop + 0.5f + lift) * s};
    out.hip = hip;
    const Point td = dir(kPi + p.lean);  // torso points up, tilted forward
    const Point shoulder{hip.x + td.x * kTorso * s, hip.y + td.y * kTorso * s};
    const Point hd = dir(kPi + p.lean * 0.6f);
    out.head = {shoulder.x + hd.x * (kHeadR + 0.6f) * s, shoulder.y + hd.y * (kHeadR + 0.6f) * s};

    auto seg = [&](Point a, float ang, float len) -> Point {
        const Point d = dir(ang);
        return {a.x + d.x * len * s, a.y + d.y * len * s};
    };
    auto cap = [&](Point a, float ra, Point b, float rb, Rgb c) { cv.capsule(a.x, a.y, ra * s, b.x, b.y, rb * s, c); };

    auto arm = [&](int i) {
        const Rgb shirt = i == 0 ? scale(look.shirt, look.shade) : look.shirt;
        const Rgb skin = i == 0 ? scale(look.skin, look.shade) : look.skin;
        const Point sh{shoulder.x, shoulder.y + 0.6f * s};
        const Point el = seg(sh, p.upperArm[i], kUpperArm);
        const Point wr = seg(el, p.foreArm[i], kForeArm);
        cap(sh, 0.75f, el, 0.65f, shirt);
        cap(el, 0.6f, wr, 0.55f, skin);
        out.hand[i] = wr;
    };
    auto leg = [&](int i) {
        const Rgb pants = i == 0 ? scale(look.pants, look.shade) : look.pants;
        const Rgb shoes = i == 0 ? scale(look.shoes, look.shade) : look.shoes;
        const Point kn = seg(hip, p.thigh[i], kThigh);
        const Point an = seg(kn, p.shin[i], kShin);
        cap(hip, 1.05f, kn, 0.85f, pants);
        cap(kn, 0.8f, an, 0.65f, pants);
        cap({an.x - 0.3f * facing * s, an.y + 0.2f * s}, 0.55f, {an.x + 1.1f * facing * s, an.y + 0.25f * s}, 0.5f, shoes);
    };

    arm(0);
    leg(0);
    if (look.backpack) {
        const Point back = seg({hip.x + td.x * kTorso * 0.55f * s, hip.y + td.y * kTorso * 0.55f * s}, -kPi / 2 + p.lean, 1.6f);
        cv.ellipse(back.x, back.y, 1.6f * s, 3.0f * s, p.lean * facing, look.pack);
    }
    cap(hip, 1.25f, shoulder, 1.45f, look.shirt);
    leg(1);
    cv.ellipse(out.head.x, out.head.y, kHeadR * s, kHeadR * s, 0, look.skin);
    // Hair on the back of the head, optional cap with a brim facing forward.
    const Point hb{out.head.x - 0.55f * facing * s, out.head.y - 0.45f * s};
    cv.ellipse(hb.x, hb.y, 1.25f * s, 1.15f * s, 0, look.hair);
    if (look.cap) {
        cv.ellipse(out.head.x, out.head.y - 0.8f * s, 1.85f * s, 1.0f * s, 0, look.capColor);
        const float tilt = p.headTilt;
        cap({out.head.x + 0.8f * facing * s, out.head.y - 0.7f * s}, 0.4f,
            {out.head.x + 2.6f * facing * s, out.head.y - (0.5f + tilt * 1.2f) * s}, 0.35f, look.capColor);
    }
    arm(1);
    return out;
}

// Pine tree silhouette (stacked tiers) standing on (x, base).
inline void pine(Canvas& cv, float x, float base, float height, Rgb col, float alpha = 1.0f)
{
    const float w = height * 0.36f;
    const int y0 = static_cast<int>(base - height);
    for (int y = std::max(0, y0); y < std::min(cv.h, static_cast<int>(base) + 1); ++y) {
        const float v = (y + 0.5f - (base - height)) / height;  // 0 top .. 1 base
        // Three tiers: width grows within each tier, then steps back in.
        const float tier = fract(v * 3.0f);
        const float half = w * (0.25f + 0.75f * v) * (0.55f + 0.45f * tier);
        for (int px = static_cast<int>(x - half - 1); px <= static_cast<int>(x + half + 1); ++px) {
            const float edge = half - std::fabs(px + 0.5f - x);
            cv.blend(px, y, col, std::clamp(edge + 0.5f, 0.0f, 1.0f) * alpha);
        }
    }
    cv.capsule(x, base - height * 0.1f, 0.5f, x, base + 1.0f, 0.5f, mix(col, {0.1f, 0.05f, 0.02f}, 0.5f), alpha);
}

}  // namespace Hobbies
