#include "GenreVistaScenes.h"

namespace Scenes {
using namespace AudioKit;

void AudioRollerRinkScene::register_properties()
{
    add_property(sensitivity_);
    add_property(skaterCount_);
    add_property(lapSpeed_);
}

void AudioRollerRinkScene::init_world()
{
    skaters_.clear();
    const int n = skaterCount_->get();
    for (int i = 0; i < n; ++i)
        skaters_.push_back({0.30f + 0.62f * hash(static_cast<uint32_t>(i) * 11u + 5u), hash(static_cast<uint32_t>(i) * 3u) * Tau,
                            (hash(static_cast<uint32_t>(i) * 17u + 2u) < 0.5f ? 1.0f : 1.0f) * (0.7f + 0.6f * hash(static_cast<uint32_t>(i) * 23u)),
                            hash(static_cast<uint32_t>(i) * 29u) * 360.0f});
    tile_.assign(8, 0.0f);
}

bool AudioRollerRinkScene::render(rgb_matrix::FrameCanvas* canvas)
{
    AudioState::Snapshot audio;
    float dt = 0;
    canvas->Clear();
    if (!begin_frame(audio, dt))
        return false;
    const int W = matrix_width, H = matrix_height;
    if (skaters_.size() != static_cast<size_t>(skaterCount_->get()) || tile_.empty())
        init_world();
    spectrum_.update(audio, dt, 0.04f, 0.2f);

    const float kickF = feature(audio, AudioProtocol::Feature::Kick);
    if (events_.beatHit) {
        beat_.hit(0.5f + 0.5f * kickF);
        ++beatCount_;
    }
    if (events_.onsetHit && kickF > 0.35f)
        kick_.hit(kickF);
    if (events_.dropHit) {
        drop_.hit();
        palette_.next();
    }
    if (events_.sectionHit)
        palette_.next();
    beat_.decay(dt, 0.2f);
    kick_.decay(dt, 0.14f);
    drop_.decay(dt, 1.8f);
    palette_.update(dt, 1.0f);
    const float bpm = std::clamp(audio.feature(AudioProtocol::Feature::Bpm), 70.0f, 190.0f);
    ballSpin_ += dt * (0.6f + spectrum_.level);
    for (size_t i = 0; i < tile_.size(); ++i) {
        const float target = spectrum_.band(static_cast<float>(i) / tile_.size(), static_cast<float>(i + 1) / tile_.size());
        tile_[i] += (target - tile_[i]) * (1.0f - std::exp(-dt / 0.09f));
    }

    const float cx = W * 0.5f, cy = H * 0.5f;
    const float rx = W * 0.46f, ry = H * 0.40f;
    const RGB accent = hsv(palette_.value, 0.85f, 1.0f);

    // --- carpet outside, polished floor inside: concentric rings are the spectrum -------------------------------
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const float nx = (x + 0.5f - cx) / rx, ny = (y + 0.5f - cy) / ry;
            const float d = len(nx, ny);  // 0 centre .. 1 rail
            if (d > 1.0f) {
                const float rail = d < 1.07f ? 1.0f : 0.0f;
                buf_.at(x, y) = rail > 0.0f ? RGB{110, 70, 50} : RGB{14, 8, 18} * (0.75f + 0.25f * hash(static_cast<uint32_t>(y * 131 + x)));
                continue;
            }
            const int ring = std::min(7, static_cast<int>(d * 8.0f));
            const float within = d * 8.0f - ring;
            const float edge = smoothstep(0.0f, 0.12f, within) * smoothstep(1.0f, 0.88f, within);
            const float lvl = tile_[static_cast<size_t>(ring)];
            const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(ring) * 0.9f - ballSpin_ * 2.0f);
            const RGB c = hsv(palette_.value + ring * 28.0f, 0.8f, 1.0f) * ((0.05f + 0.55f * lvl + 0.18f * beat_.v * pulse) * edge);
            buf_.at(x, y) = RGB{9, 6, 15} + c;
        }

    // --- roaming mirror-ball spots on the floor ---------------------------------------------------------------------
    for (int s = 0; s < 9; ++s) {
        const float a = ballSpin_ * (0.5f + 0.12f * s) + s * 0.7f;
        const float rr = 0.25f + 0.65f * std::fabs(std::sin(a * 0.6f + s));
        const float x = cx + std::cos(a) * rx * rr, y = cy + std::sin(a) * ry * rr;
        glow(buf_, x, y, 4.5f, RGB{255, 250, 240} * (0.16f + 0.20f * kick_.v));
    }

    // --- skaters: lane = ellipse scale, head-to-tail trails -----------------------------------------------------
    const float laps = bpm / 60.0f * 0.11f * lapSpeed_->get() * (1.0f + 0.9f * kick_.v + 0.8f * drop_.v);
    for (auto& s : skaters_) {
        s.phase += dt * Tau * laps * s.speed / s.lane;
        for (int k = 0; k < 7; ++k) {
            const float a = s.phase - k * 0.05f / s.lane;
            const float x = cx + std::cos(a) * rx * s.lane, y = cy + std::sin(a) * ry * s.lane;
            const RGB col = hsv(s.hue + palette_.value, 0.7f, 1.0f) * ((1.0f - k / 7.0f) * 0.95f);
            if (k == 0) {
                // Bob in time with the beat so the whole rink feels like it dances.
                const float bob = beat_.v * std::sin(s.hue + time_ * 8.0f) * 0.9f;
                buf_.disc(x, y + bob, 2.0f, col, false);
                buf_.disc(x, y + bob - 0.2f, 1.0f, RGB{235, 200, 175}, false);
            } else
                buf_.splat(x - 0.5f, y - 0.5f, col * 0.55f);
        }
    }
    // Rink-centre ball glow and floor-edge LED strip, flashing with drops.
    glow(buf_, cx, cy, rx * 0.22f, accent * (0.12f + 0.4f * drop_.v + 0.15f * kick_.v));
    for (int a = 0; a < 140; ++a) {
        const float ang = a * Tau / 140.0f;
        const bool on = (a + static_cast<int>(ballSpin_ * 8.0f)) % 4 < 2;
        buf_.add(static_cast<int>(cx + std::cos(ang) * rx * 1.035f), static_cast<int>(cy + std::sin(ang) * ry * 1.035f),
                 hsv(palette_.value + a * 2.5f, 0.8f, 1.0f) * (on ? 0.55f + 0.5f * beat_.v : 0.12f));
    }
    if (kick_.v > 0.02f)
        for (auto& p : buf_.px) p = p * (1.0f + kick_.v * 0.10f);
    buf_.blit(canvas);
    return true;
}

SceneDescriptor AudioRollerRinkScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "roller-rink";
    d.tags = {"music", "roller-disco", "dance", "party", "retro", "spectrum", "audio-reactive", "beat-driven", "drop-reactive",
              "music-showcase", "style:party", "style:edm"};
    d.intensity = 0.72f;
    d.motion = 0.70f;
    d.music_affinity = 1.0f;
    d.performance_cost = 0.40f;
    return d;
}
}  // namespace Scenes
