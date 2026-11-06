#include "GenreVistaScenes.h"

namespace Scenes {
using namespace AudioKit;

namespace {
// Value noise tiled horizontally with period `period` cells.
float vnoise(float x, float y, int period, uint32_t seed)
{
    const int xi = static_cast<int>(std::floor(x)), yi = static_cast<int>(std::floor(y));
    const float fx = x - xi, fy = y - yi;
    auto h = [&](int ix, int iy) {
        const int wx = ((ix % period) + period) % period;
        return hash(static_cast<uint32_t>(wx * 73856093) ^ static_cast<uint32_t>(iy * 19349663) ^ seed);
    };
    const float sx = fx * fx * (3 - 2 * fx), sy = fy * fy * (3 - 2 * fy);
    const float a = h(xi, yi) + (h(xi + 1, yi) - h(xi, yi)) * sx;
    const float b = h(xi, yi + 1) + (h(xi + 1, yi + 1) - h(xi, yi + 1)) * sx;
    return a + (b - a) * sy;
}
}  // namespace

void AudioStormFrontScene::register_properties()
{
    add_property(sensitivity_);
    add_property(rainAmount_);
    add_property(boltRate_);
}

void AudioStormFrontScene::init_world()
{
    const int W = matrix_width, H = matrix_height;
    cloudA_.assign(static_cast<size_t>(W * H), 0.0f);
    cloudB_.assign(static_cast<size_t>(W * H), 0.0f);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const float fx = static_cast<float>(x) / W, fy = static_cast<float>(y) / H;
            auto fbm = [&](uint32_t seed, float scale) {
                float v = 0, amp = 0.55f, tot = 0;
                int period = 4;
                for (int o = 0; o < 4; ++o) {
                    v += amp * vnoise(fx * period * scale, fy * 5.0f * (o + 1) * 0.5f * scale, period, seed + o);
                    tot += amp;
                    amp *= 0.5f;
                    period *= 2;
                }
                return v / tot;
            };
            cloudA_[static_cast<size_t>(y * W + x)] = fbm(11u, 1.0f);
            cloudB_[static_cast<size_t>(y * W + x)] = fbm(97u, 1.0f);
        }
    rain_.clear();
    for (int i = 0; i < 220; ++i) rain_.push_back({rnd() * W, rnd() * H, 0.7f + rnd() * 0.6f});
    bolts_.clear();
}

void AudioStormFrontScene::strike(float x)
{
    const int W = matrix_width, H = matrix_height;
    Bolt b;
    float cx = x, cy = 0.0f;
    const float ground = H * 0.80f;
    b.pts.push_back({cx, cy});
    while (cy < ground) {
        cy += 3.0f + rnd() * 4.0f;
        cx += (rnd() - 0.5f) * 9.0f;
        cx = std::clamp(cx, 2.0f, W - 3.0f);
        b.pts.push_back({cx, std::min(cy, ground)});
        // Side branches leave from the middle of the bolt.
        if (rnd() < 0.22f && cy > H * 0.2f) {
            float bx = cx, by = cy;
            const float dir = rnd() < 0.5f ? -1.0f : 1.0f;
            b.branch.push_back({bx, by});
            for (int k = 0; k < 4; ++k) {
                bx += dir * (2.0f + rnd() * 4.0f);
                by += 2.0f + rnd() * 3.0f;
                b.branch.push_back({bx, by});
            }
            b.branch.push_back({-1.0f, -1.0f});  // separator
        }
    }
    b.life = 1.0f;
    bolts_.push_back(std::move(b));
    if (bolts_.size() > 4)
        bolts_.erase(bolts_.begin());
}

