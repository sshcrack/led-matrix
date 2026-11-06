#include "GenreVistaScenes.h"

namespace Scenes {
using namespace AudioKit;

void AudioMoshPitScene::register_properties()
{
    add_property(sensitivity_);
    add_property(crowdSize_);
    add_property(strobe_on_);
}

void AudioMoshPitScene::init_world()
{
    crowd_.clear();
    const int n = crowdSize_->get();
    for (int i = 0; i < n; ++i)
        crowd_.push_back({hash(static_cast<uint32_t>(i) * 7u + 1u) * Tau, 0.22f + 0.70f * std::sqrt(hash(static_cast<uint32_t>(i) * 13u + 3u)),
                          0.6f + hash(static_cast<uint32_t>(i) * 19u) * 0.8f, hash(static_cast<uint32_t>(i) * 31u) * Tau,
                          // Mostly black/red/denim band shirts, a few bright ones.
                          hash(static_cast<uint32_t>(i) * 41u) < 0.15f ? 40.0f + hash(static_cast<uint32_t>(i)) * 280.0f : 0.0f});
}

bool AudioMoshPitScene::render(rgb_matrix::FrameCanvas* canvas)
{
    AudioState::Snapshot audio;
    float dt = 0;
    canvas->Clear();
    if (!begin_frame(audio, dt))
        return false;
    const int W = matrix_width, H = matrix_height;
    if (crowd_.size() != static_cast<size_t>(crowdSize_->get()))
        init_world();
    spectrum_.update(audio, dt, 0.04f, 0.2f);

    const float kickF = feature(audio, AudioProtocol::Feature::Kick);
    if (events_.beatHit)
        beat_.hit(0.5f + 0.5f * kickF);
    if (events_.onsetHit && kickF > 0.35f)
        kick_.hit(kickF);
    if (events_.dropHit) {
        drop_.hit();
        wall_.hit();
        strobe_.hit();
        palette_.next();
    }
    if (events_.sectionHit)
        palette_.next();
    beat_.decay(dt, 0.18f);
    kick_.decay(dt, 0.13f);
    drop_.decay(dt, 1.6f);
    strobe_.decay(dt, 1.0f);
    wall_.decay(dt, 1.2f);
    palette_.update(dt, 1.2f);

    const float bpm = std::clamp(audio.feature(AudioProtocol::Feature::Bpm), 80.0f, 200.0f);
    const float energy = std::min(1.0f, spectrum_.level * 1.6f);
    spin_ += dt * (0.35f + 1.7f * energy) * (bpm / 120.0f);
    const float cx = W * 0.5f, cy = H * 0.5f, R = std::min(W, H) * 0.47f;
    const RGB accent = hsv(palette_.value, 0.85f, 1.0f);

    // --- floor with a worn circle where the pit forms ---------------------------------------------------
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const float d = len(x + 0.5f - cx, y + 0.5f - cy) / R;
            const float dust = 0.7f + 0.3f * hash(static_cast<uint32_t>(y * 977 + x));
            RGB c = RGB{10, 9, 12} * dust;
            if (d < 1.05f)
                c = RGB{22, 18, 20} * dust * (1.0f - 0.35f * d);
            buf_.at(x, y) = c;
        }
    // Kick shockwaves roll out from the middle of the pit.
    {
        const float r = (1.0f - kick_.v) * R;
        for (int a = 0; a < 90; ++a) {
            const float ang = a * Tau / 90.0f;
            buf_.splat(cx + std::cos(ang) * r - 0.5f, cy + std::sin(ang) * r - 0.5f, accent * (0.5f * kick_.v));
        }
    }
    // Four overhead spots sweep in circles over the crowd.
    for (int l = 0; l < 4; ++l) {
        const float a = spin_ * 0.5f + l * Tau / 4.0f;
        const float lvl = spectrum_.band(0.12f * l, 0.12f * l + 0.3f);
        glow(buf_, cx + std::cos(a) * R * 0.55f, cy + std::sin(a * 1.3f) * R * 0.55f, R * 0.5f,
             hsv(palette_.value + l * 70.0f, 0.7f, 1.0f) * (0.06f + 0.22f * lvl + 0.08f * beat_.v));
    }

