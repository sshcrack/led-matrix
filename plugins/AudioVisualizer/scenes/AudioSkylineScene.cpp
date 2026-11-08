#include "AmbientMusicScenes.h"

namespace Scenes {
using namespace AudioKit;

void AudioSkylineScene::register_properties()
{
    add_property(sensitivity_);
    add_property(fireworks_);
    add_property(reflection_);
}

void AudioSkylineScene::init_city()
{
    const int W = matrix_width;
    back_.clear();
    front_.clear();
    for (int x = -2, i = 0; x < W; ++i) {
        const int w = 6 + static_cast<int>(hash(i * 31 + 7) * 9);
        const int h = 18 + static_cast<int>(hash(i * 17 + 3) * 34);
        back_.push_back({x, w, h, h, 0, static_cast<int>(hash(i * 5) * 3), 0, static_cast<uint32_t>(i * 977 + 13)});
        x += w;
    }
    constexpr int Towers = 16;
    int x = 0;
    for (int i = 0; i < Towers; ++i) {
        const int w = (W - (Towers - 1)) / Towers + ((i % 3 == 1) ? 1 : 0);
        // Lows sit in the middle of the skyline, highs at the edges.
        const float centred = std::fabs(i - (Towers - 1) * 0.5f) / (Towers * 0.5f);
        const int band = std::clamp(static_cast<int>(centred * 26.0f), 0, 31);
        const int minH = 10 + static_cast<int>(hash(i * 13) * 8);
        const int maxH = 46 + static_cast<int>((1.0f - centred) * 34.0f + hash(i * 19) * 6);
        front_.push_back({x, w, minH, maxH, band, static_cast<int>(hash(i * 23 + 1) * 3), 0, static_cast<uint32_t>(i * 7919 + 5)});
        x += w + 1;
    }
}

void AudioSkylineScene::firework(float x, float y, float hue, int count)
{
    for (int i = 0; i < count; ++i) {
        const float a = rnd() * Tau;
        const float s = 18.0f + rnd() * 26.0f;
        sparks_.push_back({x, y, std::cos(a) * s, std::sin(a) * s, 0, 1.2f + rnd() * 0.9f, hue + rnd() * 40.0f});
    }
}

bool AudioSkylineScene::render(rgb_matrix::FrameCanvas* canvas)
{
    AudioState::Snapshot audio;
    float dt = 0;
    canvas->Clear();
    if (!begin_frame(audio, dt))
        return false;
    const int W = matrix_width, H = matrix_height;
    if (front_.empty())
        init_city();
    spectrum_.update(audio, dt, 0.05f, 0.25f);

    const float kick = feature(audio, AudioProtocol::Feature::Kick);
    const float loud = feature(audio, AudioProtocol::Feature::LoudnessFast);
    if (events_.beatHit)
        beat_.hit(0.5f + 0.5f * kick);
    if (events_.dropHit) {
        drop_.hit();
        if (fireworks_->get())
            for (int i = 0; i < 3; ++i) firework(W * (0.2f + 0.3f * i), H * (0.14f + 0.14f * rnd()), rnd() * 360.0f, 110);
    }
    // Occasional single rockets on strong downbeats in loud passages.
    if (fireworks_->get() && events_.beatHit && audio.beat_counter % 8 == 0 && loud > 0.55f)
        firework(W * (0.15f + 0.7f * rnd()), H * (0.12f + 0.15f * rnd()), rnd() * 360.0f, 60);
    if (events_.sectionHit || events_.dropHit)
        palette_.next();
    palette_.update(dt, 2.0f);
    const float skyHue_ = palette_.value;
    beat_.decay(dt, 0.2f);
    drop_.decay(dt, 1.6f);

    const bool river = reflection_->get();
    const int groundY = river ? static_cast<int>(H * 0.76f) : H;

    // --- sky -------------------------------------------------------------
    const RGB top = hsv(skyHue_, 0.8f, 0.05f);
    const RGB low = hsv(skyHue_ + 60.0f, 0.65f, 0.26f + 0.08f * drop_.v);
    for (int y = 0; y < groundY; ++y) {
        const float t = static_cast<float>(y) / groundY;
        const RGB c = lerp(top, low, t * t);
        for (int x = 0; x < W; ++x) buf_.at(x, y) = c;
    }
    const float treble = spectrum_.band(0.65f, 1.0f);
    for (int i = 0; i < 60; ++i) {
        const int sx = static_cast<int>(hash(i * 7 + 2) * W), sy = static_cast<int>(hash(i * 11 + 9) * groundY * 0.6f);
        const float tw = 0.4f + 0.6f * std::fabs(std::sin(time_ * (0.5f + hash(i) * 1.5f) + i));
        buf_.add(sx, sy, RGB{140, 140, 170} * (tw * (0.4f + treble * 0.7f)));
    }
    buf_.disc(W * 0.17f, H * 0.14f, 6.0f, RGB{230, 225, 200}, false);
    buf_.disc(W * 0.17f + 2.5f, H * 0.14f - 1.5f, 5.0f, top, false);  // crescent
    buf_.disc(W * 0.17f, H * 0.14f, 13.0f, RGB{30, 30, 40} * 0.5f, true);

    // --- fireworks (behind the towers) -----------------------------------
    for (auto& s : sparks_) {
        s.life += dt;
        s.vy += 16.0f * dt;
        s.vx *= std::exp(-dt * 1.2f);
        s.vy *= std::exp(-dt * 1.2f);
        s.x += s.vx * dt;
        s.y += s.vy * dt;
        const float f = std::clamp(1.0f - s.life / s.maxLife, 0.0f, 1.0f);
        const RGB c = hsv(s.hue, 0.75f - 0.45f * f, 1.0f) * (1.6f * f * std::sqrt(f));
        buf_.splat(s.x, s.y, c);
        buf_.splat(s.x - s.vx * 0.05f, s.y - s.vy * 0.05f, c * 0.5f);
        buf_.splat(s.x - s.vx * 0.10f, s.y - s.vy * 0.10f, c * 0.25f);
    }
    std::erase_if(sparks_, [](const Spark& s) { return s.life >= s.maxLife; });

    // --- back towers: static silhouettes ----------------------------------
    for (const auto& t : back_) {
        const int topY = groundY - t.maxH;
        for (int y = topY; y < groundY; ++y)
            for (int x = t.x; x < t.x + t.w; ++x) {
                if (!buf_.inside(x, y))
                    continue;
                RGB c = lerp(RGB{14, 12, 30}, low * 0.35f, 0.4f);
                const bool win = ((x - t.x) % 3 == 1) && ((y - topY) % 4 == 2) && x < t.x + t.w - 1;
                if (win && hash(t.seed + static_cast<uint32_t>(x * 131 + y * 7)) > 0.78f)
                    c = RGB{70, 55, 30};
                buf_.at(x, y) = c;
            }
    }

    // --- front towers: an equaliser in disguise ----------------------------
    const float onsetFlicker = events_.onsetHit ? 1.0f : 0.0f;
    if (windowFlicker_.size() != front_.size())
        windowFlicker_.assign(front_.size(), 0.0f);
    for (size_t i = 0; i < front_.size(); ++i) {
        auto& t = front_[i];
        const float target = spectrum_.value[static_cast<size_t>(t.band)];
        t.level += (target - t.level) * (1.0f - std::exp(-dt / (target > t.level ? 0.04f : 0.30f)));
        windowFlicker_[i] = std::max(windowFlicker_[i] * std::exp(-dt / 0.15f), onsetFlicker * hash(static_cast<uint32_t>(i) + audio.onset_counter * 31));
        const int height = t.minH + static_cast<int>(t.level * (t.maxH - t.minH));
        const int topY = groundY - height;
        const float lit = 0.18f + t.level * 0.72f;
        const RGB warm = t.style == 0 ? RGB{255, 190, 90} : (t.style == 1 ? RGB{120, 220, 255} : RGB{255, 140, 200});
        for (int y = topY; y < groundY; ++y)
            for (int x = t.x; x < t.x + t.w; ++x) {
                if (!buf_.inside(x, y))
                    continue;
                const int lx = x - t.x, ly = groundY - y;  // windows anchored to the street
                RGB c = RGB{20, 18, 38};
                if (lx == 0)
                    c = c + low * 0.25f;  // rim light from the sky
                const bool win = (lx % 2 == 1) && (ly % 3 == 1) && lx < t.w - 1 && y > topY + 1;
                if (win) {
                    const float hv = hash(t.seed + static_cast<uint32_t>(lx * 97 + ly * 13));
                    // Floors near the top light up as the band gets louder.
                    const float floorT = 1.0f - static_cast<float>(y - topY) / std::max(1, height);
                    if (hv < lit + windowFlicker_[i] * 0.3f)
                        c = warm * (0.35f + 0.45f * hv + 0.35f * floorT * t.level);
                }
                buf_.at(x, y) = c;
            }
        // Roof strip glows with the band; tall towers get a blinking beacon.
        for (int x = t.x; x < t.x + t.w; ++x) buf_.add(x, topY, warm * (0.25f + 0.75f * t.level));
        if (t.maxH > 70) {
            const int ax = t.x + t.w / 2;
            for (int y = topY - 4; y < topY; ++y) buf_.set(ax, y, RGB{40, 40, 50});
            buf_.add(ax, topY - 5, RGB{255, 30, 30} * (0.25f + beat_.v));
            buf_.disc(ax + 0.5f, topY - 4.5f, 2.0f, RGB{90, 0, 0} * beat_.v);
        }
    }

    // --- river reflection ------------------------------------------------
    if (river) {
        for (int y = groundY; y < H; ++y) {
            const int dy = y - groundY;
            for (int x = 0; x < W; ++x) {
                const float wob = std::sin(x * 0.35f + time_ * 2.2f + dy * 0.9f) * (0.6f + dy * 0.05f) * (1.0f + kick * 0.8f);
                const int sx = std::clamp(static_cast<int>(x + wob), 0, W - 1);
                const int sy = std::clamp(groundY - 1 - dy - static_cast<int>(std::fabs(wob) * 0.5f), 0, groundY - 1);
                const RGB src = buf_.at(sx, sy);
                const float fade = 0.55f - dy * 0.012f;
                buf_.at(x, y) = src * fade + RGB{2, 6, 16};
            }
        }
        for (int x = 0; x < W; ++x) buf_.at(x, groundY) = RGB{30, 30, 46} + low * 0.2f;
    }

    buf_.blit(canvas);
    return true;
}

SceneDescriptor AudioSkylineScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "skyline";
    d.tags = {"music", "city", "night", "fits:dusk", "fits:night", "spectrum", "audio-reactive", "music-showcase", "style:hiphop", "style:party", "style:edm", "drop-reactive"};
    d.intensity = 0.55f;
    d.motion = 0.5f;
    d.music_affinity = 1.0f;
    d.performance_cost = 0.3f;
    return d;
}
}  // namespace Scenes
