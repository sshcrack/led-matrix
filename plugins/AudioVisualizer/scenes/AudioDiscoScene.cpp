#include "MusicVistaScenes.h"

namespace Scenes {
using namespace AudioKit;

void AudioDiscoScene::register_properties()
{
    add_property(sensitivity_);
    add_property(spinSpeed_);
    add_property(spotCount_);
}

void AudioDiscoScene::init_world()
{
    spots_.clear();
    const int n = spotCount_->get();
    for (int i = 0; i < n; ++i)
        spots_.push_back({hash(i * 7u + 1u) * Tau, (hash(i * 13u + 3u) - 0.5f) * 1.1f, 1.1f + hash(i * 19u) * 1.4f,
                          static_cast<int>(hash(i * 31u + 2u) * 32.0f), (hash(i * 41u) - 0.5f) * 90.0f});
    tileHue_.assign(4 * 8, 0.0f);
    tileLit_.assign(4 * 8, 0.0f);
}

bool AudioDiscoScene::render(rgb_matrix::FrameCanvas* canvas)
{
    AudioState::Snapshot audio;
    float dt = 0;
    canvas->Clear();
    if (!begin_frame(audio, dt))
        return false;
    const int W = matrix_width, H = matrix_height;
    if (spots_.size() != static_cast<size_t>(spotCount_->get()) || tileHue_.empty())
        init_world();
    spectrum_.update(audio, dt, 0.04f, 0.25f);

    const float kickF = feature(audio, AudioProtocol::Feature::Kick);
    const float energy = spectrum_.level;
    if (events_.beatHit) {
        beat_.hit();
        ++beatCount_;
        // A new floor pattern every beat; colours from the current palette.
        const int pattern = static_cast<int>(beatCount_ % 4);
        for (int i = 0; i < static_cast<int>(tileLit_.size()); ++i) {
            const int r = i / 8, c = i % 8;
            bool on = false;
            switch (pattern) {
            case 0: on = (r + c) % 2 == 0; break;
            case 1: on = (r + c) % 2 == 1; break;
            case 2: on = c == static_cast<int>(beatCount_ / 4 % 8) || c == 7 - static_cast<int>(beatCount_ / 4 % 8); break;
            default: on = hash(beatCount_ * 97u + static_cast<uint32_t>(i)) < 0.45f; break;
            }
            if (on) {
                tileLit_[static_cast<size_t>(i)] = 1.0f;
                tileHue_[static_cast<size_t>(i)] = palette_.value + (hash(static_cast<uint32_t>(i) * 5u + beatCount_) - 0.5f) * 120.0f;
            }
        }
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
    kick_.decay(dt, 0.15f);
    drop_.decay(dt, 2.0f);
    palette_.update(dt, 0.8f);
    for (auto& t : tileLit_) t *= std::exp(-dt / 0.45f);
    const float bpm = std::clamp(audio.feature(AudioProtocol::Feature::Bpm), 70.0f, 180.0f);
    // One revolution every 16 beats, faster for a moment after a drop.
    spin_ += dt * Tau * (bpm / 60.0f / 16.0f) * spinSpeed_->get() * (1.0f + 2.0f * drop_.v);

    const int floorY = static_cast<int>(H * 0.66f);
    const float ballX = W * 0.5f, ballY = H * 0.24f, ballR = std::min(W, H) * 0.12f;

    // --- room: back wall, side walls in one-point perspective ------------------------------
    const RGB wallTint = hsv(palette_.value, 0.6f, 1.0f);
    for (int y = 0; y < floorY; ++y)
        for (int x = 0; x < W; ++x) {
            const float vignette = 1.0f - 0.5f * std::fabs(x - W * 0.5f) / (W * 0.5f);
            buf_.at(x, y) = RGB{8, 5, 14} * vignette + wallTint * (0.02f + 0.04f * energy);
        }

    // --- floor: 8x4 light-up tiles in perspective --------------------------------------------
    const float vx = W * 0.5f;  // vanishing point x
    for (int y = floorY; y < H; ++y) {
        const float depth = static_cast<float>(y - floorY + 1) / (H - floorY);  // 0 far .. 1 near
        const float rowF = std::pow(depth, 0.7f) * 4.0f;
        const int row = std::clamp(static_cast<int>(rowF), 0, 3);
        const float spread = 0.55f + 0.85f * depth;
        for (int x = 0; x < W; ++x) {
            const float u = (x + 0.5f - vx) / (W * spread) + 0.5f;  // 0..1 across the floor
            if (u < 0.0f || u >= 1.0f) {
                buf_.at(x, y) = RGB{6, 4, 8};
                continue;
            }
            const float colF = u * 8.0f;
            const int col = std::clamp(static_cast<int>(colF), 0, 7);
            const bool seam = (colF - col) < 0.08f || (rowF - row) < 0.10f;
            const size_t ti = static_cast<size_t>(row * 8 + col);
            const float lit = tileLit_[ti] * (0.5f + 0.5f * energy) + 0.04f;
            RGB c = hsv(tileHue_[ti], 0.75f, 1.0f) * (lit * 0.85f);
            if (seam)
                c = c * 0.25f;
            buf_.at(x, y) = c + RGB{3, 2, 5};
        }
    }

    // --- light beams onto the ball from two spots in the ceiling corners ------------------------
    for (int s = 0; s < 2; ++s) {
        const float sx = s == 0 ? 2.0f : W - 3.0f, sy = 1.0f;
        const float dx = ballX - sx, dy = ballY - sy;
        const float len = AudioKit::len(dx, dy);
        const RGB col = hsv(palette_.value + (s == 0 ? 0.0f : 140.0f), 0.55f, 1.0f) * (0.10f + 0.10f * beat_.v);
        for (int t = 0; t < static_cast<int>(len); ++t) {
            const float k = t / len;
            const float px = sx + dx * k, py = sy + dy * k;
            const float w = 0.5f + k * ballR * 0.6f;
            for (int o = static_cast<int>(-w); o <= static_cast<int>(w); ++o) {
                const float a = 1.0f - std::fabs(static_cast<float>(o)) / (w + 1.0f);
                buf_.add(static_cast<int>(px - dy / len * o), static_cast<int>(py + dx / len * o), col * a);
            }
        }
    }

    // --- reflected light spots sweeping around the room -----------------------------------------
    for (const auto& sp : spots_) {
        // Azimuth relative to the viewer; only the half facing the walls is visible.
        const float az = std::remainder(sp.lon + spin_, Tau);
        const float facing = std::cos(az);
        if (facing < -0.2f)
            continue;
        const float x = W * 0.5f + std::sin(az) * W * 0.62f;
        const float y = ballY + sp.lat * H * 0.55f;
        if (y > H)
            continue;
        const float level = spectrum_.value[static_cast<size_t>(sp.band % static_cast<int>(spectrum_.value.size()))];
        const float bright = (0.25f + 0.9f * level + 0.3f * beat_.v) * std::clamp(facing + 0.2f, 0.0f, 1.0f);
        // White spots normally, full colour after a drop.
        const RGB col = lerp(RGB{255, 245, 235}, hsv(palette_.value + sp.hueOffset, 0.85f, 1.0f), 0.35f + 0.65f * drop_.v);
        const float stretch = 1.0f + std::fabs(std::sin(az)) * 0.8f;  // grazing angle near the edges
        const float r = sp.size * (y > floorY ? 1.4f : 1.0f);
        for (int yy = static_cast<int>(y - r - 1); yy <= static_cast<int>(y + r + 1); ++yy)
            for (int xx = static_cast<int>(x - r * stretch - 1); xx <= static_cast<int>(x + r * stretch + 1); ++xx) {
                const float d = AudioKit::len((xx + 0.5f - x) / stretch, yy + 0.5f - y) / r;
                if (d < 1.2f)
                    buf_.add(xx, yy, col * (bright * 0.75f * smoothstep(1.2f, 0.4f, d)));
            }
    }

    // --- the mirror ball ---------------------------------------------------------------------
    for (int y = static_cast<int>(ballY - ballR - 1); y <= static_cast<int>(ballY + ballR + 1); ++y)
        for (int x = static_cast<int>(ballX - ballR - 1); x <= static_cast<int>(ballX + ballR + 1); ++x) {
            const float nx = (x + 0.5f - ballX) / ballR, ny = (y + 0.5f - ballY) / ballR;
            const float rr = nx * nx + ny * ny;
            if (rr > 1.0f || !buf_.inside(x, y))
                continue;
            const float nz = std::sqrt(1.0f - rr);
            const float lon = std::atan2(nx, nz) + spin_;
            const float lat = std::asin(std::clamp(ny, -1.0f, 1.0f));
            const int fi = static_cast<int>(std::floor(lon / (Tau / 18.0f)));
            const int fj = static_cast<int>(std::floor(lat / (Pi / 10.0f)));
            const float fu = lon / (Tau / 18.0f) - fi, fv = lat / (Pi / 10.0f) - fj;
            const bool grout = fu < 0.12f || fv < 0.12f;
            // Each facet catches a light at a slightly different moment.
            const float glint = hash(static_cast<uint32_t>((fi & 1023) * 131 + (fj + 32) * 7));
            const float spark = std::pow(std::max(0.0f, std::sin(spin_ * 3.0f + glint * 40.0f)), 18.0f);
            const float shade = 0.25f + 0.5f * nz + 0.25f * (-ny);
            RGB c = RGB{120, 120, 135} * (shade * (0.6f + 0.4f * glint)) + RGB{255, 250, 240} * (spark * (0.6f + 0.8f * energy));
            if (grout)
                c = c * 0.3f;
            buf_.at(x, y) = c;
        }
    // Hanging wire.
    for (int y = 0; y < static_cast<int>(ballY - ballR); ++y) buf_.set(static_cast<int>(ballX), y, RGB{40, 40, 46});

    // Kick: the whole room breathes a little.
    if (kick_.v > 0.02f)
        for (auto& p : buf_.px) p = p * (1.0f + kick_.v * 0.15f);

    buf_.blit(canvas);
    return true;
}

SceneDescriptor AudioDiscoScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "disco";
    d.tags = {"music", "disco", "dance", "party", "night", "spectrum", "audio-reactive", "beat-driven", "drop-reactive",
              "music-showcase", "style:party", "style:edm"};
    d.intensity = 0.70f;
    d.motion = 0.62f;
    d.music_affinity = 1.0f;
    d.performance_cost = 0.45f;
    return d;
}
}  // namespace Scenes
