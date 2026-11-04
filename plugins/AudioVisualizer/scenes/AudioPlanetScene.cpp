#include "AmbientMusicScenes.h"

#include <array>

namespace Scenes {
using namespace AudioKit;

void AudioPlanetScene::register_properties()
{
    add_property(sensitivity_);
    add_property(ringTilt_);
}

bool AudioPlanetScene::render(rgb_matrix::FrameCanvas* canvas)
{
    AudioState::Snapshot audio;
    float dt = 0;
    canvas->Clear();
    if (!begin_frame(audio, dt))
        return false;
    const int W = matrix_width, H = matrix_height;
    constexpr int Rings = 16;
    if (ringLevel_.empty())
        ringLevel_.assign(Rings, 0.0f);
    spectrum_.update(audio, dt, 0.05f, 0.30f);

    const float kick = feature(audio, AudioProtocol::Feature::Kick);
    if (events_.beatHit)
        beat_.hit(0.5f + 0.5f * kick);
    if (events_.dropHit) {
        drop_.hit();
        palette_.next();
    }
    else if (events_.sectionHit)
        palette_.next();
    beat_.decay(dt, 0.22f);
    drop_.decay(dt, 1.4f);
    palette_.update(dt, 2.5f);
    const float hue = palette_.value;
    const float bass = spectrum_.band(0.0f, 0.2f);
    spin_ += dt * (0.05f + bass * 0.10f + drop_.v * 0.2f);
    moonAngle_ += dt * 0.12f;

    if (events_.onsetHit && meteors_.size() < 6 && rnd() < 0.45f) {
        const float y = rnd() * H * 0.4f;
        meteors_.push_back({W + 4.0f, y, -(70.0f + rnd() * 50.0f), 18.0f + rnd() * 20.0f, 1.0f});
    }

    // Each ring follows one band: inner rings = bass, outer = treble.
    for (int r = 0; r < Rings; ++r) {
        const float target = spectrum_.value[static_cast<size_t>(r * 32 / Rings)];
        auto& l = ringLevel_[static_cast<size_t>(r)];
        l += (target - l) * (1.0f - std::exp(-dt / (target > l ? 0.05f : 0.35f)));
    }

    const float cx = W * 0.5f, cy = H * 0.54f, R = 27.0f + beat_.v * 0.6f;
    const float tilt = ringTilt_->get();
    const float ringIn = R * 1.28f, ringOut = R * 2.25f;
    const float lx = -0.55f, ly = -0.45f, lz = 0.70f;  // light from upper left

    // Starfield + faint nebula. The nebula pattern is static, so it is
    // computed once; per-frame colours are hoisted out of the pixel loops.
    if (nebula_.size() != static_cast<size_t>(W * H)) {
        nebula_.resize(static_cast<size_t>(W * H));
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                const float n = std::sin(x * 0.05f + 1.3f) * std::sin(y * 0.04f + 0.4f) + 0.6f * std::sin((x - y) * 0.03f);
                nebula_[static_cast<size_t>(y * W + x)] = 0.05f + 0.05f * std::max(0.0f, n);
            }
    }
    const RGB nebulaColor = hsv(225.0f + 25.0f * std::sin(hue * 0.0174533f), 0.7f, 1.0f);
    for (int i = 0; i < W * H; ++i) buf_.px[static_cast<size_t>(i)] = nebulaColor * nebula_[static_cast<size_t>(i)];
    for (int i = 0; i < 120; ++i) {
        const uint32_t id = static_cast<uint32_t>(i);
        const int x = static_cast<int>(hash(id * 5 + 3) * W), y = static_cast<int>(hash(id * 11 + 7) * H);
        buf_.add(x, y, RGB{150, 150, 180} * (0.35f + 0.65f * std::fabs(std::sin(time_ * (0.4f + hash(id)) + i))));
    }

    // Per-ring colours (inner/outer edge of each ring, quiet/loud).
    struct RingTone {
        RGB quiet0, loud0, quiet1, loud1;
    };
    std::array<RingTone, Rings> tones{};
    for (int r = 0; r < Rings; ++r) {
        const float t0 = static_cast<float>(r) / Rings, t1 = static_cast<float>(r + 1) / Rings;
        tones[static_cast<size_t>(r)] = {hsv(hue + t0 * 40.0f, 0.55f, 0.45f), hsv(hue + 30.0f + t0 * 60.0f, 0.30f, 1.0f),
                                          hsv(hue + t1 * 40.0f, 0.55f, 0.45f), hsv(hue + 30.0f + t1 * 60.0f, 0.30f, 1.0f)};
    }
    const float ringIn2 = ringIn * ringIn, ringOut2 = ringOut * ringOut;
    auto ringColor = [&](float dist2, float dx, float rdy, float& alpha) -> RGB {
        alpha = 0.0f;
        if (dist2 < ringIn2 || dist2 > ringOut2)
            return {};
        const float dist = std::sqrt(dist2);
        const float t = (dist - ringIn) / (ringOut - ringIn);
        const int ring = std::clamp(static_cast<int>(t * Rings), 0, Rings - 1);
        const float local = t * Rings - ring;
        if (local < 0.12f)  // thin gaps (Cassini-like divisions)
            return {};
        const float level = ringLevel_[static_cast<size_t>(ring)];
        const auto& tone = tones[static_cast<size_t>(ring)];
        // Grain rotating around the planet.
        const float angle = std::atan2(rdy, dx);
        const float grain = 0.75f + 0.25f * std::sin(angle * 9.0f + spin_ * 25.0f * (1.0f - t * 0.5f) + ring * 2.1f);
        alpha = std::clamp(0.35f + level * 0.9f, 0.0f, 1.0f);
        return lerp(lerp(tone.quiet0, tone.quiet1, local), lerp(tone.loud0, tone.loud1, local), level) * grain;
    };

