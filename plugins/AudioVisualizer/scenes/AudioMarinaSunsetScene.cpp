#include "GenreVistaScenes.h"

namespace Scenes {
using namespace AudioKit;

void AudioMarinaSunsetScene::register_properties()
{
    add_property(sensitivity_);
    add_property(boat_);
    add_property(gullRate_);
}

bool AudioMarinaSunsetScene::render(rgb_matrix::FrameCanvas* canvas)
{
    AudioState::Snapshot audio;
    float dt = 0;
    canvas->Clear();
    if (!begin_frame(audio, dt))
        return false;
    const int W = matrix_width, H = matrix_height;
    spectrum_.update(audio, dt, 0.06f, 0.35f);
    if (shimmer_.size() != static_cast<size_t>(W))
        shimmer_.assign(static_cast<size_t>(W), 0.0f);

    const float kickF = feature(audio, AudioProtocol::Feature::Kick);
    const float hat = feature(audio, AudioProtocol::Feature::Hihat);
    const float bass = spectrum_.band(0.0f, 0.2f);
    if (events_.beatHit)
        beat_.hit(0.5f + 0.5f * kickF);
    if (events_.onsetHit && kickF > 0.35f)
        kick_.hit(kickF);
    if (events_.onsetHit && hat > 0.3f && rnd() < 0.18f * gullRate_->get() && gulls_.size() < 6)
        gulls_.push_back({W + 6.0f, H * (0.10f + 0.25f * rnd()), -(10.0f + 12.0f * rnd()), rnd() * Tau, 2.5f + 2.0f * rnd()});
    if (events_.dropHit) {
        drop_.hit();
        palette_.next();
    }
    if (events_.sectionHit)
        palette_.next();
    beat_.decay(dt, 0.3f);
    kick_.decay(dt, 0.2f);
    drop_.decay(dt, 2.5f);
    palette_.update(dt, 3.0f);
    bob_ += dt * (1.2f + bass * 2.0f);
    sunDrift_ += dt * 0.02f;
    for (auto& g : gulls_) {
        g.x += g.vx * dt;
        g.flap += dt * (6.0f + 4.0f * spectrum_.level);
        g.y += std::sin(g.flap * 0.25f) * dt * 2.0f;
    }
    std::erase_if(gulls_, [](const Gull& g) { return g.x < -8.0f; });
    // Water shimmer: every column listens to one bin, smoothed so it glitters instead of flickers.
    for (int x = 0; x < W; ++x) {
        auto& s = shimmer_[static_cast<size_t>(x)];
        const float target = spectrum_.value[static_cast<size_t>(std::abs(x - W / 2) * 2 * spectrum_.value.size() / W) % spectrum_.value.size()];
        s += (target - s) * (1.0f - std::exp(-dt / 0.12f));
    }

    const float horizon = H * 0.54f;
    const float sunX = W * 0.40f;
    const float sunR = H * (0.15f + 0.015f * bass + 0.02f * drop_.v);
    const float sunY = horizon - sunR * 0.25f + std::sin(sunDrift_) * 1.5f;
    const RGB warm = hsv(palette_.value, 0.75f, 1.0f);
    const RGB sunCol = lerp(RGB{255, 245, 190}, warm, 0.45f);

    // --- sky ---------------------------------------------------------------------------------------------------
    for (int y = 0; y < static_cast<int>(horizon); ++y) {
        const float t = static_cast<float>(y) / horizon;
        const RGB top = hsv(palette_.value + 200.0f, 0.5f, 0.35f) * 0.45f;
        buf_.at(0, y) = RGB{};
        for (int x = 0; x < W; ++x) buf_.at(x, y) = lerp(top, warm * 0.75f + RGB{30, 10, 20}, std::pow(t, 1.7f));
    }
    glow(buf_, sunX, sunY, H * 0.65f, warm * (0.30f + 0.12f * beat_.v + 0.15f * drop_.v));
    // Thin cloud bars catching the light.
    for (int c = 0; c < 4; ++c) {
        const float cy = horizon * (0.25f + 0.17f * c);
        const float cx = std::fmod(c * 47.0f + time_ * (1.0f + c * 0.6f), W + 60.0f) - 30.0f;
        for (int x = static_cast<int>(cx - 22); x < static_cast<int>(cx + 22); ++x)
            buf_.add(x, static_cast<int>(cy), warm * (0.35f * (1.0f - std::fabs(x - cx) / 22.0f)));
    }
    // Sun, sliced by the horizon with the lower slices dissolving into the haze.
    for (int y = static_cast<int>(sunY - sunR); y < static_cast<int>(horizon); ++y)
        for (int x = static_cast<int>(sunX - sunR); x <= static_cast<int>(sunX + sunR); ++x) {
            const float d = len(x + 0.5f - sunX, y + 0.5f - sunY);
            if (d > sunR || !buf_.inside(x, y))
                continue;
            const float slice = (y > sunY + sunR * 0.3f && (y % 3) == 0) ? 0.4f : 1.0f;
            buf_.at(x, y) = lerp(buf_.at(x, y), sunCol, smoothstep(sunR, sunR - 1.5f, d) * slice);
        }

    // --- water -------------------------------------------------------------------------------------------------------
    for (int y = static_cast<int>(horizon); y < H; ++y) {
        const float t = static_cast<float>(y - horizon) / (H - horizon);
        const RGB deep = hsv(palette_.value + 210.0f, 0.6f, 0.30f) * 0.5f;
        for (int x = 0; x < W; ++x) {
            // Reflected sun: a column that widens towards the viewer, broken by ripples that follow the music.
            const float spread = sunR * (0.35f + 1.3f * t);
            const float off = std::fabs(x + 0.5f - sunX) / spread;
            const float ripple = 0.5f + 0.5f * std::sin(y * (1.6f + 2.0f * t) - time_ * 2.2f + std::sin(x * 0.12f + time_) * 2.0f);
            const float sh = shimmer_[static_cast<size_t>(x)];
            const float refl = std::max(0.0f, 1.0f - off) * (0.15f + 0.85f * smoothstep(0.35f, 0.9f, ripple * (0.5f + sh * 1.1f))) * (1.0f - 0.45f * t);
            buf_.at(x, y) = lerp(warm * 0.5f, deep, std::min(1.0f, t * 1.6f)) * 0.55f + sunCol * (refl * (0.9f + 0.5f * kick_.v));
        }
    }
    // Soft haze at the horizon line.
    for (int x = 0; x < W; ++x) buf_.add(x, static_cast<int>(horizon), warm * 0.45f);

    // --- sailboat -------------------------------------------------------------------------------------------------------
    if (boat_->get()) {
        const float bx = W * 0.72f;
        const float by = horizon + H * 0.08f + std::sin(bob_) * (0.8f + 1.8f * bass);
        const float roll = std::sin(bob_ * 0.8f) * 0.12f;
        const RGB hull{18, 14, 22}, sail = warm * 0.9f + RGB{40, 30, 30};
        for (int i = -9; i <= 9; ++i) {  // hull
            const float hy = by + std::fabs(static_cast<float>(i)) * 0.18f + i * roll;
            for (int k = 0; k < 3; ++k) buf_.set(static_cast<int>(bx) + i, static_cast<int>(hy) + k, hull);
        }
        for (int y = 0; y < 22; ++y) {  // mast + main sail + jib
            buf_.set(static_cast<int>(bx + roll * (-y)), static_cast<int>(by) - y, hull);
            const float w = (22 - y) * 0.30f;
            for (int x = 1; x < static_cast<int>(w); ++x) buf_.set(static_cast<int>(bx + x + roll * (-y)), static_cast<int>(by) - y, sail * (0.75f + 0.25f * (y / 22.0f)));
            const float jw = (20 - y) * 0.24f;
            for (int x = 1; x < static_cast<int>(jw); ++x) buf_.set(static_cast<int>(bx - x + roll * (-y)), static_cast<int>(by) - y + 2, sail * 0.6f);
        }
        buf_.disc(bx, by - 23.0f, 1.0f, RGB{255, 90, 60} * (0.5f + 0.5f * beat_.v));  // masthead light
    }

    // --- palm silhouette in the corner ------------------------------------------------------------------------------------
    {
        const float tx = W * 0.07f;
        const float sway = std::sin(time_ * 0.9f) * (0.6f + 2.0f * spectrum_.level);
        for (int y = static_cast<int>(H * 0.30f); y < H; ++y) {
            const float t = static_cast<float>(y - H * 0.30f) / (H * 0.70f);
            const float x = tx + std::sin(t * 1.4f) * 5.0f + sway * (1.0f - t) * 0.6f;
            rect(buf_, static_cast<int>(x), y, static_cast<int>(x) + 3, y + 1, RGB{6, 5, 9});
        }
        const float hx = tx + std::sin(1.4f) * 5.0f + sway * 0.0f + 1.5f, hy = H * 0.30f;
        for (int f = 0; f < 7; ++f) {
            const float a = -Pi + f * Pi / 6.0f + std::sin(time_ * 1.1f + f) * 0.07f * (1.0f + spectrum_.level * 3.0f);
            float px = hx, py = hy;
            for (int s = 0; s < 14; ++s) {
                px += std::cos(a) * 1.4f;
                py += std::sin(a) * 1.2f + s * 0.11f;
                buf_.disc(px, py, 1.4f - s * 0.07f, RGB{5, 5, 8}, false);
            }
        }
    }

    // --- gulls ---------------------------------------------------------------------------------------------------------------------
    for (const auto& g : gulls_) {
        const float w = std::sin(g.flap) * g.size * 0.45f;
        line(buf_, g.x, g.y, g.x - g.size, g.y - w, RGB{20, 14, 24});
        line(buf_, g.x, g.y, g.x + g.size, g.y - w, RGB{20, 14, 24});
    }
    buf_.blit(canvas);
    return true;
}

SceneDescriptor AudioMarinaSunsetScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "marina";
    d.tags = {"music", "sunset", "sea", "fits:dawn", "fits:dusk", "calm", "scenery", "audio-reactive", "beat-driven", "music-showcase", "style:chill",
              "style:acoustic"};
    d.intensity = 0.30f;
    d.motion = 0.30f;
    d.music_affinity = 0.9f;
    d.performance_cost = 0.40f;
    return d;
}
}  // namespace Scenes
