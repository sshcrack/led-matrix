#pragma once
// Tiny software rasteriser + procedural horse silhouette. Everything is drawn
// into a float RGB buffer with capsule/ellipse primitives, so cost is a few
// hundred pixels per horse.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace HorseScenes {
constexpr float kPi = 3.14159265358979323846f;

struct Rgb {
    float r = 0, g = 0, b = 0;
};

inline Rgb mix(Rgb a, Rgb b, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
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
    void blend(int x, int y, Rgb c, float a)
    {
        if (x < 0 || y < 0 || x >= w || y >= h || a <= 0.0f)
            return;
        a = std::min(a, 1.0f);
        float* p = &px[static_cast<size_t>((y * w + x) * 3)];
        p[0] += (c.r - p[0]) * a;
        p[1] += (c.g - p[1]) * a;
        p[2] += (c.b - p[2]) * a;
    }
    void add(int x, int y, Rgb c, float a)
    {
        if (x < 0 || y < 0 || x >= w || y >= h)
            return;
        float* p = &px[static_cast<size_t>((y * w + x) * 3)];
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
        const float rm = std::max(rx, ry) + 1.0f;
        const float ca = std::cos(angle), sa = std::sin(angle);
        for (int y = std::max(0, static_cast<int>(cy - rm)); y <= std::min(h - 1, static_cast<int>(cy + rm)); ++y)
            for (int x = std::max(0, static_cast<int>(cx - rm)); x <= std::min(w - 1, static_cast<int>(cx + rm)); ++x) {
                const float qx = x + 0.5f - cx, qy = y + 0.5f - cy;
                const float u = (qx * ca + qy * sa) / rx, v = (-qx * sa + qy * ca) / ry;
                const float d = (std::sqrt(u * u + v * v) - 1.0f) * std::min(rx, ry);
                blend(x, y, c, std::clamp(0.5f - d, 0.0f, 1.0f) * alpha);
            }
    }
};

enum class Gait { Gallop, Walk, Graze, Fly };
struct Point {
    float x, y;
};
constexpr float gallopStride = 42.0f;
constexpr float gallopFrequency = 1.15f;
constexpr float gallopStance = 0.30f;
constexpr float gallopOffsets[] = {0.0f, 0.30f, 0.10f, 0.42f};

// Foot coordinates relative to the unpitched shoulder/hip and ground.
// Contact is linear; the return arc reaches behind, folds, then reaches ahead.
inline Point gallop_hoof(bool front, float phase)
{
    phase -= std::floor(phase);
    const float start = front ? 6.3f : 7.5f;
    if (phase < gallopStance)
        return {start - gallopStride * phase, -0.65f};
    const float u = (phase - gallopStance) / (1 - gallopStance);
    constexpr float times[] = {0, 0.20f, 0.45f, 0.75f, 0.92f, 1};
    const Point fore[] = {{-6.3f, -0.65f}, {-7, -5}, {-1, -8.5f}, {10.5f, -5}, {8, -1.5f}, {6.3f, -0.65f}};
    const Point hind[] = {{-5.1f, -0.65f}, {-11, -4}, {-4, -8}, {6, -5.5f}, {8.5f, -2}, {7.5f, -0.65f}};
    const Point* keys = front ? fore : hind;
    int j = 0;
    while (j < 4 && u > times[j + 1]) ++j;
    auto tangent = [&](int n) -> Point {
        if (n == 0 || n == 5)
            return {-gallopStride * (1 - gallopStance), 0};
        const float span = times[n + 1] - times[n - 1];
        return {(keys[n + 1].x - keys[n - 1].x) / span, (keys[n + 1].y - keys[n - 1].y) / span};
    };
    const float span = times[j + 1] - times[j];
    const float v = (u - times[j]) / span, v2 = v * v, v3 = v2 * v;
    const Point a = keys[j], b = keys[j + 1], m = tangent(j), n = tangent(j + 1);
    auto hermite = [&](float av, float bv, float mv, float nv) {
        return (2 * v3 - 3 * v2 + 1) * av + (v3 - 2 * v2 + v) * span * mv + (-2 * v3 + 3 * v2) * bv + (v3 - v2) * span * nv;
    };
    return {hermite(a.x, b.x, m.x, n.x), std::min(-0.65f, hermite(a.y, b.y, m.y, n.y))};
}

