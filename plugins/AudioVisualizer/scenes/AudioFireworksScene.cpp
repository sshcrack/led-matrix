#include "GenreVistaScenes.h"

namespace Scenes {
using namespace AudioKit;

void AudioFireworksScene::register_properties()
{
    add_property(sensitivity_);
    add_property(density_);
    add_property(reflection_);
}

void AudioFireworksScene::init_world()
{
    const int W = matrix_width, H = matrix_height;
    skyline_.assign(static_cast<size_t>(W), 0.0f);
    windows_.assign(static_cast<size_t>(W * H), 0);
    const float shore = H * 0.80f;
    int x = 0;
    uint32_t seed = 1;
    while (x < W) {
        const int bw = 5 + static_cast<int>(hash(seed++) * 9.0f);
        const float bh = H * (0.08f + 0.22f * hash(seed++));
        for (int k = 0; k < bw && x + k < W; ++k) skyline_[static_cast<size_t>(x + k)] = shore - bh;
        for (int wy = static_cast<int>(shore - bh) + 2; wy < static_cast<int>(shore) - 2; wy += 3)
            for (int wx = x + 1; wx < x + bw - 1 && wx < W; wx += 2)
                if (hash(seed++) < 0.35f)
                    windows_[static_cast<size_t>(wy * W + wx)] = 1;
        x += bw;
    }
}

void AudioFireworksScene::launch(float x, int band, float power)
{
    const int H = matrix_height;
    Shell s;
    s.x = x;
    s.y = H * 0.80f;
    s.apex = H * (0.12f + 0.30f * rnd()) * (1.0f - 0.25f * power) + H * 0.05f;
    const float rise = s.y - s.apex;
    s.vy = -std::sqrt(2.0f * 55.0f * rise);  // reaches the apex under g = 55 px/s²
    s.vx = (rnd() - 0.5f) * 8.0f;
    s.hue = palette_.value + rnd() * 120.0f + band * 11.0f;
    s.band = band;
    shells_.push_back(s);
}

void AudioFireworksScene::burst(const Shell& s, float power)
{
    const int count = static_cast<int>((26 + 40 * power) * std::max(0.5f, render_quality_scale()));
    const int kind = static_cast<int>(rnd() * 3.0f);
    const float speed = 14.0f + 22.0f * power;
    for (int i = 0; i < count && sparks_.size() < 700; ++i) {
        const float ang = i * Tau / count + rnd() * 0.1f;
        float sp = speed * (0.85f + 0.3f * rnd());
        if (kind == 1)  // ring
            sp = speed;
        else if (kind == 2 && i % 2)  // double ring
            sp *= 0.55f;
        const float life = 1.0f + rnd() * 0.8f + power * 0.6f;
        sparks_.push_back({s.x, s.y, std::cos(ang) * sp + s.vx * 0.3f, std::sin(ang) * sp, life, life,
                           s.hue + (kind == 2 && i % 2 ? 60.0f : 0.0f) + (rnd() - 0.5f) * 20.0f, 0.55f + 0.4f * rnd()});
    }
    flash_.hit(0.3f + 0.4f * power);
}

bool AudioFireworksScene::render(rgb_matrix::FrameCanvas* canvas)
{
    AudioState::Snapshot audio;
    float dt = 0;
    canvas->Clear();
    if (!begin_frame(audio, dt))
        return false;
    const int W = matrix_width, H = matrix_height;
    if (skyline_.size() != static_cast<size_t>(W))
        init_world();
    spectrum_.update(audio, dt, 0.05f, 0.25f);

    const float kickF = feature(audio, AudioProtocol::Feature::Kick);
    const float density = density_->get();
    if (events_.beatHit) {
        beat_.hit();
        if (rnd() < 0.7f * density)
            launch(W * (0.12f + 0.76f * rnd()), static_cast<int>(rnd() * 8.0f), spectrum_.level);
    }
    if (events_.onsetHit && kickF > 0.4f) {
        kick_.hit(kickF);
        if (rnd() < 0.3f * density)
            launch(W * (0.12f + 0.76f * rnd()), 0, 0.5f + 0.5f * kickF);
    }
    if (events_.dropHit) {
        drop_.hit();
        finale_ = 2.2f;
        palette_.next();
    }
    if (events_.sectionHit)
        palette_.next();
    beat_.decay(dt, 0.2f);
    kick_.decay(dt, 0.15f);
    drop_.decay(dt, 2.0f);
    flash_.decay(dt, 0.25f);
    palette_.update(dt, 1.5f);
    // Finale: a quick barrage of shells.
    if (finale_ > 0.0f) {
        finale_ -= dt;
        if (rnd() < dt * 9.0f * density)
            launch(W * (0.08f + 0.84f * rnd()), static_cast<int>(rnd() * 8.0f), 0.9f);
    }

    // --- shells and sparks ----------------------------------------------------------------------------------
    for (auto& s : shells_) {
        s.vy += 55.0f * dt;
        s.y += s.vy * dt;
        s.x += s.vx * dt;
    }
    std::vector<Shell> dead;
    for (const auto& s : shells_)
        if (s.vy >= -2.0f)
            dead.push_back(s);
    std::erase_if(shells_, [](const Shell& s) { return s.vy >= -2.0f; });
    for (const auto& s : dead) burst(s, 0.4f + 0.6f * std::min(1.0f, spectrum_.band(0.0f, 0.5f) * 1.4f + kick_.v * 0.4f));
    for (auto& p : sparks_) {
        p.vy += 22.0f * dt;
        p.vx *= std::exp(-dt * 1.2f);
        p.vy *= std::exp(-dt * 0.8f);
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        p.life -= dt;
    }
    std::erase_if(sparks_, [&](const Spark& p) { return p.life <= 0.0f || p.y > H * 0.82f; });

    // --- sky, skyline, water -------------------------------------------------------------------------------------
    const float shore = H * 0.80f;
    const float glowSky = 0.05f + 0.10f * flash_.v;
    vgradient(buf_, 0, static_cast<int>(shore), RGB{3, 4, 12}, RGB{16, 14, 30} + hsv(palette_.value, 0.6f, 1.0f) * glowSky);
    for (int i = 0; i < 40; ++i) {  // stars
        const float tw = 0.5f + 0.5f * std::sin(time_ * 1.3f + i * 2.1f);
        buf_.add(static_cast<int>(hash(static_cast<uint32_t>(i) * 3u) * W), static_cast<int>(hash(static_cast<uint32_t>(i) * 7u + 1u) * shore * 0.7f),
                 RGB{200, 205, 255} * (0.25f * tw));
    }
    for (int y = static_cast<int>(shore); y < H; ++y) {
        const float t = static_cast<float>(y - shore) / (H - shore);
        for (int x = 0; x < W; ++x) buf_.at(x, y) = lerp(RGB{8, 10, 22}, RGB{3, 4, 10}, t);
    }

    // Sparks and trails (additive), plus a reflection in the water.
    auto draw_spark = [&](float x, float y, const RGB& c) {
        buf_.splat(x - 0.5f, y - 0.5f, c);
        if (reflection_->get()) {
            const float ry = shore + (shore - y) * 0.55f;
            if (ry < H)
                buf_.splat(x - 0.5f + std::sin(time_ * 4.0f + y) * 0.7f, ry - 0.5f, c * 0.28f);
        }
    };
    for (const auto& s : shells_) {
        draw_spark(s.x, s.y, RGB{255, 220, 150});
        for (int k = 1; k < 7; ++k) draw_spark(s.x - s.vx * 0.02f * k, s.y - s.vy * 0.016f * k, RGB{255, 170, 80} * (1.0f - k / 7.0f) * 0.7f);
    }
    for (const auto& p : sparks_) {
        const float a = std::clamp(p.life / p.maxLife, 0.0f, 1.0f);
        const RGB c = hsv(p.hue, p.sat * (0.5f + 0.5f * a), 1.0f) * (a * a * 1.3f + 0.1f);
        draw_spark(p.x, p.y, c);
        draw_spark(p.x - p.vx * 0.03f, p.y - p.vy * 0.03f, c * 0.45f);  // short tail
    }
    if (flash_.v > 0.02f)
        glow(buf_, W * 0.5f, shore * 0.5f, W * 0.8f, hsv(palette_.value, 0.4f, 1.0f) * (0.08f * flash_.v));

    // Skyline silhouette with windows lit by the music.
    for (int x = 0; x < W; ++x) {
        const int top = static_cast<int>(skyline_[static_cast<size_t>(x)]);
        for (int y = top; y < static_cast<int>(shore); ++y) {
            RGB c{5, 6, 12};
            if (windows_[static_cast<size_t>(y * W + x)])
                c = RGB{255, 200, 110} * (0.25f + 0.5f * spectrum_.value[static_cast<size_t>(x * spectrum_.value.size() / W)]);
            buf_.set(x, y, c);
        }
    }
    // Quay line.
    rect(buf_, 0, static_cast<int>(shore), W, static_cast<int>(shore) + 1, RGB{22, 20, 28});

    buf_.blit(canvas);
    return true;
}

SceneDescriptor AudioFireworksScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "fireworks";
    d.tags = {"music", "fireworks", "night", "party", "celebration", "audio-reactive", "beat-driven", "drop-reactive",
              "music-showcase", "style:party", "style:edm"};
    d.intensity = 0.78f;
    d.motion = 0.70f;
    d.music_affinity = 1.0f;
    d.performance_cost = 0.40f;
    return d;
}
}  // namespace Scenes
