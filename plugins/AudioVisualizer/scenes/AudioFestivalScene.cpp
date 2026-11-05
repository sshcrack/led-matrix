#include "MusicVistaScenes.h"

namespace Scenes {
using namespace AudioKit;

void AudioFestivalScene::register_properties()
{
    add_property(sensitivity_);
    add_property(crowdSize_);
    add_property(confettiOn_);
}

void AudioFestivalScene::init_world()
{
    const int W = matrix_width;
    hills_.assign(static_cast<size_t>(W), 0.0f);
    for (int x = 0; x < W; ++x)
        hills_[static_cast<size_t>(x)] = std::sin(x * 0.045f + 1.3f) * 4.0f + std::sin(x * 0.11f) * 2.0f + std::sin(x * 0.023f) * 3.0f;
    crowd_.clear();
    for (int r = 0; r < 5; ++r)
        for (int i = 0; i < 26; ++i) {
            const float x = (i + 0.5f) / 26.0f * W * 1.1f - W * 0.05f + (hash(r * 71u + i) - 0.5f) * 5.0f;
            crowd_.push_back({x, static_cast<float>(r), hash(r * 13u + i * 7u) * Tau, hash(r * 3u + i * 29u), hash(r * 37u + i * 11u) * 360.0f,
                              hash(r * 53u + i * 17u) < 0.05f});
        }
    wallLevel_.assign(12, 0.0f);
    confetti_.clear();
}

bool AudioFestivalScene::render(rgb_matrix::FrameCanvas* canvas)
{
    AudioState::Snapshot audio;
    float dt = 0;
    canvas->Clear();
    if (!begin_frame(audio, dt))
        return false;
    const int W = matrix_width, H = matrix_height;
    if (static_cast<int>(hills_.size()) != W || crowd_.empty())
        init_world();
    spectrum_.update(audio, dt, 0.05f, 0.28f);

    const float kickF = feature(audio, AudioProtocol::Feature::Kick);
    const float energy = spectrum_.level;
    if (events_.beatHit)
        beat_.hit();
    if (events_.onsetHit && kickF > 0.35f)
        kick_.hit(0.5f + 0.5f * kickF);
    if (events_.dropHit) {
        drop_.hit();
        palette_.next();
        if (confettiOn_->get())
            for (int s = 0; s < 2; ++s)
                for (int i = 0; i < 70; ++i) {
                    const float ang = -Pi * 0.5f + (s == 0 ? -0.35f : 0.35f) + (rnd() - 0.5f) * 0.6f;
                    const float speed = 45.0f + rnd() * 35.0f;
                    confetti_.push_back({s == 0 ? W * 0.22f : W * 0.78f, H * 0.52f, std::cos(ang) * speed, std::sin(ang) * speed,
                                         rnd() * 360.0f, rnd() * Tau, 0.0f});
                }
    }
    if (events_.sectionHit)
        palette_.next();
    beat_.decay(dt, 0.22f);
    kick_.decay(dt, 0.18f);
    drop_.decay(dt, 2.0f);
    palette_.update(dt, 1.4f);
    const float bpm = std::clamp(audio.feature(AudioProtocol::Feature::Bpm), 70.0f, 180.0f);
    sway_ += dt * (bpm / 60.0f) * Pi * 0.5f;  // crowd sways once every two beats

    const int horizon = static_cast<int>(H * 0.50f);
    // --- sunset sky -----------------------------------------------------------------------------
    for (int y = 0; y < horizon + 8; ++y) {
        const float t = static_cast<float>(y) / horizon;
        RGB sky = t < 0.5f ? lerp(RGB{30, 22, 70}, RGB{150, 60, 110}, t / 0.5f) : lerp(RGB{150, 60, 110}, RGB{255, 140, 60}, (t - 0.5f) / 0.5f);
        for (int x = 0; x < W; ++x) buf_.at(x, std::min(y, H - 1)) = sky;
    }
    // Low sun behind the stage, glowing with the overall loudness.
    const float sunX = W * 0.30f, sunY = horizon - 4.0f;
    for (int y = 0; y < horizon + 4; ++y)
        for (int x = 0; x < W; ++x) {
            const float d = std::hypot(x + 0.5f - sunX, y + 0.5f - sunY);
            buf_.add(x, y, RGB{255, 170, 80} * ((0.25f + 0.25f * energy) * std::exp(-d / 14.0f)));
            if (d < 7.0f)
                buf_.at(x, y) = lerp(buf_.at(x, y), RGB{255, 220, 150}, smoothstep(7.0f, 5.5f, d));
        }
    // Thin clouds.
    for (int c = 0; c < 4; ++c) {
        const float cx = std::fmod(c * 41.0f + time_ * (1.0f + c * 0.3f), W + 40.0f) - 20.0f, cy = 12.0f + c * 7.0f;
        for (int x = static_cast<int>(cx - 16); x < static_cast<int>(cx + 16); ++x)
            buf_.mix(x, static_cast<int>(cy), RGB{255, 150, 130}, 0.25f * (1.0f - std::fabs(x - cx) / 16.0f));
    }

    // --- distant hills and trees --------------------------------------------------------------
    for (int x = 0; x < W; ++x)
        for (int y = static_cast<int>(horizon - 6 + hills_[static_cast<size_t>(x)]); y < H; ++y)
            if (y >= 0)
                buf_.at(x, y) = y < horizon + 4 ? RGB{26, 14, 34} : lerp(RGB{34, 18, 40}, RGB{14, 8, 18}, static_cast<float>(y - horizon) / (H - horizon));

    // --- stage: roof, LED wall (= spectrum), light towers ----------------------------------------
    const int stageL = static_cast<int>(W * 0.30f), stageR = static_cast<int>(W * 0.70f);
    const int roofY = static_cast<int>(H * 0.24f), deckY = horizon + 4;
    const RGB tint = hsv(palette_.value, 0.8f, 1.0f);
    for (int x = stageL - 3; x <= stageR + 3; ++x)  // roof
        for (int y = roofY - 3; y < roofY; ++y) buf_.set(x, y, RGB{12, 8, 16});
    for (int y = roofY; y < deckY; ++y) {  // pillars
        buf_.set(stageL - 2, y, RGB{12, 8, 16});
        buf_.set(stageR + 2, y, RGB{12, 8, 16});
    }
    const int bars = static_cast<int>(wallLevel_.size());
    const int wallT = roofY + 2, wallB = deckY - 7;
    for (int b = 0; b < bars; ++b) {
        auto& lv = wallLevel_[static_cast<size_t>(b)];
        const float target = spectrum_.band(static_cast<float>(b) / bars, static_cast<float>(b + 1) / bars);
        lv += (target - lv) * (1.0f - std::exp(-dt / 0.06f));
    }
    for (int y = wallT; y < wallB; ++y)
        for (int x = stageL; x <= stageR; ++x) {
            const float u = static_cast<float>(x - stageL) / (stageR - stageL + 1);
            const int b = std::clamp(static_cast<int>(u * bars), 0, bars - 1);
            // Classic EQ bars rising from the bottom of the LED wall, with gaps.
            const float v = static_cast<float>(wallB - 1 - y) / (wallB - wallT);
            const float lv = wallLevel_[static_cast<size_t>(b)];
            const float bu = u * bars - b;
            const bool gap = bu < 0.18f;
            const bool on = !gap && v < 0.08f + lv * 0.92f;
            const RGB c = hsv(palette_.value + u * 70.0f + v * 50.0f, 0.75f, 1.0f);
            buf_.at(x, y) = on ? c * (0.5f + 0.35f * lv + 0.15f * beat_.v) : RGB{8, 5, 12} + tint * (0.04f + 0.05f * beat_.v);
        }
    for (int y = deckY - 7; y < deckY; ++y)  // deck front
        for (int x = stageL - 4; x <= stageR + 4; ++x) buf_.set(x, y, RGB{10, 6, 12});
    // Two light towers whose cans fire beams into the sky on the beat.
    for (int t = 0; t < 2; ++t) {
        const float tx = t == 0 ? W * 0.12f : W * 0.88f;
        for (int y = static_cast<int>(H * 0.30f); y < deckY + 6; ++y) {
            buf_.set(static_cast<int>(tx), y, RGB{14, 10, 18});
            if (y % 4 == 0)
                buf_.set(static_cast<int>(tx) + 1, y, RGB{14, 10, 18});
        }
        const float level = spectrum_.band(t == 0 ? 0.0f : 0.5f, t == 0 ? 0.5f : 1.0f);
        const float ang = (t == 0 ? 0.35f : -0.35f) + std::sin(time_ * 0.6f + t * 2.0f) * 0.25f;
        const RGB beam = hsv(palette_.value + 180.0f * t, 0.5f, 1.0f) * (0.12f + 0.25f * level + 0.25f * beat_.v + 0.3f * drop_.v);
        for (int s = 0; s < 70; ++s) {
            const float px = tx + std::sin(ang) * s, py = H * 0.30f - std::cos(ang) * s;
            const float a = 1.0f - s / 70.0f;
            for (int o = -1; o <= 1; ++o) buf_.add(static_cast<int>(px) + o, static_cast<int>(py), beam * (a * (o == 0 ? 1.0f : 0.4f)));
        }
    }

    // --- crowd: rows of silhouettes from back to front, jumping on kicks --------------------------
    const int rows = std::clamp(static_cast<int>(std::round(5 * crowdSize_->get() / 1.6f + 1)), 2, 5);
    for (int r = 0; r < rows; ++r) {
        // Perspective: rows get bigger and lower towards the viewer, the
        // front row's heads sit just above the bottom edge.
        const float k = static_cast<float>(r) / std::max(1, rows - 1);
        const float scale = 0.55f + k * k * 1.35f;
        const float baseY = deckY + 8 + (H + 2 - deckY - 8) * (0.25f * k + 0.75f * k * k);
        // Stage wash: back rows catch the coloured light, front rows stay dark
        // but keep a rim on heads and shoulders so the crowd reads.
        const float depth = static_cast<float>(r) / (rows - 1 + 0.001f);
        const RGB shade = lerp(RGB{40, 22, 46} + tint * (0.14f + 0.10f * beat_.v), RGB{14, 8, 20}, depth);
        const RGB rim = lerp(hsv(palette_.value, 0.5f, 1.0f) * 0.55f, hsv(palette_.value + 30.0f, 0.6f, 1.0f) * 0.28f, depth) *
                        (0.8f + 0.4f * beat_.v + 0.4f * drop_.v);
        for (const auto& f : crowd_) {
            if (static_cast<int>(f.row) != r)
                continue;
            if (r == rows - 1 && static_cast<int>(f.phase * 10) % 2 == 1)
                continue;  // thinner front row so the stage stays visible
            const float bounce = (kick_.v * (0.6f + 0.4f * std::sin(f.phase)) + drop_.v * 0.7f) * 4.0f * scale;
            const float x = f.x + std::sin(sway_ + f.phase * 0.3f) * 1.2f * scale;
            const float headY = baseY - 9.0f * scale - bounce;
            buf_.disc(x, headY - 0.7f, 2.3f * scale, rim, false);
            buf_.disc(x, headY, 2.3f * scale, shade, false);
            for (int y = static_cast<int>(headY + 2 * scale); y < H; ++y) {
                const float k = std::min(1.0f, (y - headY - 2 * scale) / (4.0f * scale));
                const float half = (1.5f + 3.2f * std::sqrt(k)) * scale;
                for (int xx = static_cast<int>(x - half); xx <= static_cast<int>(x + half); ++xx) buf_.set(xx, y, shade);
            }
            // Raised hands at loud moments, a few waving flags.
            if (f.hand < 0.2f + energy * 0.6f + drop_.v * 0.5f) {
                const float side = f.hand < 0.5f ? -1.0f : 1.0f;
                for (int t = 0; t < static_cast<int>(9 * scale); ++t)
                    buf_.set(static_cast<int>(x + side * (2.5f * scale + t * 0.25f)), static_cast<int>(headY - t + 3), shade);
            }
            if (f.flag && r < rows - 1) {
                const float poleTop = headY - 16.0f * scale;
                for (int y = static_cast<int>(poleTop); y < static_cast<int>(headY); ++y) buf_.set(static_cast<int>(x + 2), y, shade);
                for (int fx = 0; fx < static_cast<int>(8 * scale); ++fx)
                    for (int fy = 0; fy < static_cast<int>(5 * scale); ++fy) {
                        const float wave = std::sin(fx * 0.8f - time_ * 6.0f) * 1.0f;
                        buf_.set(static_cast<int>(x + 3 + fx), static_cast<int>(poleTop + fy + wave), hsv(f.flagHue, 0.7f, 0.55f));
                    }
            }
        }
    }

    // --- confetti ---------------------------------------------------------------------------------
    for (auto& c : confetti_) {
        c.life += dt;
        c.vy += 35.0f * dt;
        c.vx *= std::exp(-dt * 1.5f);
        c.vy = std::min(c.vy, 9.0f);  // paper flutters down slowly
        c.x += (c.vx + std::sin(c.life * 4.0f + c.spin) * 6.0f) * dt;
        c.y += c.vy * dt;
    }
    std::erase_if(confetti_, [&](const Confetti& c) { return c.y > H + 2 || c.life > 9.0f; });
    for (const auto& c : confetti_) {
        const float flip = 0.4f + 0.6f * std::fabs(std::sin(c.life * 7.0f + c.spin));
        buf_.set(static_cast<int>(c.x), static_cast<int>(c.y), hsv(c.hue, 0.8f, flip));
    }

    buf_.blit(canvas);
    return true;
}

SceneDescriptor AudioFestivalScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "festival";
    d.tags = {"music", "festival", "concert", "sunset", "crowd", "spectrum", "audio-reactive", "beat-driven", "drop-reactive",
              "music-showcase", "style:rock", "style:party"};
    d.intensity = 0.62f;
    d.motion = 0.58f;
    d.music_affinity = 1.0f;
    d.performance_cost = 0.40f;
    return d;
}
}  // namespace Scenes
