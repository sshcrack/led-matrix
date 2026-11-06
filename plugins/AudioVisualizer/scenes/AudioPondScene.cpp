#include "AmbientMusicScenes.h"

namespace Scenes {
using namespace AudioKit;

void AudioPondScene::register_properties()
{
    add_property(sensitivity_);
    add_property(rain_);
    add_property(damping_);
}

void AudioPondScene::build_sky()
{
    // The reflected night sky: deep gradient, a moon with halo and stars.
    const int W = matrix_width, H = matrix_height;
    sky_.assign(static_cast<size_t>(W * H), {});
    const float mx = W * 0.68f, my = H * 0.30f, mr = 9.0f;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const float t = static_cast<float>(y) / H;
            RGB c = lerp(RGB{8, 14, 38}, RGB{2, 18, 30}, t);
            const float d = AudioKit::len(x + 0.5f - mx, y + 0.5f - my);
            c = c + RGB{90, 110, 150} * (0.55f * std::exp(-d / 16.0f));
            if (d < mr + 1) {
                const float shade = 0.82f + 0.18f * std::clamp(((mx - x) + (my - y)) / (mr * 1.4f), -1.0f, 1.0f);
                c = lerp(c, RGB{240, 236, 210} * shade, std::clamp(mr - d + 0.5f, 0.0f, 1.0f));
            }
            const uint32_t id = static_cast<uint32_t>(y * W + x);
            if (hash(id * 3 + 11) > 0.992f && d > mr + 4)
                c = c + RGB{120, 130, 160} * hash(id);
            sky_[static_cast<size_t>(y * W + x)] = c;
        }
}

void AudioPondScene::drop(float cx, float cy, float radius, float strength)
{
    const int W = matrix_width, H = matrix_height;
    const int r = static_cast<int>(std::ceil(radius));
    for (int y = static_cast<int>(cy) - r; y <= static_cast<int>(cy) + r; ++y)
        for (int x = static_cast<int>(cx) - r; x <= static_cast<int>(cx) + r; ++x) {
            if (x < 1 || y < 1 || x >= W - 1 || y >= H - 1)
                continue;
            const float d = AudioKit::len(x - cx, y - cy) / std::max(0.5f, radius);
            if (d >= 1.0f)
                continue;
            h0_[static_cast<size_t>(y * W + x)] -= strength * (0.5f + 0.5f * std::cos(d * Pi));
        }
}

