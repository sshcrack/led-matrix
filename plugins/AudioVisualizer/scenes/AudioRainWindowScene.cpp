#include "MusicVistaScenes.h"

namespace Scenes {
using namespace AudioKit;

void AudioRainWindowScene::register_properties()
{
    add_property(sensitivity_);
    add_property(rain_);
    add_property(showMug_);
}

void AudioRainWindowScene::init_world()
{
    const int W = matrix_width, H = matrix_height;
    bokeh_.clear();
    // Out-of-focus city lights behind the glass. Lower lights are streetlamps
    // and cars (warm), higher ones are office windows (cool / neon).
    for (int i = 0; i < 30; ++i) {
        const float y = H * (0.12f + 0.70f * hash(i * 7u + 1u));
        const float warm = y > H * 0.5f ? 0.75f : 0.35f;
        const bool isWarm = hash(i * 13u + 5u) < warm;
        const float hue = isWarm ? 22.0f + hash(i * 3u) * 26.0f : 170.0f + hash(i * 3u) * 170.0f;
        bokeh_.push_back({W * hash(i * 11u + 3u), y, 2.5f + hash(i * 17u + 2u) * 5.5f, hue, isWarm ? 0.85f : 0.8f,
                          hash(i * 23u) * Tau, static_cast<int>(hash(i * 29u + 9u) * 16.0f), 0.0f});
    }
    beads_.assign(static_cast<size_t>(W * H), 0.0f);
    trail_.assign(static_cast<size_t>(W * H), 0.0f);
    for (int i = 0; i < W * H / 30; ++i) {
        const int x = static_cast<int>(rnd() * W), y = static_cast<int>(rnd() * H);
        beads_[static_cast<size_t>(y * W + x)] = 0.35f + rnd() * 0.65f;
    }
    drops_.clear();
}

bool AudioRainWindowScene::render(rgb_matrix::FrameCanvas* canvas)
{
    AudioState::Snapshot audio;
    float dt = 0;
    canvas->Clear();
    if (!begin_frame(audio, dt))
        return false;
    const int W = matrix_width, H = matrix_height;
    if (bokeh_.empty() || static_cast<int>(trail_.size()) != W * H)
        init_world();
    spectrum_.update(audio, dt, 0.05f, 0.35f);

    const float kick = feature(audio, AudioProtocol::Feature::Kick);
    const float hihat = feature(audio, AudioProtocol::Feature::Hihat);
    const float bass = spectrum_.band(0.0f, 0.2f);
    if (events_.beatHit)
        beat_.hit(0.5f + 0.5f * kick);
    if (events_.dropHit) {
        drop_.hit();
        palette_.next();
    }
    if (events_.sectionHit)
        palette_.next();
    beat_.decay(dt, 0.25f);
    drop_.decay(dt, 2.2f);
    palette_.update(dt, 2.5f);

    const float rain = rain_->get();
    // Running drops: a beat releases one, a drop a whole sheet.
    auto release = [&](float x) { drops_.push_back({x, rnd() * H * 0.45f, 0.0f, 0.9f + rnd() * 0.8f, rnd() * Tau}); };
    if (events_.beatHit && rnd() < 0.85f * rain)
        release(rnd() * W);
    if (events_.dropHit)
        for (int i = 0; i < static_cast<int>(10 * rain); ++i) release(rnd() * W);
    // New beads land on the glass with the hi-hats.
    if (events_.onsetHit)
        for (int i = 0; i < static_cast<int>((4 + hihat * 10) * rain); ++i) {
            const int x = static_cast<int>(rnd() * W), y = static_cast<int>(rnd() * H);
            beads_[static_cast<size_t>(y * W + x)] = 0.5f + rnd() * 0.5f;
        }

    // --- background: night sky haze + bokeh ------------------------------------
    const RGB tint = hsv(palette_.value, 0.65f, 1.0f);
    for (int y = 0; y < H; ++y) {
        const float t = static_cast<float>(y) / H;
        const RGB base = lerp(RGB{6, 7, 18}, RGB{18, 12, 22}, t) + tint * (0.03f + 0.03f * drop_.v);
        for (int x = 0; x < W; ++x) buf_.at(x, y) = base;
    }
    for (auto& b : bokeh_) {
        const float target = spectrum_.value[static_cast<size_t>(b.band * 2 % spectrum_.value.size())];
        b.level += (target - b.level) * (1.0f - std::exp(-dt / 0.12f));
        const float flicker = 0.85f + 0.15f * std::sin(time_ * 0.7f + b.phase);
        // Cool lights take on the current palette tint; warm lights stay warm.
        const float hue = b.hue > 150.0f ? palette_.value + (b.hue - 260.0f) * 0.4f : b.hue;
        const RGB col = hsv(hue, b.sat, 1.0f) * ((0.16f + 0.85f * b.level + 0.15f * beat_.v) * flicker);
        const int r = static_cast<int>(std::ceil(b.r + 1));
        for (int y = static_cast<int>(b.y) - r; y <= static_cast<int>(b.y) + r; ++y)
            for (int x = static_cast<int>(b.x) - r; x <= static_cast<int>(b.x) + r; ++x) {
                if (!buf_.inside(x, y))
                    continue;
                const float d = std::hypot(x + 0.5f - b.x, y + 0.5f - b.y) / b.r;
                if (d > 1.15f)
                    continue;
                // Bokeh discs have a slightly brighter rim.
                const float a = smoothstep(1.15f, 0.9f, d) * (0.65f + 0.35f * smoothstep(0.55f, 0.95f, d));
                buf_.add(x, y, col * a);
            }
    }

    // --- running drops and their wet trails --------------------------------------
    for (auto& t : trail_) t *= std::exp(-dt / 2.5f);
    for (auto& d : drops_) {
        const float gravity = 30.0f + 40.0f * bass;
        d.vy = std::min(d.vy + gravity * dt, 26.0f + 30.0f * bass);
        // Drops stick and slip, which reads much more like real glass.
        const float slip = 0.55f + 0.45f * std::sin(time_ * 3.0f + d.wobble * 5.0f);
        d.y += d.vy * dt * slip;
        d.x += std::sin(d.y * 0.15f + d.wobble) * dt * 2.0f;
        const int tx = static_cast<int>(d.x), ty = static_cast<int>(d.y);
        if (buf_.inside(tx, ty)) {
            trail_[static_cast<size_t>(ty * W + tx)] = 1.0f;
            beads_[static_cast<size_t>(ty * W + tx)] = 0.0f;  // the drop collects beads
        }
    }
    std::erase_if(drops_, [&](const Drop& d) { return d.y > H + 3; });

    // Glass: beads and trails refract (brighten) whatever is behind them.
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const size_t i = static_cast<size_t>(y * W + x);
            const float wet = std::max(beads_[i], trail_[i] * 0.55f);
            if (wet <= 0.01f)
                continue;
            // Sample a pixel from above to fake refraction of the inverted image.
            const int sy = std::clamp(y - 3, 0, H - 1);
            const RGB behind = buf_.at(x, sy);
            buf_.at(x, y) = lerp(buf_.at(x, y), behind * 1.8f + RGB{26, 30, 40}, wet * 0.7f);
        }
    for (const auto& d : drops_) {
        const RGB behind = buf_.inside(static_cast<int>(d.x), static_cast<int>(d.y) - 4)
                               ? buf_.at(static_cast<int>(d.x), static_cast<int>(d.y) - 4)
                               : RGB{};
        buf_.disc(d.x, d.y, d.size, behind * 1.2f + RGB{50, 56, 70});
        buf_.splat(d.x - 0.5f, d.y - 0.7f, RGB{150, 150, 165});  // specular
    }