    // --- the crowd ------------------------------------------------------------------------------------------
    // Wall of death: on a drop the crowd parts along the x axis, then slams back together as it decays.
    const float part = wall_.v > 0.5f ? (wall_.v - 0.5f) * 2.0f : 0.0f;
    const float slam = wall_.v <= 0.5f && wall_.v > 0.02f ? wall_.v * 2.0f : 0.0f;
    for (const auto& m : crowd_) {
        // Inner crowd circles faster; the middle stays open for the circle pit.
        const float orbit = spin_ * m.speed * (1.0f / (0.45f + m.rad));
        const float ang = m.ang + orbit;
        const float jitter = std::sin(time_ * 9.0f * m.speed + m.phase) * (0.012f + 0.03f * energy);
        float x = cx + std::cos(ang) * (m.rad + jitter) * R;
        float y = cy + std::sin(ang) * (m.rad + jitter) * R;
        const float side = x < cx ? -1.0f : 1.0f;
        if (std::fabs(y - cy) < R * 0.18f)
            x += side * (part * R * 0.35f - slam * R * 0.05f);
        // Bounce on the beat.
        const float hop = beat_.v * std::fabs(std::sin(m.phase + time_ * 6.0f)) * 1.4f;
        y -= hop;
        const RGB shirt = m.hue == 0.0f ? lerp(RGB{16, 16, 20}, RGB{90, 20, 20}, hash(static_cast<uint32_t>(m.phase * 100.0f)))
                                        : hsv(m.hue, 0.7f, 0.85f);
        // Flailing arms.
        const float armA = time_ * 11.0f * m.speed + m.phase;
        line(buf_, x, y, x + std::cos(armA) * 2.8f, y + std::sin(armA) * 2.8f, RGB{150, 120, 100} * 0.7f);
        line(buf_, x, y, x - std::cos(armA * 1.3f) * 2.8f, y - std::sin(armA * 1.3f) * 2.8f, RGB{150, 120, 100} * 0.7f);
        buf_.disc(x, y, 1.6f, shirt + accent * (0.06f * beat_.v), false);       // torso seen from above
        buf_.disc(x, y - 0.3f, 0.9f, RGB{190, 150, 125}, false);                // head
    }
    // Crowd surfer carried across on drops.
    if (drop_.v > 0.05f) {
        const float t = 1.0f - drop_.v;
        const float sx = W * (0.1f + 0.8f * t), sy = H * 0.5f + std::sin(t * 9.0f) * 3.0f;
        line(buf_, sx - 4.0f, sy, sx + 4.0f, sy, RGB{200, 180, 170});
        buf_.disc(sx + 4.5f, sy, 1.2f, RGB{200, 160, 140});
    }

    if (strobe_on_->get() && strobe_.v > 0.05f && std::fmod(time_ * 12.0f, 1.0f) < 0.35f)
        for (auto& p : buf_.px) p = p + RGB{255, 245, 235} * (0.40f * strobe_.v);
    if (kick_.v > 0.02f)
        for (auto& p : buf_.px) p = p * (1.0f + kick_.v * 0.10f);
    buf_.blit(canvas);
    return true;
}

SceneDescriptor AudioMoshPitScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "mosh-pit";
    d.tags = {"music", "rock", "metal", "punk", "crowd", "audio-reactive", "beat-driven", "drop-reactive", "music-showcase",
              "style:rock"};
    d.intensity = 0.85f;
    d.motion = 0.80f;
    d.music_affinity = 1.0f;
    d.performance_cost = 0.45f;
    return d;
}
}  // namespace Scenes
