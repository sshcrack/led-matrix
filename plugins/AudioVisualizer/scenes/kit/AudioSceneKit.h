#pragma once

// Small header-only toolkit shared by the "ambient music" scenes
// (ridgeline, pond, reef, skyline). Everything here is CPU-cheap: a float
// accumulation buffer, an auto-gain spectrum follower and event helpers.

#include <shared/matrix/audio_state.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "led-matrix.h"

namespace AudioKit {

constexpr float Pi = 3.14159265358979323846f;
constexpr float Tau = 2.0f * Pi;

struct RGB {
    float r = 0, g = 0, b = 0;
    RGB operator*(float s) const { return {r * s, g * s, b * s}; }
    RGB operator+(const RGB& o) const { return {r + o.r, g + o.g, b + o.b}; }
};

inline RGB lerp(const RGB& a, const RGB& b, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}

inline RGB hsv(float h, float s, float v)
{
    h = std::fmod(h, 360.0f);
    if (h < 0)
        h += 360.0f;
    const float c = v * s;
    const float x = c * (1.0f - std::fabs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    const float m = v - c;
    RGB o;
    if (h < 60)
        o = {c, x, 0};
    else if (h < 120)
        o = {x, c, 0};
    else if (h < 180)
        o = {0, c, x};
    else if (h < 240)
        o = {0, x, c};
    else if (h < 300)
        o = {x, 0, c};
    else
        o = {c, 0, x};
    return {(o.r + m) * 255.0f, (o.g + m) * 255.0f, (o.b + m) * 255.0f};
}

inline float hash(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return static_cast<float>(x & 0xFFFFFFU) / static_cast<float>(0xFFFFFF);
}

inline float smoothstep(float a, float b, float x)
{
    const float t = std::clamp((x - a) / (b - a), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// Float RGB accumulation buffer; blitted to the canvas once per frame.
class Buffer {
public:
    int w = 0, h = 0;
    std::vector<RGB> px;

    void resize(int width, int height)
    {
        w = width;
        h = height;
        px.assign(static_cast<size_t>(w * h), {});
    }
    void fill(const RGB& c) { std::fill(px.begin(), px.end(), c); }
    [[nodiscard]] bool inside(int x, int y) const { return x >= 0 && y >= 0 && x < w && y < h; }
    RGB& at(int x, int y) { return px[static_cast<size_t>(y * w + x)]; }
    void set(int x, int y, const RGB& c)
    {
        if (inside(x, y))
            at(x, y) = c;
    }
    void add(int x, int y, const RGB& c)
    {
        if (inside(x, y)) {
            auto& p = at(x, y);
            p.r += c.r;
            p.g += c.g;
            p.b += c.b;
        }
    }
    void mix(int x, int y, const RGB& c, float a)
    {
        if (inside(x, y))
            at(x, y) = lerp(at(x, y), c, a);
    }
    // Anti-aliased additive point (bilinear splat).
    void splat(float x, float y, const RGB& c)
    {
        const int x0 = static_cast<int>(std::floor(x));
        const int y0 = static_cast<int>(std::floor(y));
        const float fx = x - x0, fy = y - y0;
        add(x0, y0, c * ((1 - fx) * (1 - fy)));
        add(x0 + 1, y0, c * (fx * (1 - fy)));
        add(x0, y0 + 1, c * ((1 - fx) * fy));
        add(x0 + 1, y0 + 1, c * (fx * fy));
    }
    void disc(float cx, float cy, float radius, const RGB& c, bool additive = true)
    {
        const int r = static_cast<int>(std::ceil(radius + 1));
        for (int y = static_cast<int>(cy) - r; y <= static_cast<int>(cy) + r; ++y)
            for (int x = static_cast<int>(cx) - r; x <= static_cast<int>(cx) + r; ++x) {
                const float d = std::hypot(x + 0.5f - cx, y + 0.5f - cy);
                const float a = std::clamp(radius + 0.5f - d, 0.0f, 1.0f);
                if (a <= 0)
                    continue;
                if (additive)
                    add(x, y, c * a);
                else
                    mix(x, y, c, a);
            }
    }
    void blit(rgb_matrix::FrameCanvas* canvas) const
    {
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                const auto& p = px[static_cast<size_t>(y * w + x)];
                canvas->SetPixel(x, y, static_cast<uint8_t>(std::clamp(p.r, 0.0f, 255.0f)),
                                 static_cast<uint8_t>(std::clamp(p.g, 0.0f, 255.0f)),
                                 static_cast<uint8_t>(std::clamp(p.b, 0.0f, 255.0f)));
            }
    }
};

// Exactly-once consumption of monotonic analysis counters.
inline bool consume(uint64_t current, uint64_t& seen)
{
    if (current < seen) {
        seen = current;
        return false;
    }
    const bool advanced = current > seen;
    seen = current;
    return advanced;
}

struct EventTracker {
    uint64_t beat = 0, onset = 0, drop = 0, section = 0;
    bool primed = false;
    bool beatHit = false, onsetHit = false, dropHit = false, sectionHit = false;

    void update(const AudioState::Snapshot& a)
    {
        if (!primed) {
            beat = a.beat_counter;
            onset = a.onset_counter;
            drop = a.drop_counter;
            section = a.section_counter;
            primed = true;
        }
        beatHit = consume(a.beat_counter, beat);
        onsetHit = consume(a.onset_counter, onset);
        dropHit = consume(a.drop_counter, drop);
        sectionHit = consume(a.section_counter, section);
    }
};

// Resamples the incoming spectrum to a fixed bin count and normalises it
// against a slowly decaying running peak, so quiet breakdowns still move and
// loud drops do not saturate every bin.
class SpectrumFollower {
public:
    std::vector<float> value;  // smoothed, 0..1
    std::vector<float> peak;   // falling peak markers, 0..1
    float level = 0.0f;        // overall normalised energy 0..1

    explicit SpectrumFollower(int bins = 32) { resize(bins); }
    void resize(int bins)
    {
        value.assign(static_cast<size_t>(bins), 0.0f);
        peak.assign(static_cast<size_t>(bins), 0.0f);
        raw_.assign(static_cast<size_t>(bins), 0.0f);
    }

    void update(const AudioState::Snapshot& a, float dt, float attack = 0.06f, float release = 0.22f)
    {
        const int n = static_cast<int>(value.size());
        const auto& s = a.spectrum;
        float frameMax = 0.0f;
        for (int i = 0; i < n; ++i) {
            float v = 0.0f;
            if (!s.empty()) {
                const float pos = (static_cast<float>(i) + 0.5f) / n * static_cast<float>(s.size()) - 0.5f;
                const int i0 = std::clamp(static_cast<int>(std::floor(pos)), 0, static_cast<int>(s.size()) - 1);
                const int i1 = std::min(i0 + 1, static_cast<int>(s.size()) - 1);
                const float f = std::clamp(pos - i0, 0.0f, 1.0f);
                v = s[static_cast<size_t>(i0)] * (1 - f) + s[static_cast<size_t>(i1)] * f;
            }
            raw_[static_cast<size_t>(i)] = v;
            frameMax = std::max(frameMax, v);
        }
        // Automatic gain: track a floor and ceiling of the whole spectrum.
        gainPeak_ = std::max(frameMax, gainPeak_ - dt * 0.08f);
        gainPeak_ = std::max(gainPeak_, 0.12f);
        float sum = 0.0f;
        for (int i = 0; i < n; ++i) {
            auto& v = value[static_cast<size_t>(i)];
            float target = std::clamp((raw_[static_cast<size_t>(i)] - gainFloor_) / (gainPeak_ - gainFloor_), 0.0f, 1.0f);
            target = std::pow(target, 1.35f);
            const float k = target > v ? 1.0f - std::exp(-dt / attack) : 1.0f - std::exp(-dt / release);
            v += (target - v) * k;
            auto& p = peak[static_cast<size_t>(i)];
            p = std::max(v, p - dt * 0.45f);
            sum += v;
        }
        level = sum / std::max(1, n);
    }

    [[nodiscard]] float band(float from, float to) const
    {
        const int n = static_cast<int>(value.size());
        const int a = std::clamp(static_cast<int>(from * n), 0, n - 1);
        const int b = std::clamp(static_cast<int>(to * n), a + 1, n);
        float s = 0.0f;
        for (int i = a; i < b; ++i) s += value[static_cast<size_t>(i)];
        return s / static_cast<float>(b - a);
    }

private:
    std::vector<float> raw_;
    float gainPeak_ = 0.5f;
    float gainFloor_ = 0.08f;
};

// Exponential envelope: kicked to 1 by events, decays with a time constant.
struct Envelope {
    float v = 0.0f;
    void hit(float amount = 1.0f) { v = std::max(v, amount); }
    void decay(float dt, float tau) { v *= std::exp(-dt / tau); }
};

// Steps through a curated list of hues (avoids muddy olive/yellow skies)
// and eases towards the current one along the shortest path.
struct HueCycle {
    std::vector<float> hues;
    size_t index = 0;
    float value;
    explicit HueCycle(std::vector<float> h) : hues(std::move(h)), value(hues.front()) {}
    void next() { index = (index + 1) % hues.size(); }
    void update(float dt, float tau)
    {
        const float delta = std::remainder(hues[index] - value, 360.0f);
        value += delta * (1.0f - std::exp(-dt / tau));
    }
};

inline float feature(const AudioState::Snapshot& a, AudioProtocol::Feature f) { return std::clamp(a.feature(f), 0.0f, 1.0f); }

}  // namespace AudioKit
