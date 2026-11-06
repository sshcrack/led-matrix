#include "GenreVistaScenes.h"

namespace Scenes {
using namespace AudioKit;

void AudioSkateparkScene::register_properties()
{
    add_property(sensitivity_);
    add_property(speed_);
    add_property(graffiti_);
}

bool AudioSkateparkScene::render(rgb_matrix::FrameCanvas* canvas)
{
    AudioState::Snapshot audio;
    float dt = 0;
    canvas->Clear();
    if (!begin_frame(audio, dt))
        return false;
    const int W = matrix_width, H = matrix_height;
    spectrum_.update(audio, dt, 0.04f, 0.2f);

    const float kick = feature(audio, AudioProtocol::Feature::Kick);
    if (events_.beatHit)
        beat_.hit(0.5f + 0.5f * kick);
    if (events_.onsetHit && kick > 0.35f)
        kick_.hit(kick);
    if (events_.dropHit) {
        drop_.hit();
        palette_.next();
    }
    if (events_.sectionHit)
        palette_.next();
    beat_.decay(dt, 0.2f);
    kick_.decay(dt, 0.14f);
    drop_.decay(dt, 1.6f);
    palette_.update(dt, 1.2f);

    const float bpm = std::clamp(audio.feature(AudioProtocol::Feature::Bpm), 70.0f, 190.0f);
    phase_ += dt * Tau * (bpm / 60.0f / 8.0f) * speed_->get();
    const float s = std::sin(phase_);
    const float lipness = smoothstep(0.72f, 0.98f, std::fabs(s));
    const float airTarget = lipness * (0.25f + 0.9f * std::min(spectrum_.level * 1.6f, 1.0f) + 0.35f * kick_.v);
    air_ += (airTarget - air_) * (1.0f - std::exp(-dt / 0.05f));
    // A beat near the lip throws a trick (flip one way, then the other).
    if (events_.beatHit && lipness > 0.2f) {
        airVel_ = Tau * 2.2f * trickDir_;
        trickDir_ = -trickDir_;
    }
    spin_ += airVel_ * dt;
    airVel_ *= std::exp(-dt / 0.3f);
    if (lipness < 0.05f)
        spin_ = std::remainder(spin_, Tau) * std::exp(-dt / 0.08f);

    const float floorY = H * 0.84f;
    const float R = H * 0.30f;
    auto surf = [&](float x) -> float {
        float dx;
        if (x < R)
            dx = x - R;
        else if (x > W - R)
            dx = x - (W - R);
        else
            return floorY;
        return floorY - R + std::sqrt(std::max(0.0f, R * R - dx * dx));
    };

    // --- night sky + back wall -----------------------------------------------------------
    const RGB tint = hsv(palette_.value, 0.7f, 1.0f);
    vgradient(buf_, 0, static_cast<int>(floorY), RGB{6, 6, 16} + tint * 0.05f, RGB{16, 12, 26} + tint * (0.05f + 0.06f * drop_.v));
    const int wallTop = static_cast<int>(H * 0.14f);
    for (int y = wallTop; y < static_cast<int>(floorY); ++y)
        for (int x = 0; x < W; ++x) {
            const bool seam = y % 10 == 0 || (x + ((y / 10) & 1) * 8) % 16 == 0;
            const float n = 0.85f + 0.3f * hash(static_cast<uint32_t>(y * W + x));
            buf_.at(x, y) = (seam ? RGB{9, 8, 14} : RGB{22, 19, 30}) * n;
        }

    // --- graffiti: one tag column per band, drips run down -----------------------------------
    if (graffiti_->get()) {
        const int cols = 16;
        const int cw = W / cols;
        for (int i = 0; i < cols; ++i) {
            const float v = spectrum_.value[static_cast<size_t>(i * 2 % spectrum_.value.size())];
            const float hgt = (0.06f + v * 0.5f) * H * 0.7f;
            const float hue = palette_.value + i * 23.0f + hash(static_cast<uint32_t>(i) * 5u) * 60.0f;
            const RGB col = hsv(hue, 0.85f, 1.0f);
            const int x0 = i * cw + 1, x1 = x0 + cw - 2;
            const int yBottom = static_cast<int>(floorY) - 2;
            for (int y = static_cast<int>(yBottom - hgt); y < yBottom; ++y) {
                // A chunky stencil look: every third row is a gap.
                if ((y + i) % 5 == 0)
                    continue;
                const float a = 0.25f + 0.55f * v + 0.2f * beat_.v;
                for (int x = x0; x < x1; ++x) buf_.mix(x, y, col, a);
            }
            // Drip.
            const int dx = x0 + static_cast<int>(hash(static_cast<uint32_t>(i) * 3u + 1u) * (cw - 3));
            for (int y = yBottom; y < yBottom + 2 + static_cast<int>(hash(static_cast<uint32_t>(i) * 9u) * 6.0f); ++y)
                buf_.mix(dx, y, col, 0.5f);
        }
    }

    // --- floodlights ----------------------------------------------------------------------------
    for (int l = 0; l < 2; ++l) {
        const float lx = l == 0 ? W * 0.30f : W * 0.70f;
        glow(buf_, lx, wallTop - 2.0f, H * 0.5f, RGB{255, 235, 210} * (0.10f + 0.10f * kick_.v + 0.08f * drop_.v));
        buf_.disc(lx, wallTop - 2.0f, 1.4f, RGB{255, 240, 220});
    }

    // --- the halfpipe ---------------------------------------------------------------------------
    for (int x = 0; x < W; ++x) {
        const float sy = surf(x + 0.5f);
        const float lit = 0.55f + 0.45f * std::cos((x + 0.5f - W * 0.5f) / (W * 0.5f) * 1.3f);
        for (int y = static_cast<int>(sy); y < H; ++y) {
            const float depth = static_cast<float>(y - sy) / std::max(1.0f, H - sy);
            buf_.set(x, y, lerp(RGB{58, 56, 72}, RGB{22, 20, 30}, depth * 1.6f) * (lit + 0.25f * kick_.v) + tint * (0.05f * beat_.v));
        }
        if (static_cast<int>(sy) >= 0 && static_cast<int>(sy) < H)
            buf_.set(x, static_cast<int>(sy), RGB{150, 150, 165});  // coping / top edge
    }
    for (int x = 0; x < W; x += 6)
        if (surf(static_cast<float>(x)) > floorY - 1.0f)
            buf_.set(x, static_cast<int>(floorY) + 1, RGB{18, 16, 24});

    // --- skater -------------------------------------------------------------------------------------
    {
        const float px = W * 0.5f + s * (W * 0.5f - R * 0.12f);
        const float slope = std::atan2(surf(px + 1.0f) - surf(px - 1.0f), 2.0f);
        const float tilt = slope * (1.0f - 0.5f * air_) + spin_;
        const float bx = px, by = surf(px) - 1.5f - air_ * H * 0.26f;
        const float dxv = std::cos(tilt), dyv = std::sin(tilt);
        const float nx = std::sin(tilt), ny = -std::cos(tilt);
        const RGB skin{235, 195, 165}, shirt = hsv(palette_.value + 180.0f, 0.7f, 1.0f) * 0.9f, pants{40, 50, 90};
        line(buf_, bx - dxv * 4.0f, by - dyv * 4.0f, bx + dxv * 4.0f, by + dyv * 4.0f, RGB{210, 210, 220});  // board
        buf_.disc(bx - dxv * 3.2f, by - dyv * 3.2f + 0.8f, 0.7f, RGB{180, 160, 60});                      // wheels
        buf_.disc(bx + dxv * 3.2f, by + dyv * 3.2f + 0.8f, 0.7f, RGB{180, 160, 60});
        const float hx = bx + nx * 4.5f, hy = by + ny * 4.5f;
        line(buf_, bx - dxv * 2.0f, by - dyv * 2.0f, hx, hy, pants);
        line(buf_, bx + dxv * 2.0f, by + dyv * 2.0f, hx, hy, pants);
        const float sx = hx + nx * 5.0f, sy2 = hy + ny * 5.0f;
        line(buf_, hx, hy, sx, sy2, shirt);
        line(buf_, hx + nx * 0.5f, hy + ny * 0.5f, sx, sy2, shirt);
        const float arm = 1.0f + air_ * 3.0f;
        line(buf_, sx, sy2, sx - dxv * 4.5f, sy2 - dyv * 4.5f + ny * arm, skin);
        line(buf_, sx, sy2, sx + dxv * 4.5f, sy2 + dyv * 4.5f + ny * arm, skin);
        buf_.disc(sx + nx * 2.4f, sy2 + ny * 2.4f, 1.7f, skin, false);
        buf_.disc(sx + nx * 3.0f, sy2 + ny * 3.0f, 1.8f, RGB{30, 30, 36}, false);  // cap
    }

    // Kick: the whole park thumps a little.
    if (kick_.v > 0.02f)
        for (auto& p : buf_.px) p = p * (1.0f + kick_.v * 0.14f);
    buf_.blit(canvas);
    return true;
}

SceneDescriptor AudioSkateparkScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "skatepark";
    d.tags = {"music", "skate", "punk", "night", "spectrum", "audio-reactive", "beat-driven", "drop-reactive",
              "music-showcase", "style:rock", "style:party"};
    d.intensity = 0.68f;
    d.motion = 0.66f;
    d.music_affinity = 1.0f;
    d.performance_cost = 0.40f;
    return d;
}
}  // namespace Scenes