bool AudioPondScene::render(rgb_matrix::FrameCanvas* canvas)
{
    AudioState::Snapshot audio;
    float dt = 0;
    canvas->Clear();
    if (!begin_frame(audio, dt))
        return false;
    const int W = matrix_width, H = matrix_height;
    if (h0_.empty()) {
        h0_.assign(static_cast<size_t>(W * H), 0.0f);
        h1_ = h0_;
        build_sky();
        pads_ = {{W * 0.22f, H * 0.70f, 9.0f, 0.0f}, {W * 0.33f, H * 0.80f, 6.0f, 1.7f}, {W * 0.84f, H * 0.82f, 8.0f, 3.1f},
                 {W * 0.12f, H * 0.42f, 5.0f, 4.4f}};
        for (int i = 0; i < 7; ++i) flies_.push_back({rnd() * W, rnd() * H * 0.6f, 0, 0, rnd() * Tau});
    }
    spectrum_.update(audio, dt);

    const float kick = feature(audio, AudioProtocol::Feature::Kick);
    const float onset = feature(audio, AudioProtocol::Feature::OnsetStrength);
    const float hihat = feature(audio, AudioProtocol::Feature::Hihat);
    const float rain = rain_->get();
    if (events_.beatHit) {
        beat_.hit(0.5f + 0.5f * kick);
        // Heavy drop: bigger ripples on the beat, biased towards the moon path.
        drop(W * (0.45f + rnd() * 0.45f), H * (0.15f + rnd() * 0.7f), 2.5f + kick * 2.0f, (1.6f + kick * 2.2f) * rain);
    }
    if (events_.onsetHit)
        drop(rnd() * W, rnd() * H, 1.4f, (0.6f + onset * 1.2f) * rain);
    if (events_.dropHit)
        for (int i = 0; i < 14; ++i) {
            const float a = i / 14.0f * Tau;
            drop(W * 0.5f + std::cos(a) * 34.0f, H * 0.5f + std::sin(a) * 34.0f, 2.0f, 2.0f * rain);
        }
    beat_.decay(dt, 0.25f);
    drizzle_ += dt * rain * (0.4f + hihat * 5.0f + spectrum_.band(0.6f, 1.0f) * 2.0f);
    while (drizzle_ >= 1.0f) {
        drizzle_ -= 1.0f;
        drop(rnd() * W, rnd() * H, 1.2f, 0.35f + rnd() * 0.3f);
    }

    // Two wave-equation steps per frame (ripple speed ~60 px/s).
    const float damping = damping_->get();
    for (int step = 0; step < 2; ++step) {
        for (int y = 1; y < H - 1; ++y)
            for (int x = 1; x < W - 1; ++x) {
                const size_t i = static_cast<size_t>(y * W + x);
                const float avg = (h0_[i - 1] + h0_[i + 1] + h0_[i - W] + h0_[i + W]) * 0.25f;
                float n = avg * 2.0f - h1_[i];
                n += (avg - n) * 0.12f;  // viscosity: kills grid-scale noise
                h1_[i] = n * damping;
            }
        // Lily pads float and calm the water under them.
        for (const auto& p : pads_) {
            for (int y = static_cast<int>(p.y - p.r); y <= static_cast<int>(p.y + p.r); ++y)
                for (int x = static_cast<int>(p.x - p.r); x <= static_cast<int>(p.x + p.r); ++x)
                    if (x > 0 && y > 0 && x < W - 1 && y < H - 1 && AudioKit::len(x - p.x, y - p.y) < p.r)
                        h1_[static_cast<size_t>(y * W + x)] *= 0.6f;
        }
        std::swap(h0_, h1_);
    }

    // Shade: refract the sky by the surface gradient, add crest highlights.
    const float tint = spectrum_.band(0.0f, 0.3f);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const size_t i = static_cast<size_t>(y * W + x);
            const float gx = h0_[static_cast<size_t>(y * W + std::min(x + 1, W - 1))] - h0_[static_cast<size_t>(y * W + std::max(x - 1, 0))];
            const float gy = h0_[static_cast<size_t>(std::min(y + 1, H - 1) * W + x)] - h0_[static_cast<size_t>(std::max(y - 1, 0) * W + x)];
            const int sx = std::clamp(static_cast<int>(x + gx * 14.0f), 0, W - 1);
            const int sy = std::clamp(static_cast<int>(y + gy * 14.0f), 0, H - 1);
            RGB c = sky_[static_cast<size_t>(sy * W + sx)];
            const float slope = (-gx - gy) * 2.2f;
            const float spec = std::clamp(slope, 0.0f, 1.0f);
            c = c * (1.0f - 0.45f * std::clamp(-slope, 0.0f, 1.0f)) + RGB{130, 165, 210} * spec;
            c = c + RGB{10, 40, 60} * (tint * 0.6f + beat_.v * 0.25f);
            buf_.px[i] = c;
        }

    // Lily pads with a notch and a pink flower that opens with the bass.
    for (const auto& p : pads_) {
        const float bob = std::sin(time_ * 0.8f + p.phase) * 0.4f;
        for (int y = static_cast<int>(p.y - p.r - 1); y <= static_cast<int>(p.y + p.r + 1); ++y)
            for (int x = static_cast<int>(p.x - p.r - 1); x <= static_cast<int>(p.x + p.r + 1); ++x) {
                const float dx = x + 0.5f - p.x, dy = y + 0.5f - (p.y + bob);
                const float d = AudioKit::len(dx, dy);
                float ang = std::atan2(dy, dx) - p.phase;
                ang = std::remainder(ang, Tau);
                if (std::fabs(ang) < 0.32f)
                    continue;
                const float a = std::clamp(p.r - d + 0.5f, 0.0f, 1.0f);
                const float vein = 0.85f + 0.15f * std::cos(ang * 9.0f);
                buf_.mix(x, y, RGB{18, 70, 30} * vein * (0.8f + 0.2f * d / p.r), a);
            }
        if (p.r > 7)
            buf_.disc(p.x + 1, p.y + bob, 1.6f + tint * 1.4f, RGB{200, 80, 140} * (0.6f + beat_.v * 0.5f), false);
    }

    // Reeds framing the bottom corners, swaying with the mids.
    const float mid = spectrum_.band(0.25f, 0.6f);
    for (int r = 0; r < 9; ++r) {
        const bool left = r < 5;
        const float bx = left ? 2.0f + r * 4.5f : W - 3.0f - (r - 5) * 5.0f;
        const float len = 26.0f + hash(r * 17) * 26.0f;
        const float sway = std::sin(time_ * 1.3f + r) * (1.5f + mid * 5.0f);
        for (int s = 0; s < static_cast<int>(len); ++s) {
            const float t = s / len;
            const float x = bx + sway * t * t * (left ? 1.0f : -1.0f);
            buf_.mix(static_cast<int>(x), H - 1 - s, RGB{4, 10, 6}, 0.95f);
            if (t > 0.8f && r % 2 == 0)  // cattail head
                buf_.mix(static_cast<int>(x) + 1, H - 1 - s, RGB{30, 16, 6}, 0.95f);
        }
    }

    // Fireflies drift above the water; they glow with the high end.
    const float treble = spectrum_.band(0.55f, 1.0f);
    for (auto& f : flies_) {
        f.vx += (std::sin(time_ * 0.7f + f.phase * 3.0f) * 6.0f - f.vx) * dt;
        f.vy += (std::cos(time_ * 0.5f + f.phase * 2.0f) * 4.0f - f.vy) * dt;
        f.x = std::fmod(f.x + f.vx * dt + W, static_cast<float>(W));
        f.y = std::clamp(f.y + f.vy * dt, 4.0f, H - 10.0f);
        const float glow = std::clamp(0.25f + treble * 1.1f + 0.3f * std::sin(time_ * 3.0f + f.phase * 5.0f), 0.0f, 1.2f);
        buf_.splat(f.x, f.y, RGB{220, 240, 90} * glow);
        buf_.disc(f.x, f.y, 2.0f, RGB{40, 60, 10} * glow);
    }

    buf_.blit(canvas);
    return true;
}

SceneDescriptor AudioPondScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "pond";
    d.tags = {"music", "water", "nature", "night", "audio-reactive", "calm", "music-showcase", "style:chill", "style:acoustic", "drop-reactive"};
    d.intensity = 0.35f;
    d.motion = 0.45f;
    d.music_affinity = 0.9f;
    d.performance_cost = 0.35f;
    return d;
}
}  // namespace Scenes
