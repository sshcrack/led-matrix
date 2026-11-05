#include "MusicVistaScenes.h"

namespace Scenes {
using namespace AudioKit;

namespace {
RGB lava_color(float heat)
{
    heat = std::clamp(heat, 0.0f, 1.0f);
    if (heat < 0.35f)
        return lerp(RGB{25, 2, 0}, RGB{150, 18, 2}, heat / 0.35f);
    if (heat < 0.7f)
        return lerp(RGB{150, 18, 2}, RGB{255, 95, 8}, (heat - 0.35f) / 0.35f);
    return lerp(RGB{255, 95, 8}, RGB{255, 225, 120}, (heat - 0.7f) / 0.3f);
}
}  // namespace

void AudioVolcanoScene::register_properties()
{
    add_property(sensitivity_);
    add_property(eruption_);
    add_property(lightning_);
}

void AudioVolcanoScene::init_world()
{
    const int W = matrix_width, H = matrix_height;
    const float peakX = W * 0.5f, craterY = H * 0.36f, baseY = H * 0.80f;
    cone_.assign(static_cast<size_t>(W), static_cast<float>(H));
    for (int x = 0; x < W; ++x) {
        const float d = std::fabs(x + 0.5f - peakX) / (W * 0.62f);
        // Concave slopes, a flat-ish crater lip and a little ruggedness.
        float y = craterY + (baseY - craterY) * std::pow(std::clamp(d, 0.0f, 1.0f), 0.75f);
        if (std::fabs(x + 0.5f - peakX) < 7.0f)
            y = craterY + 1.5f - std::cos((x + 0.5f - peakX) / 7.0f * Pi * 0.5f) * 1.5f;
        y += (hash(static_cast<uint32_t>(x / 3) * 977u) - 0.5f) * 1.6f;
        cone_[static_cast<size_t>(x)] = y;
    }
    // Two or three lava rivers wander down the slope from the crater.
    lava_.assign(static_cast<size_t>(W * H), 0);
    lavaPos_.assign(static_cast<size_t>(W * H), 0.0f);
    for (int r = 0; r < 3; ++r) {
        float x = peakX + (r - 1) * 4.0f;
        const float dir = r == 1 ? (hash(55u) < 0.5f ? -0.25f : 0.25f) : (r == 0 ? -0.55f : 0.55f);
        const int y0 = static_cast<int>(craterY + 2), y1 = static_cast<int>(baseY + 2);
        for (int y = y0; y < y1; ++y) {
            x += dir + (hash(static_cast<uint32_t>(y * 31 + r * 7)) - 0.5f) * 1.1f;
            const float width = 1.0f + (y - y0) * 0.035f;
            for (int dx = static_cast<int>(-width); dx <= static_cast<int>(width); ++dx) {
                const int xx = static_cast<int>(x) + dx;
                if (xx < 0 || xx >= W || y < cone_[static_cast<size_t>(xx)] + 1)
                    continue;
                lava_[static_cast<size_t>(y * W + xx)] = static_cast<uint8_t>(r + 1);
                lavaPos_[static_cast<size_t>(y * W + xx)] = static_cast<float>(y - y0) / (y1 - y0);
            }
        }
    }
    embers_.clear();
    smoke_.clear();
}

bool AudioVolcanoScene::render(rgb_matrix::FrameCanvas* canvas)
{
    AudioState::Snapshot audio;
    float dt = 0;
    canvas->Clear();
    if (!begin_frame(audio, dt))
        return false;
    const int W = matrix_width, H = matrix_height;
    if (static_cast<int>(cone_.size()) != W || static_cast<int>(lava_.size()) != W * H)
        init_world();
    spectrum_.update(audio, dt, 0.04f, 0.30f);

    const float kickF = feature(audio, AudioProtocol::Feature::Kick);
    const float snareF = feature(audio, AudioProtocol::Feature::Snare);
    const float bass = spectrum_.band(0.0f, 0.22f);
    const float energy = spectrum_.level;
    const float strength = eruption_->get();
    const float peakX = W * 0.5f, craterY = H * 0.36f, seaY = H * 0.80f;

    auto fountain = [&](int count, float power) {
        for (int i = 0; i < count; ++i) {
            const float spread = (rnd() - 0.5f) * (0.9f + power * 0.5f);
            const float speed = (28.0f + rnd() * 30.0f) * power;
            embers_.push_back({peakX + (rnd() - 0.5f) * 8.0f, craterY, std::sin(spread) * speed, -std::cos(spread) * speed, 0.0f,
                               1.4f + rnd() * 1.6f, 0.8f + rnd() * 0.2f});
        }
    };
    if (events_.onsetHit && kickF > 0.35f) {
        kick_.hit(0.5f + 0.5f * kickF);
        fountain(static_cast<int>((4 + kickF * 10) * strength), 0.75f + 0.35f * kickF);
    }
    if (events_.beatHit) {
        glow_.hit(0.6f + 0.4f * bass);
        fountain(static_cast<int>((3 + bass * 8) * strength), 0.6f + 0.4f * bass);
    }
    // A steady trickle of spatter keeps the crater alive between hits.
    if (rnd() < dt * (2.0f + bass * 10.0f) * strength)
        fountain(1, 0.45f + 0.3f * bass);
    if (events_.dropHit) {
        drop_.hit();
        fountain(static_cast<int>(90 * strength), 1.5f);
        for (int i = 0; i < 6; ++i) smoke_.push_back({peakX + (rnd() - 0.5f) * 10.0f, craterY - 4, 6.0f + rnd() * 4.0f, 0.0f});
    }
    boltCooldown_ -= dt;
    // Volcanic lightning in the ash cloud on hard snares, rarely.
    if (lightning_->get() && events_.onsetHit && snareF > 0.6f && boltCooldown_ <= 0 && smoke_.size() > 4 && rnd() < 0.35f + drop_.v) {
        boltCooldown_ = 3.0f;
        bolt_env_.hit();
        bolt_.clear();
        float bx = peakX + (rnd() - 0.5f) * W * 0.5f, by = craterY - 30.0f - rnd() * 14.0f;
        for (int s = 0; s < 14; ++s) {
            bolt_.push_back({bx, by});
            bx += (rnd() - 0.5f) * 6.0f;
            by += 1.6f + rnd() * 1.4f;
        }
    }
    kick_.decay(dt, 0.25f);
    glow_.decay(dt, 0.35f);
    drop_.decay(dt, 2.5f);
    bolt_env_.decay(dt, 0.18f);

    // Continuous smoke column, thicker when loud.
    if (rnd() < dt * (3.0f + energy * 6.0f))
        smoke_.push_back({peakX + (rnd() - 0.5f) * 6.0f, craterY - 2, 3.5f + rnd() * 2.0f, 0.0f});

    const float crater = std::clamp(0.35f + bass * 0.6f + glow_.v * 0.25f + drop_.v * 0.6f, 0.0f, 1.6f);

    // --- dusk sky ---------------------------------------------------------------------
    for (int y = 0; y < H; ++y) {
        const float t = static_cast<float>(y) / seaY;
        const RGB sky = t < 1.0f ? lerp(RGB{14, 6, 24}, RGB{70, 20, 26}, t * t) : RGB{};
        for (int x = 0; x < W; ++x) {
            // Glow of the crater lighting the sky around it.
            const float d = AudioKit::len((x - peakX) / W, (y - craterY) / H);
            buf_.at(x, y) = sky + RGB{120, 30, 6} * (crater * 0.35f * std::exp(-d * d * 18.0f));
        }
    }

    // --- ash/smoke puffs lit from below ---------------------------------------------------
    for (auto& p : smoke_) {
        p.life += dt;
        p.y -= dt * (6.0f + p.r * 0.4f);
        p.x += dt * (4.0f + p.life * 1.2f);
        p.r += dt * 2.2f;
    }
    std::erase_if(smoke_, [&](const Puff& p) { return p.y + p.r < -4 || p.life > 14.0f; });
    for (const auto& p : smoke_) {
        const float lit = std::exp(-(craterY - p.y) / 30.0f) * crater;
        const RGB col = lerp(RGB{46, 38, 42}, RGB{150, 60, 24}, lit * 0.6f);
        const float fade = std::clamp(1.0f - p.life / 14.0f, 0.0f, 1.0f);
        const int r = static_cast<int>(p.r + 1);
        for (int y = static_cast<int>(p.y) - r; y <= static_cast<int>(p.y) + r; ++y)
            for (int x = static_cast<int>(p.x) - r; x <= static_cast<int>(p.x) + r; ++x) {
                if (!buf_.inside(x, y))
                    continue;
                const float d = AudioKit::len(x - p.x, y - p.y) / p.r;
                if (d < 1.0f)
                    buf_.mix(x, y, col, (1.0f - d * d) * 0.28f * fade);
            }
    }

    // --- lightning ------------------------------------------------------------------------
    if (bolt_env_.v > 0.05f)
        for (size_t i = 1; i < bolt_.size(); ++i)
            for (int s = 0; s < 4; ++s) {
                const float k = s / 4.0f;
                buf_.splat(bolt_[i - 1].first + (bolt_[i].first - bolt_[i - 1].first) * k,
                           bolt_[i - 1].second + (bolt_[i].second - bolt_[i - 1].second) * k, RGB{200, 190, 255} * bolt_env_.v);
            }

    // --- volcano cone with lava rivers -------------------------------------------------------
    const float flow = time_ * (0.18f + bass * 0.25f);
    for (int x = 0; x < W; ++x)
        for (int y = static_cast<int>(cone_[static_cast<size_t>(x)]); y < static_cast<int>(seaY); ++y) {
            if (y < 0)
                continue;
            const float shade = 0.75f + 0.25f * hash(static_cast<uint32_t>((y / 2) * W + x / 2));
            // Left flank faces the last of the dusk light, right flank is in shadow.
            const float side = x < peakX ? 1.25f : 0.8f;
            RGB rock = RGB{30, 16, 20} * (shade * side);
            // Rim light from the crater.
            const float rim = std::exp(-std::fabs(y - cone_[static_cast<size_t>(x)]) * 0.8f) * std::exp(-std::fabs(x - peakX) / 18.0f);
            rock = rock + RGB{150, 40, 8} * (rim * crater * 0.5f);
            const size_t i = static_cast<size_t>(y * W + x);
            if (lava_[i]) {
                // Pulses travel down the river; overall heat follows the bass.
                const float pulse = 0.5f + 0.5f * std::sin((lavaPos_[i] - flow) * 18.0f + lava_[i] * 2.0f);
                const float heat = (0.25f + 0.45f * bass + 0.25f * pulse + drop_.v * 0.3f) * (1.0f - lavaPos_[i] * 0.45f);
                rock = lava_color(heat);
            }
            buf_.at(x, y) = rock;
        }
    // Crater mouth: a glowing lava lake sunk into the summit, plus a halo.
    for (int y = static_cast<int>(craterY - 2); y <= static_cast<int>(craterY + 3); ++y)
        for (int x = static_cast<int>(peakX - 8); x <= static_cast<int>(peakX + 8); ++x) {
            const float e = std::pow((x + 0.5f - peakX) / 7.0f, 2.0f) + std::pow((y + 0.5f - craterY - 0.5f) / 2.0f, 2.0f);
            if (e < 1.0f)
                buf_.mix(x, y, lava_color(0.55f + crater * 0.3f + (1.0f - e) * 0.2f), smoothstep(1.0f, 0.6f, e));
        }
    for (int y = static_cast<int>(craterY - 14); y < static_cast<int>(craterY + 2); ++y)
        for (int x = static_cast<int>(peakX - 16); x <= static_cast<int>(peakX + 16); ++x) {
            const float d = AudioKit::len((x + 0.5f - peakX) / 1.6f, y + 0.5f - craterY);
            buf_.add(x, y, RGB{255, 90, 20} * (crater * 0.22f * (0.7f + 0.3f * kick_.v) * std::exp(-d / 5.0f)));
        }

    // --- embers / lava bombs --------------------------------------------------------------
    for (auto& e : embers_) {
        e.life += dt;
        e.vy += 38.0f * dt;
        e.x += e.vx * dt;
        e.y += e.vy * dt;
        e.heat -= dt * 0.35f;
    }
    std::erase_if(embers_, [&](const Ember& e) {
        const int xi = std::clamp(static_cast<int>(e.x), 0, W - 1);
        return e.life > e.maxLife || e.y > seaY || (e.vy > 0 && e.y > cone_[static_cast<size_t>(xi)] + 1);
    });
    for (const auto& e : embers_) {
        buf_.splat(e.x, e.y, lava_color(e.heat) * 1.1f);
        buf_.splat(e.x - e.vx * 0.012f, e.y - e.vy * 0.012f, lava_color(e.heat * 0.7f) * 0.5f);
    }

    // --- sea with the reflection -------------------------------------------------------------
    for (int y = static_cast<int>(seaY); y < H; ++y) {
        const float depth = (y - seaY) / (H - seaY);
        for (int x = 0; x < W; ++x) {
            const float wave = std::sin(x * 0.4f + time_ * 1.5f + y * 1.3f) * 1.5f;
            const int sy = static_cast<int>(2 * seaY - y - 1 + wave);
            const RGB refl = buf_.inside(x, sy) ? buf_.at(x, sy) : RGB{};
            buf_.at(x, y) = RGB{4, 6, 12} + refl * (0.45f - depth * 0.25f);
        }
    }

    // Drop flash: a single warm swell, no strobing.
    if (drop_.v > 0.05f) {
        const float a = drop_.v * drop_.v * 0.18f;
        for (auto& p : buf_.px) p = p + RGB{255, 120, 40} * a;
    }

    buf_.blit(canvas);
    return true;
}

SceneDescriptor AudioVolcanoScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "volcano";
    d.tags = {"music", "volcano", "landscape", "dusk", "fire", "audio-reactive", "beat-driven", "drop-reactive", "music-showcase",
              "style:rock", "style:edm"};
    d.intensity = 0.68f;
    d.motion = 0.55f;
    d.music_affinity = 1.0f;
    d.performance_cost = 0.42f;
    return d;
}
}  // namespace Scenes