    // --- window frame ------------------------------------------------------------
    const RGB frame{14, 11, 12};
    const RGB frameLit = frame + tint * 0.06f;
    const int mx = static_cast<int>(W * 0.62f), my = static_cast<int>(H * 0.36f);
    const int sill = H - 14;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const bool border = x < 3 || x >= W - 3 || y < 3;
            const bool mullion = std::abs(x - mx) <= 1 || std::abs(y - my) <= 1;
            if (border || mullion)
                buf_.at(x, y) = (x == mx - 2 || y == my - 2) ? frameLit : frame;
        }
    // Sill with a warm room glow from below.
    for (int y = sill; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const float t = static_cast<float>(y - sill) / (H - sill);
            buf_.at(x, y) = lerp(RGB{40, 28, 22}, RGB{20, 14, 12}, t) * (y == sill ? 1.4f : 1.0f);
        }

    // --- mug with steam and a little plant: the lo-fi corner ------------------------
    if (showMug_->get()) {
        const float cx = W * 0.24f;
        const int top = sill - 13;
        for (int y = top; y < sill; ++y)
            for (int x = static_cast<int>(cx - 6); x <= static_cast<int>(cx + 6); ++x)
                buf_.set(x, y, lerp(RGB{150, 70, 50}, RGB{90, 38, 30}, static_cast<float>(x - cx + 6) / 12.0f));
        for (int y = top + 3; y < top + 10; ++y) {  // handle
            buf_.set(static_cast<int>(cx + 8), y, RGB{110, 50, 38});
            if (y == top + 3 || y == top + 9)
                buf_.set(static_cast<int>(cx + 7), y, RGB{110, 50, 38});
        }
        // Steam wisps sway with the mids.
        const float mids = spectrum_.band(0.25f, 0.6f);
        for (int w = 0; w < 3; ++w)
            for (int k = 0; k < 22; ++k) {
                const float yy = top - 2 - k * 1.1f;
                const float xx = cx - 3 + w * 3 + std::sin(k * 0.35f - time_ * 1.6f + w * 2.0f) * (1.0f + k * 0.12f) * (0.6f + mids);
                const float a = (1.0f - k / 22.0f) * (0.14f + 0.16f * mids);
                buf_.splat(xx, yy, RGB{200, 200, 210} * a);
            }
        // Plant: a pot and a few leaves that bob on the beat.
        const float px = W * 0.84f;
        for (int y = sill - 9; y < sill; ++y)
            for (int x = static_cast<int>(px - 5 + (y - sill + 9) * 0.2f); x <= static_cast<int>(px + 5 - (y - sill + 9) * 0.2f); ++x)
                buf_.set(x, y, RGB{90, 50, 32});
        for (int l = 0; l < 6; ++l) {
            const float ang = -Pi * 0.5f + (l - 2.5f) * 0.42f + std::sin(time_ * 0.8f + l) * 0.06f;
            const float len = 9.0f + (l % 3) * 3.0f - beat_.v * 1.2f;
            for (int t = 0; t < static_cast<int>(len); ++t) {
                const float lx = px + std::cos(ang) * t, ly = sill - 9 + std::sin(ang) * t + t * t * 0.03f;
                const float wdt = std::sin(static_cast<float>(t) / len * Pi) * 1.6f;
                buf_.disc(lx, ly, wdt, RGB{30, 78, 40}, false);
            }
        }
    }

    // Warm room light spilling onto the glass at the bottom.
    for (int y = H / 2; y < sill; ++y) {
        const float a = static_cast<float>(y - H / 2) / (sill - H / 2);
        for (int x = 0; x < W; ++x) buf_.add(x, y, RGB{18, 9, 4} * (a * a * (0.6f + 0.4f * beat_.v)));
    }

    buf_.blit(canvas);
    return true;
}

SceneDescriptor AudioRainWindowScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "window";
    d.tags = {"music", "rain", "night", "city", "cozy", "spectrum", "audio-reactive", "beat-driven", "music-showcase",
              "style:hiphop", "style:chill"};
    d.intensity = 0.32f;
    d.motion = 0.35f;
    d.music_affinity = 1.0f;
    d.performance_cost = 0.35f;
    return d;
}
}  // namespace Scenes