    const RGB haloColor = hsv(hue + 180.0f, 0.5f, 1.0f);
    const RGB bandDark = hsv(hue, 0.55f, 0.55f), bandLight = hsv(hue + 25.0f, 0.35f, 0.95f), stormColor = hsv(hue - 20.0f, 0.7f, 1.0f);
    const float haloReach = R + 14.0f;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
            const float rdy = dy / tilt;
            const bool frontRing = dy > 0.0f;
            const float pd2 = dx * dx + dy * dy;
            float ra = 0.0f;
            const RGB rc = ringColor(dx * dx + rdy * rdy, dx, rdy, ra);
            auto& px = buf_.at(x, y);
            if (!frontRing && ra > 0)
                px = lerp(px, rc, ra);
            if (pd2 < haloReach * haloReach) {
                const float pd = std::sqrt(pd2);
                if (pd < R + 0.5f) {
                    const float nx = dx / R, ny = dy / R;
                    const float nz = std::sqrt(std::max(0.0f, 1.0f - nx * nx - ny * ny));
                    const float diffuse = std::clamp(nx * lx + ny * ly + nz * lz, 0.0f, 1.0f);
                    // Latitude bands drifting at different speeds, warped by bass.
                    const float lat = ny / std::max(0.2f, nz + 0.3f);
                    const float lon = std::atan2(nx, nz) + spin_ * (1.0f + 0.4f * std::sin(lat * 3.0f));
                    const float band = std::sin(lat * 9.0f + std::sin(lon * 2.0f + lat * 4.0f) * (0.5f + bass * 0.8f));
                    RGB surface = lerp(bandDark, bandLight, band * 0.5f + 0.5f);
                    // A storm eye that brightens on beats.
                    const float sx = std::remainder(lon - 1.0f, Tau), sy = lat - 0.35f;
                    const float storm = std::exp(-(sx * sx * 6.0f + sy * sy * 30.0f));
                    surface = lerp(surface, stormColor, storm * (0.6f + 0.4f * beat_.v));
                    // Ring shadow across the planet's southern hemisphere.
                    const float sdx = dx + 4.0f, sdy = (dy - 3.0f) / tilt;
                    const float shadowR2 = sdx * sdx + sdy * sdy;
                    const float shadow = (dy > -2.0f && shadowR2 > ringIn2 && shadowR2 < ringOut2) ? 0.55f : 1.0f;
                    const float inv = 1.0f - nz;
                    const float rim = inv * inv * inv;
                    RGB c = surface * (0.12f + 0.88f * diffuse) * shadow + haloColor * (rim * (0.25f + 0.5f * drop_.v));
                    px = lerp(px, c, std::clamp(R + 0.5f - pd, 0.0f, 1.0f));
                }
                if (frontRing && ra > 0)
                    px = lerp(px, rc, ra);
                // Atmospheric halo.
                if (pd >= R)
                    px = px + haloColor * (0.12f * std::exp(-(pd - R) / 3.0f) * (1.0f + beat_.v));
            } else if (frontRing && ra > 0) {
                px = lerp(px, rc, ra);
            }
        }

    // Small moon orbiting in front/behind.
    const float mxp = cx + std::cos(moonAngle_) * ringOut * 1.15f;
    const float myp = cy + std::sin(moonAngle_) * ringOut * 1.15f * tilt;
    if (std::sin(moonAngle_) > 0.0f || std::hypot(mxp - cx, myp - cy) > R + 3)
        buf_.disc(mxp, myp, 3.0f, RGB{170, 165, 150} * (0.8f + 0.3f * std::sin(moonAngle_)), false);

    for (auto& m : meteors_) {
        m.x += m.vx * dt;
        m.y += m.vy * dt;
        m.life -= dt * 0.9f;
        for (int t = 0; t < 10; ++t)
            buf_.splat(m.x - m.vx * 0.012f * t, m.y - m.vy * 0.012f * t, RGB{230, 220, 255} * (m.life * (1.0f - t / 10.0f)));
    }
    std::erase_if(meteors_, [](const Meteor& m) { return m.life <= 0 || m.x < -20; });

    buf_.blit(canvas);
    return true;
}

SceneDescriptor AudioPlanetScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "planetary";
    d.tags = {"music", "space", "planet", "spectrum", "audio-reactive", "music-showcase", "calm", "style:chill", "style:edm", "drop-reactive"};
    d.intensity = 0.45f;
    d.motion = 0.40f;
    d.music_affinity = 0.95f;
    d.performance_cost = 0.40f;
    return d;
}
}  // namespace Scenes