bool AudioStormFrontScene::render(rgb_matrix::FrameCanvas* canvas)
{
    AudioState::Snapshot audio;
    float dt = 0;
    canvas->Clear();
    if (!begin_frame(audio, dt))
        return false;
    const int W = matrix_width, H = matrix_height;
    if (cloudA_.size() != static_cast<size_t>(W * H))
        init_world();
    spectrum_.update(audio, dt, 0.05f, 0.25f);

    const float kick = feature(audio, AudioProtocol::Feature::Kick);
    const float snare = feature(audio, AudioProtocol::Feature::Snare);
    const float treble = spectrum_.band(0.55f, 1.0f);
    if (events_.onsetHit && (kick > 0.5f || snare > 0.5f) && rnd() < 0.55f * boltRate_->get()) {
        strike(W * (0.1f + 0.8f * rnd()));
        flash_.hit(0.7f + 0.3f * rnd());
    }
    if (events_.onsetHit && kick > 0.35f)
        kick_.hit(kick);
    if (events_.dropHit) {
        drop_.hit();
        palette_.next();
        for (int i = 0; i < 3; ++i) strike(W * (0.1f + 0.8f * rnd()));
        flash_.hit();
    }
    if (events_.sectionHit)
        palette_.next();
    flash_.decay(dt, 0.18f);
    kick_.decay(dt, 0.2f);
    drop_.decay(dt, 2.0f);
    palette_.update(dt, 2.0f);
    for (auto& b : bolts_) b.life -= dt / 0.35f;
    std::erase_if(bolts_, [](const Bolt& b) { return b.life <= 0.0f; });
    wind_ += dt * (6.0f + 22.0f * spectrum_.level);

    const float ground = H * 0.80f;
    const RGB tint = hsv(palette_.value, 0.55f, 1.0f);
    const float lightning = std::min(1.0f, flash_.v);

    // --- sky and two cloud decks ---------------------------------------------------------------------------
    for (int y = 0; y < static_cast<int>(ground); ++y) {
        const float t = static_cast<float>(y) / ground;
        const RGB sky = lerp(RGB{8, 9, 20}, RGB{26, 24, 40}, t) + tint * 0.03f;
        for (int x = 0; x < W; ++x) {
            const int ox = (x + static_cast<int>(wind_ * 0.4f)) % W;
            const int ox2 = (x + static_cast<int>(wind_ * 0.9f)) % W;
            const float a = cloudA_[static_cast<size_t>(y * W + ox)], b2 = cloudB_[static_cast<size_t>(y * W + ox2)];
            const float top = 1.0f - t;  // clouds thick near the top
            const float dens = std::clamp(((a * 0.6f + b2 * 0.5f) - 0.28f) * 2.4f * (0.35f + top), 0.0f, 1.0f);
            const RGB cloud = RGB{20, 20, 30} + tint * (0.05f + 0.10f * lightning) + RGB{220, 225, 255} * (lightning * 0.45f * dens);
            buf_.at(x, y) = lerp(sky, cloud, dens);
        }
    }

    // --- bolts -----------------------------------------------------------------------------------------------
    for (const auto& b : bolts_) {
        const float a = std::clamp(b.life, 0.0f, 1.0f);
        const RGB core = RGB{235, 240, 255} * (1.4f * a);
        for (size_t i = 1; i < b.pts.size(); ++i) {
            line(buf_, b.pts[i - 1].first, b.pts[i - 1].second, b.pts[i].first, b.pts[i].second, core);
            glow(buf_, b.pts[i].first, b.pts[i].second, 6.0f, RGB{110, 120, 230} * (0.35f * a));
        }
        for (size_t i = 1; i < b.branch.size(); ++i) {
            if (b.branch[i].first < 0.0f || b.branch[i - 1].first < 0.0f)
                continue;
            line(buf_, b.branch[i - 1].first, b.branch[i - 1].second, b.branch[i].first, b.branch[i].second, core * 0.5f);
        }
    }

    // --- field, fence line, and one lone tree ------------------------------------------------------------------
    for (int y = static_cast<int>(ground); y < H; ++y) {
        const float t = static_cast<float>(y - ground) / (H - ground);
        for (int x = 0; x < W; ++x) {
            const float grass = 0.7f + 0.3f * hash(static_cast<uint32_t>(x * 31 + y * 7));
            buf_.at(x, y) = RGB{8, 14, 12} * grass * (1.0f - 0.4f * t) + RGB{150, 160, 220} * (lightning * 0.12f * (1.0f - t));
        }
    }
    for (int x = 0; x < W; ++x) {
        const float hill = ground - 3.0f + std::sin(x * 0.07f) * 2.0f + std::sin(x * 0.19f) * 1.0f;
        for (int y = static_cast<int>(hill); y < static_cast<int>(ground) + 1; ++y) buf_.set(x, y, RGB{6, 9, 10});
    }
    {
        const float tx = W * 0.78f;
        const float sway = std::sin(time_ * 1.3f) * (0.6f + 2.0f * spectrum_.level);
        for (int y = static_cast<int>(ground) - 20; y < static_cast<int>(ground) + 1; ++y) buf_.set(static_cast<int>(tx), y, RGB{5, 6, 8});
        for (int k = 0; k < 7; ++k) {
            const float ang = -Pi * 0.5f + (k - 3) * 0.45f;
            const float l = 9.0f - std::fabs(k - 3.0f);
            for (int s = 1; s < static_cast<int>(l); ++s)
                buf_.disc(tx + std::cos(ang) * s + sway * s * 0.1f, ground - 20 + std::sin(ang) * s + s * 0.2f, 1.6f, RGB{6, 8, 10}, false);
        }
    }

    // --- rain streaks ----------------------------------------------------------------------------------------------
    const int active = static_cast<int>(rain_.size() * std::clamp((0.25f + treble * 1.1f) * 0.5f * rainAmount_->get(), 0.0f, 1.0f));
    const float slant = 0.25f + 0.002f * wind_ * 0.1f;
    for (int i = 0; i < active; ++i) {
        auto& r = rain_[static_cast<size_t>(i)];
        r.y += dt * (70.0f + 70.0f * r.speed);
        r.x += dt * (70.0f + 70.0f * r.speed) * slant;
        if (r.y > H) {
            r.y -= H;
            r.x = rnd() * W;
        }
        if (r.x > W)
            r.x -= W;
        const float a = 0.25f * r.speed + 0.5f * lightning;
        line(buf_, r.x, r.y, r.x - slant * 3.0f, r.y - 3.0f, RGB{150, 165, 230} * a);
    }

    // Whole-frame flash and low-frequency rumble.
    if (lightning > 0.02f)
        for (auto& p : buf_.px) p = p + RGB{120, 125, 170} * (0.3f * lightning);
    if (kick_.v > 0.02f)
        for (auto& p : buf_.px) p = p * (1.0f + kick_.v * 0.10f);
    buf_.blit(canvas);
    return true;
}

SceneDescriptor AudioStormFrontScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "storm";
    d.tags = {"music", "rock", "metal", "storm", "weather", "lightning", "night", "audio-reactive", "beat-driven",
              "drop-reactive", "music-showcase", "style:rock"};
    d.intensity = 0.72f;
    d.motion = 0.55f;
    d.music_affinity = 0.95f;
    d.performance_cost = 0.55f;
    return d;
}
}  // namespace Scenes
