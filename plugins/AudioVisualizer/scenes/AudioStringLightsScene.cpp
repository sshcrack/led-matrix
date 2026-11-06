#include "GenreVistaScenes.h"

namespace Scenes {
using namespace AudioKit;

void AudioStringLightsScene::register_properties()
{
    add_property(sensitivity_);
    add_property(rows_);
    add_property(fireflies_);
}

void AudioStringLightsScene::init_world()
{
    const int W = matrix_width, H = matrix_height;
    flies_.clear();
    for (int i = 0; i < 18; ++i)
        flies_.push_back({rnd() * W, H * (0.35f + 0.5f * rnd()), rnd() * Tau, 0.5f + rnd()});
    bulbLevel_.assign(64, 0.0f);
}

bool AudioStringLightsScene::render(rgb_matrix::FrameCanvas* canvas)
{
    AudioState::Snapshot audio;
    float dt = 0;
    canvas->Clear();
    if (!begin_frame(audio, dt))
        return false;
    const int W = matrix_width, H = matrix_height;
    if (flies_.empty())
        init_world();
    spectrum_.update(audio, dt, 0.05f, 0.3f);

    const float kickF = feature(audio, AudioProtocol::Feature::Kick);
    if (events_.beatHit) {
        beat_.hit(0.5f + 0.5f * kickF);
        sway_.hit(0.6f);
    }
    if (events_.dropHit) {
        drop_.hit();
        palette_.next();
    }
    if (events_.sectionHit)
        palette_.next();
    beat_.decay(dt, 0.25f);
    drop_.decay(dt, 2.0f);
    sway_.decay(dt, 0.8f);
    palette_.update(dt, 2.0f);

    // --- dusk garden: gradient, tree line, fence --------------------------------------------------------------------------
    vgradient(buf_, 0, H, RGB{8, 10, 28} + hsv(palette_.value, 0.6f, 1.0f) * 0.02f, RGB{26, 16, 28} + hsv(palette_.value, 0.6f, 1.0f) * 0.03f);
    for (int x = 0; x < W; ++x) {
        const float tree = H * 0.58f - (std::sin(x * 0.11f) * 5.0f + std::sin(x * 0.29f + 1.0f) * 3.0f + std::sin(x * 0.53f) * 1.5f);
        for (int y = static_cast<int>(tree); y < static_cast<int>(H * 0.80f); ++y) buf_.set(x, y, RGB{4, 7, 8});
    }
    const int fenceTop = static_cast<int>(H * 0.76f);
    for (int y = fenceTop; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const bool slat = x % 6 == 0;
            const bool rail = y == fenceTop + 6 || y == fenceTop + 18;
            const float shade = slat ? 0.55f : 1.0f;
            buf_.at(x, y) = (rail ? RGB{34, 22, 16} : RGB{20, 13, 11}) * shade * (1.0f - 0.35f * static_cast<float>(y - fenceTop) / (H - fenceTop));
        }
    // Pointed tops on the fence slats.
    for (int x = 0; x < W; x += 6) {
        buf_.set(x, fenceTop - 1, RGB{20, 13, 11});
        buf_.set(x, fenceTop - 2, RGB{14, 10, 9});
    }

    // --- garlands -------------------------------------------------------------------------------------------------------------
    const int rows = rows_->get();
    const float swing = 1.0f + 0.5f * sway_.v;
    size_t bulbIdx = 0;
    for (int r = 0; r < rows; ++r) {
        const float baseY = H * (0.08f + 0.13f * r);
        const int swags = 2 + (r % 2);          // alternate rows get a different rhythm
        const float sag = H * (0.07f + 0.012f * r) * swing;
        const float phase = r * 0.5f;
        // The cable (very dark), then bulbs along it.
        for (int x = 0; x < W; ++x) {
            const float t = std::fmod(static_cast<float>(x) / W * swags, 1.0f);
            const float y = baseY + sag * (1.0f - (2.0f * t - 1.0f) * (2.0f * t - 1.0f)) * 0.9f + std::sin(time_ * 0.8f + phase + x * 0.04f) * 0.5f * sway_.v;
            buf_.set(x, static_cast<int>(y), RGB{10, 9, 12});
        }
        const int bulbs = 12 + 2 * r;
        for (int b = 0; b < bulbs; ++b) {
            const float fx = (static_cast<float>(b) + 0.5f) / bulbs;
            const float x = fx * W;
            const float t = std::fmod(fx * swags, 1.0f);
            const float y = baseY + sag * (1.0f - (2.0f * t - 1.0f) * (2.0f * t - 1.0f)) * 0.9f + 2.5f;
            const float target = spectrum_.value[static_cast<size_t>((b * 3 + r * 5) % spectrum_.value.size())];
            auto& lvl = bulbLevel_[bulbIdx % bulbLevel_.size()];
            lvl += (target - lvl) * (1.0f - std::exp(-dt / 0.1f));
            ++bulbIdx;
            const float flicker = 0.9f + 0.1f * std::sin(time_ * 3.0f + b * 1.9f + r);
            // Mostly warm white, with the palette creeping in on drops.
            const RGB colour = lerp(RGB{255, 190, 110}, hsv(palette_.value + b * 17.0f, 0.8f, 1.0f), 0.35f + 0.5f * drop_.v);
            const float bright = (0.28f + 0.85f * lvl + 0.25f * beat_.v) * flicker;
            glow(buf_, x, y, 4.0f + 7.0f * lvl, colour * (0.30f * bright));
            buf_.disc(x, y, 1.5f, colour * bright);
            buf_.add(static_cast<int>(x), static_cast<int>(y) - 2, RGB{15, 12, 10});  // cap
        }
    }

    // --- fireflies ---------------------------------------------------------------------------------------------------------------------
    if (fireflies_->get())
        for (auto& f : flies_) {
            f.phase += dt * f.speed;
            f.x += std::cos(f.phase * 0.7f) * dt * 5.0f * f.speed;
            f.y += std::sin(f.phase * 1.1f) * dt * 3.5f;
            if (f.x < -3)
                f.x += W + 6;
            if (f.x > W + 3)
                f.x -= W + 6;
            f.y = std::clamp(f.y, H * 0.30f, H * 0.78f);
            const float blink = std::pow(std::max(0.0f, std::sin(f.phase * 1.6f)), 6.0f);
            glow(buf_, f.x, f.y, 3.0f, RGB{160, 255, 90} * (0.55f * blink));
            buf_.splat(f.x - 0.5f, f.y - 0.5f, RGB{210, 255, 140} * blink);
        }
    buf_.blit(canvas);
    return true;
}

SceneDescriptor AudioStringLightsScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "string-lights";
    d.tags = {"music", "garden", "evening", "calm", "cozy", "audio-reactive", "beat-driven", "music-showcase",
              "style:acoustic", "style:chill"};
    d.intensity = 0.32f;
    d.motion = 0.28f;
    d.music_affinity = 0.9f;
    d.performance_cost = 0.40f;
    return d;
}
}  // namespace Scenes