constexpr float walkStride = 18.0f;
constexpr float walkFrequency = 0.55f;
constexpr float walkStance = 0.65f;
// Near hind -> near fore -> far hind -> far fore: four lateral footfalls.
constexpr float walkOffsets[] = {0.5f, 0.75f, 0.0f, 0.25f};
inline Point walk_hoof(float phase)
{
    phase -= std::floor(phase);
    const float reach = walkStride * walkStance * 0.5f;
    if (phase < walkStance)
        return {reach - walkStride * phase, -0.65f};
    const float u = (phase - walkStance) / (1 - walkStance);
    // Hermite endpoint velocities match stance: lift-off and landing retract
    // smoothly rather than stopping in mid-air before every contact.
    const float ease = u * u * (3 - 2 * u);
    const float tangent = -walkStride * (1 - walkStance);
    const float dx = -reach + 2 * reach * ease + tangent * (2 * u * u * u - 3 * u * u + u);
    return {dx, -0.65f - 1.8f * std::pow(std::sin(kPi * u), 2.0f)};
}

// (x,y) is the ground beneath the body. The torso pitches as one assembly;
// stance hooves remain in world space, rather than rotating with the body.
inline void horse(Canvas& cv, float x, float y, float s, float flip, float phase, Gait gait, Rgb col, Rgb mane)
{
    phase -= std::floor(phase);
    const float tau = 2 * kPi;
    const bool gallop = gait == Gait::Gallop, fly = gait == Gait::Fly, walk = gait == Gait::Walk;
    const float bob = gallop               ? -0.4f - 1.4f * std::cos(tau * (phase - 0.86f))
                      : fly                ? 0
                      : gait == Gait::Walk ? 0.22f * std::cos(phase * tau * 2)
                                           : 0;
    const float bodyY = -16 + bob;
    const float pitch = gallop ? 0.085f * std::cos(tau * (phase - 0.42f)) : fly ? -0.05f : walk ? 0.018f * std::sin(phase * tau * 2) : 0;
    const float ca = std::cos(pitch), sa = std::sin(pitch);
    auto torso = [&](float dx, float dy) -> Point { return {dx * ca - dy * sa, bodyY + dx * sa + dy * ca}; };
    auto screen = [&](Point p) -> Point { return {x + p.x * s * flip, y + p.y * s}; };
    auto line = [&](Point a, float ar, Point b, float br, Rgb color) {
        a = screen(a);
        b = screen(b);
        cv.capsule(a.x, a.y, ar * s, b.x, b.y, br * s, color);
    };
    auto oval = [&](Point c, float rx, float ry, Rgb color) {
        c = screen(c);
        cv.ellipse(c.x, c.y, rx * s, ry * s, pitch * flip, color);
    };
    const Rgb dim = gallop ? mix(col, {0.50f, 0.20f, 0.26f}, 0.16f) : mix(col, {0, 0, 0}, 0.25f);
    for (int i = 0; i < 4; ++i) {
        const bool front = i % 2 == 1;
        const float anchorX = front ? 7.0f : -8.0f;
        const Point hip = torso(anchorX, front ? 1.5f : 0.5f);
        Point foot{anchorX, -0.65f};
        if (gallop) {
            foot = gallop_hoof(front, phase - gallopOffsets[i]);
            foot.x += anchorX;
        }
        else if (fly) {
            const float drift = 0.6f * std::sin(phase * tau + (i < 2 ? 0.4f : 0));
            foot = torso(anchorX + (front ? (i < 2 ? -1.5f : 1.0f) : (i < 2 ? -11.0f : -9.0f)),
                         (front ? (i < 2 ? 7.0f : 8.5f) : (i < 2 ? 9.0f : 11.0f)) + drift);
        }
        else if (walk) {
            foot = walk_hoof(phase - walkOffsets[i]);
            foot.x += anchorX;
        }
        else
            foot.x += front ? 1.5f : -1.0f;
        // Walking needs a visible elbow/stifle before the carpus/hock.
        // A two-link leg directly from the rump makes the thigh bend backward.
        const Point joint = walk ? torso(anchorX + (front ? -1.0f : 3.0f), front ? 5.0f : 5.5f) : hip;
        const float dx = foot.x - joint.x, dy = foot.y - joint.y;
        const float upper = gallop || fly ? (front ? 8.0f : 8.5f) : walk ? 6.5f : 7.0f;
        const float lower = gallop || fly ? (front ? 9.0f : 9.5f) : walk ? (front ? 6.3f : 7.0f) : 8.0f;
        const float distance = std::max(0.001f, std::sqrt(dx * dx + dy * dy));
        const float d = std::min(distance, upper + lower - 0.001f);
        const float along = (upper * upper - lower * lower + d * d) / (2 * d);
        const float bend = std::sqrt(std::max(0.0f, upper * upper - along * along));
        const float dir = front ? 1.0f : -1.0f;
        const Point knee{joint.x + dx / distance * along + dir * dy / distance * bend,
                         joint.y + dy / distance * along - dir * dx / distance * bend};
        const Rgb lc = i < 2 ? dim : col;
        if (walk)
            line(hip, front ? 1.5f : 2.1f, joint, front ? 1.0f : 1.5f, lc);
        line(joint, walk ? (front ? 1.0f : 1.5f) : (front ? 1.5f : 2.1f), knee, front ? 0.75f : 1.0f, lc);
        line(knee, front ? 0.75f : 1.0f, foot, 0.55f, lc);
        line({foot.x - 0.45f, foot.y}, 0.65f, {foot.x + 0.75f, foot.y}, 0.65f, lc);
    }
    const float flow = gallop || fly ? 1.0f : 0.4f;
    const float tw = std::sin(phase * tau * 2 - 0.8f) * 1.5f * flow;
    line(torso(-12.5f, -1.5f), 1.4f, torso(-17.5f, 0.5f + tw), 1.5f, mane);
    line(torso(-17.5f, 0.5f + tw), 1.5f, torso(-21.5f, (fly ? 2.0f : 5.0f) + tw * 0.5f), 0.7f, mane);
    oval(torso(0, 0), 11, 4.8f, col);
    oval(torso(-8, -0.4f), 5, 4.9f, col);
    oval(torso(8.5f, -0.2f), 4.6f, 5, col);
    const float headDrop = gallop ? 2.2f * std::cos(tau * (phase - 0.55f)) : walk ? 0.7f * std::cos(tau * (phase - 0.25f) * 2) : 0;
    const float nibble = 0.25f * std::sin(phase * tau);
    const Point neckBase = torso(10, -2);
    const Point poll =
        gait == Gait::Graze ? Point{16, -5.5f + nibble} : torso(gallop ? 17 + headDrop : 15.5f, (gallop ? -7.0f : -8.5f) + headDrop);
    const Point muzzle =
        gait == Gait::Graze ? Point{17.5f, -1.8f + nibble} : torso(gallop ? 23.5f + headDrop : 21, (gallop ? -3.5f : -4.5f) + headDrop);
    line(neckBase, 3.6f, poll, 2.2f, col);
    line(poll, 2.1f, muzzle, 1.3f, col);
    line({poll.x - 0.5f, poll.y - 1.5f}, 0.7f, {poll.x - 0.2f, poll.y - 4}, 0.2f, col);
    const float mw = std::sin(phase * tau * 2 - 0.3f) * 0.9f * flow;
    line({neckBase.x - 1.2f, neckBase.y - 3}, 1.3f, {poll.x - 1.8f, poll.y - 1.2f + mw}, 1, mane);
    line({poll.x - 1.8f, poll.y - 1.2f + mw}, 1, {poll.x - 4.2f, poll.y + 1.2f + mw}, 0.5f, mane);
}
}  // namespace HorseScenes
