#include "AmbientMusicScenes.h"

namespace Scenes {
using namespace AudioKit;

void AudioLighthouseScene::register_properties()
{
    add_property(sensitivity_);
    add_property(beatsPerTurn_);
    add_property(storm_);
}

bool AudioLighthouseScene::render(rgb_matrix::FrameCanvas* canvas)
{
    AudioState::Snapshot audio;
    float dt = 0;
    canvas->Clear();
    if (!begin_frame(audio, dt))
        return false;
    const int W = matrix_width, H = matrix_height;
    constexpr int Layers = 6;
    if (layerLevel_.empty())
        layerLevel_.assign(Layers, 0.0f);
    spectrum_.update(audio, dt, 0.06f, 0.40f);

    const float kick = feature(audio, AudioProtocol::Feature::Kick);
    if (events_.beatHit)
        beat_.hit(0.4f + 0.6f * kick);
    if (events_.dropHit) {
        drop_.hit();
        if (storm_->get()) {
            lightning_.hit();
            bolt_.clear();
            float x = W * (0.15f + 0.45f * rnd()), y = 0.0f;
            while (y < H * 0.52f) {
                bolt_.emplace_back(x, y);
                x += (rnd() - 0.5f) * 9.0f;
                y += 3.0f + rnd() * 5.0f;
            }
        }
    }
    beat_.decay(dt, 0.25f);
    drop_.decay(dt, 1.5f);
    lightning_.decay(dt, 0.18f);

    // Beam rotation locked to the tempo: one turn every N beats.
    float bpm = audio.feature(AudioProtocol::Feature::Bpm);
    if (bpm < 60.0f || bpm > 200.0f || audio.feature(AudioProtocol::Feature::BeatConfidence) < 0.05f)
        bpm = bpm >= 60.0f && bpm <= 200.0f ? bpm : 100.0f;
    beamAngle_ = std::fmod(beamAngle_ + dt * Tau * (bpm / 60.0f) / static_cast<float>(beatsPerTurn_->get()), Tau);

    // Far waves = highs, near waves = bass.
    for (int k = 0; k < Layers; ++k) {
        const float from = 1.0f - (k + 1.0f) / Layers, to = 1.0f - static_cast<float>(k) / Layers;
        const float target = spectrum_.band(from, to);
        auto& l = layerLevel_[static_cast<size_t>(k)];
        l += (target - l) * (1.0f - std::exp(-dt / (target > l ? 0.10f : 0.6f)));
    }

    const int horizon = static_cast<int>(H * 0.52f);
    const float flash = lightning_.v;

    // --- sky ---------------------------------------------------------------
    for (int y = 0; y < horizon; ++y) {
        const float t = static_cast<float>(y) / horizon;
        const RGB c = lerp(RGB{4, 6, 22}, RGB{28, 34, 70}, t * t) + RGB{110, 110, 150} * (flash * 0.6f);
        for (int x = 0; x < W; ++x) buf_.at(x, y) = c;
    }
    const float treble = spectrum_.band(0.65f, 1.0f);
    for (int i = 0; i < 70; ++i) {
        const int sx = static_cast<int>(hash(i * 5 + 3) * W), sy = static_cast<int>(hash(i * 23 + 7) * horizon * 0.8f);
        const float tw = 0.35f + 0.65f * std::fabs(std::sin(time_ * (0.4f + hash(i) * 1.6f) + i));
        buf_.add(sx, sy, RGB{150, 150, 180} * (tw * (0.35f + treble * 0.7f)));
    }
    const float moonX = W * 0.22f, moonY = H * 0.16f;
    buf_.disc(moonX, moonY, 12.0f, RGB{30, 32, 45}, true);
    buf_.disc(moonX, moonY, 5.0f, RGB{235, 232, 210}, false);
    for (size_t i = 1; i < bolt_.size() && flash > 0.05f; ++i) {
        const auto [x0, y0] = bolt_[i - 1];
        const auto [x1, y1] = bolt_[i];
        for (int s = 0; s <= 6; ++s) {
            const float t = s / 6.0f;
            buf_.splat(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, RGB{230, 230, 255} * (flash * 1.6f));
        }
    }

    // --- sea: far to near layers ---------------------------------------------
    // Wave surfaces of all layers first, so each layer only fills down to
    // where the next (nearer, opaque) layer starts instead of to the bottom.
    std::vector<std::vector<float>> surfaces(Layers, std::vector<float>(static_cast<size_t>(W)));
    for (int k = 0; k < Layers; ++k) {
        const float depth = static_cast<float>(k) / (Layers - 1);  // 0 far .. 1 near
        const float baseY = horizon + 2.0f + std::pow(depth, 1.4f) * (H - horizon - 10.0f);
        const float level = layerLevel_[static_cast<size_t>(k)];
        const float amp = (0.6f + depth * 4.2f) * (0.35f + level * 1.3f + (k == Layers - 1 ? beat_.v * 0.8f : 0.0f));
        const float wl = 0.30f - depth * 0.20f;
        const float speed = 0.8f + depth * 1.6f;
        for (int x = 0; x < W; ++x) {
            const float phase = x * wl + time_ * speed + k * 1.7f;
            surfaces[static_cast<size_t>(k)][static_cast<size_t>(x)] =
                baseY - amp * (0.6f * std::sin(phase) + 0.4f * std::sin(phase * 1.9f + 0.6f));
        }
    }
    for (int k = 0; k < Layers; ++k) {
        const float depth = static_cast<float>(k) / (Layers - 1);  // 0 far .. 1 near
        const float level = layerLevel_[static_cast<size_t>(k)];
        const RGB water = lerp(RGB{10, 22, 52}, RGB{16, 48, 86}, depth) + RGB{80, 80, 110} * (flash * 0.25f);
        const auto& surface = surfaces[static_cast<size_t>(k)];
        for (int x = 0; x < W; ++x) {
            const float sy = surface[static_cast<size_t>(x)];
            const float slope = surface[static_cast<size_t>(std::min(x + 1, W - 1))] - surface[static_cast<size_t>(std::max(x - 1, 0))];
            const int fillEnd = k + 1 < Layers
                ? std::clamp(static_cast<int>(std::floor(surfaces[static_cast<size_t>(k + 1)][static_cast<size_t>(x)])), horizon, H)
                : H;
            for (int y = std::max(horizon, static_cast<int>(std::floor(sy))); y < fillEnd; ++y) {
                const float below = y - sy;
                RGB c = water * (below < 16.0f ? 0.8f + 0.2f * std::exp(-below / 4.0f) : 0.8f);
                // Moonlight glitter column (negligible beyond 3 widths).
                const float u = (x - moonX) / (3.0f + (y - horizon) * 0.25f);
                if (u * u < 9.0f
                    && hash(static_cast<uint32_t>(x * 73 + y * 977) + static_cast<uint32_t>(time_ * 6.0f) * 131) > 0.80f)
                    c = c + RGB{120, 120, 110} * (std::exp(-u * u) * 0.8f);
                buf_.at(x, y) = c;
            }
            // Foam on crests, brighter when the band is loud.
            const float crest = std::clamp(0.4f + level * 0.9f - std::fabs(slope) * 0.6f, 0.0f, 1.0f);
            buf_.mix(x, static_cast<int>(std::floor(sy)), RGB{150, 180, 210} * (0.4f + 0.6f * depth), crest * (0.35f + 0.5f * depth));
        }
        // A small boat riding the third layer, with a lantern.
        if (k == 2) {
            const float bx = std::fmod(W * 0.42f + time_ * 1.2f, W + 20.0f) - 10.0f;
            const float by = surface[static_cast<size_t>(std::clamp(static_cast<int>(bx), 0, W - 1))];
            for (int i = -4; i <= 4; ++i) buf_.set(static_cast<int>(bx) + i, static_cast<int>(by) - (std::abs(i) < 4 ? 1 : 2), RGB{30, 20, 16});
            for (int i = -3; i <= 3; ++i) buf_.set(static_cast<int>(bx) + i, static_cast<int>(by) - 2, RGB{40, 26, 18});
            for (int m = 3; m < 12; ++m) buf_.set(static_cast<int>(bx), static_cast<int>(by) - m, RGB{50, 40, 34});
            for (int m = 4; m < 11; ++m)
                for (int s = 1; s <= (11 - m) / 2 + 1; ++s) buf_.mix(static_cast<int>(bx) + s, static_cast<int>(by) - m, RGB{150, 145, 130}, 0.8f);
            buf_.add(static_cast<int>(bx) - 3, static_cast<int>(by) - 4, RGB{255, 180, 70});
            buf_.disc(bx - 3, by - 4, 2.2f, RGB{70, 40, 10});
        }
    }

    // --- cliff + lighthouse ------------------------------------------------
    const float lampX = W * 0.87f, lampY = H * 0.17f;
    for (int x = static_cast<int>(W * 0.72f); x < W; ++x) {
        const float t = (x - W * 0.72f) / (W * 0.28f);
        const float top = horizon - 14.0f + (1.0f - std::min(t * 2.5f, 1.0f)) * 16.0f + 1.5f * std::sin(x * 0.9f) + std::sin(x * 0.37f);
        for (int y = static_cast<int>(top); y < H; ++y) {
            const float rock = hash(static_cast<uint32_t>((x / 3) * 131 + (y / 3) * 17));
            const float shade = (0.65f + 0.35f * rock) * (1.0f - 0.4f * static_cast<float>(y - top) / (H - top));
            buf_.set(x, y, RGB{18, 18, 24} * shade + RGB{30, 30, 40} * (flash * 0.5f));
        }
    }
    const int towerTop = static_cast<int>(lampY + 5), towerBottom = horizon - 13;
    for (int y = towerTop; y <= towerBottom; ++y) {
        const float t = static_cast<float>(y - towerTop) / (towerBottom - towerTop);
        const int half = static_cast<int>(2.0f + t * 2.5f);
        const bool stripe = ((y - towerTop) / 5) % 2 == 1;
        for (int x = static_cast<int>(lampX) - half; x <= static_cast<int>(lampX) + half; ++x) {
            const float side = (x - (lampX - half)) / std::max(1.0f, 2.0f * half);
            const RGB base = stripe ? RGB{150, 30, 30} : RGB{190, 190, 185};
            buf_.set(x, y, base * (0.35f + 0.65f * (1.0f - side)));
        }
    }
    for (int x = static_cast<int>(lampX) - 4; x <= static_cast<int>(lampX) + 4; ++x) buf_.set(x, towerTop, RGB{40, 40, 46});
    for (int x = static_cast<int>(lampX) - 2; x <= static_cast<int>(lampX) + 2; ++x) buf_.set(x, towerTop - 5, RGB{40, 40, 46});

    // --- rotating beam ---------------------------------------------------
    const float dir = std::sin(beamAngle_);    // horizontal extent
    const float facing = std::cos(beamAngle_);  // towards the viewer
    const float beamPower = 0.55f + 0.25f * beat_.v + 0.3f * drop_.v;
    if (std::fabs(dir) > 0.04f) {
        const float length = 160.0f * std::fabs(dir);
        for (int x = 0; x < W; ++x) {
            const float d = (x - lampX) * (dir > 0 ? 1.0f : -1.0f);
            if (d < 0 || d > length)
                continue;
            const float halfWidth = 1.2f + d * 0.10f;
            const float fade = (1.0f - d / length) * beamPower * (0.55f + 0.45f * std::fabs(dir));
            for (int y = static_cast<int>(lampY - 2 - halfWidth); y <= static_cast<int>(lampY - 2 + halfWidth + 1); ++y) {
                const float edge = 1.0f - std::fabs(y - (lampY - 2)) / (halfWidth + 1.0f);
                if (edge > 0)
                    buf_.add(x, y, RGB{255, 235, 170} * (edge * fade * 0.45f));
            }
        }
    }
    const float flare = std::pow(std::max(0.0f, facing), 8.0f);
    buf_.disc(lampX, lampY - 2, 2.0f, RGB{255, 240, 190}, false);
    buf_.disc(lampX, lampY - 2, 4.0f + flare * 8.0f, RGB{255, 220, 140} * (0.25f + flare * 0.9f));

    buf_.blit(canvas);
    return true;
}

SceneDescriptor AudioLighthouseScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "coast";
    d.tags = {"music", "ocean", "nature", "night", "audio-reactive", "music-showcase", "beat-driven", "calm", "style:chill", "style:rock", "style:acoustic", "drop-reactive"};
    d.intensity = 0.40f;
    d.motion = 0.45f;
    d.music_affinity = 0.9f;
    d.performance_cost = 0.35f;
    return d;
}
}  // namespace Scenes
