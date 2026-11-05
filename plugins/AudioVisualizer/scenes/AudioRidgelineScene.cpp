#include "AmbientMusicScenes.h"

namespace Scenes {
using namespace AudioKit;

bool AmbientMusicScene::begin_frame(AudioState::Snapshot& audio, float& dt)
{
    dt = std::clamp(static_cast<float>(frame_context().delta_seconds), 0.0f, 0.05f);
    audio = AudioState::snapshot();
    if (!audio.fresh())
        return false;
    if (buf_.w != matrix_width || buf_.h != matrix_height)
        buf_.resize(matrix_width, matrix_height);
    time_ += dt;
    events_.update(audio);
    // Sensitivity scales the incoming spectrum before auto-gain.
    if (sensitivity_->get() != 1.0f)
        for (auto& v : audio.spectrum) v = std::clamp(v * sensitivity_->get(), 0.0f, 1.0f);
    return true;
}

void AudioRidgelineScene::register_properties()
{
    add_property(sensitivity_);
    add_property(scrollSpeed_);
    add_property(showSun_);
}

bool AudioRidgelineScene::render(rgb_matrix::FrameCanvas* canvas)
{
    AudioState::Snapshot audio;
    float dt = 0;
    canvas->Clear();
    if (!begin_frame(audio, dt))
        return false;
    if (history_.empty()) {
        history_.assign(Rows, std::vector<float>(Cols, 0.0f));
        spectrum_.resize(Cols / 2);
    }
    spectrum_.update(audio, dt, 0.03f, 0.16f);

    const float kick = feature(audio, AudioProtocol::Feature::Kick);
    if (events_.beatHit)
        beat_.hit(0.6f + 0.4f * kick);
    if (events_.dropHit) {
        drop_.hit();
        palette_.next();
    }
    else if (events_.sectionHit)
        palette_.next();
    beat_.decay(dt, 0.18f);
    drop_.decay(dt, 1.2f);
    palette_.update(dt, 1.5f);
    const float hue_ = palette_.value;

    // Fill the live front row from the current spectrum; mirrored so bass sits
    // in the middle of the landscape and treble towards the edges.
    auto writeRow = [&](std::vector<float>& row) {
        for (int c = 0; c < Cols; ++c) {
            const float centred = std::fabs(c - (Cols - 1) * 0.5f) / (Cols * 0.5f);
            const int bin = std::clamp(static_cast<int>(centred * (Cols / 2)), 0, Cols / 2 - 1);
            const float bell = std::exp(-centred * centred * 3.2f);
            row[static_cast<size_t>(c)] = spectrum_.value[static_cast<size_t>(bin)] * (0.30f + 0.70f * bell);
        }
    };
    rowTimer_ += dt * scrollSpeed_->get();
    while (rowTimer_ >= 1.0f) {
        rowTimer_ -= 1.0f;
        head_ = (head_ + Rows - 1) % Rows;  // new front row
    }
    writeRow(history_[static_cast<size_t>(head_)]);

    const int W = matrix_width, H = matrix_height;
    const float horizon = H * 0.36f;

    // Sky gradient + stars.
    const RGB skyTop = hsv(hue_ + 20.0f, 0.85f, 0.06f);
    const RGB skyLow = hsv(hue_ - 10.0f, 0.80f, 0.32f + 0.10f * drop_.v);
    const float treble = spectrum_.band(0.65f, 1.0f);
    for (int y = 0; y < H; ++y) {
        const float t = std::clamp(y / horizon, 0.0f, 1.0f);
        const RGB c = lerp(skyTop, skyLow, t * t);
        for (int x = 0; x < W; ++x) buf_.at(x, y) = y < horizon ? c : skyLow * 0.18f;
    }
    for (int i = 0; i < 70; ++i) {
        const int sx = static_cast<int>(hash(i * 7 + 1) * W);
        const int sy = static_cast<int>(hash(i * 13 + 5) * (horizon - 6));
        const float tw = 0.35f + 0.65f * std::fabs(std::sin(time_ * (0.6f + hash(i) * 2.0f) + i));
        buf_.add(sx, sy, RGB{150, 140, 190} * (tw * (0.45f + treble * 0.8f) * (1.0f - sy / horizon)));
    }

    // Synthwave sun sitting on the horizon (behind the ridges).
    if (showSun_->get()) {
        const float r = 24.0f + beat_.v * 3.0f + drop_.v * 4.0f;
        const float cy = horizon + 2.0f;
        const float stripeScroll = std::fmod(time_ * 3.0f, 6.0f);
        for (int y = static_cast<int>(cy - r - 6); y <= static_cast<int>(cy); ++y)
            for (int x = static_cast<int>(W / 2 - r - 6); x <= static_cast<int>(W / 2 + r + 6); ++x) {
                if (!buf_.inside(x, y))
                    continue;
                const float d = std::hypot(x + 0.5f - W * 0.5f, y + 0.5f - cy);
                const float vt = std::clamp((cy - y) / r, 0.0f, 1.0f);
                // Glow halo
                buf_.add(x, y, hsv(hue_ + 20.0f, 0.9f, 1.0f) * (0.18f * std::exp(-std::max(0.0f, d - r) / 5.0f) * (0.6f + beat_.v)));
                if (d > r)
                    continue;
                // Gaps in the lower half, scrolling downwards.
                const float band = std::fmod((cy - y) + stripeScroll, 6.0f);
                if (vt < 0.55f && band < (0.55f - vt) * 6.0f)
                    continue;
                const RGB c = lerp(hsv(hue_ + 10.0f, 0.85f, 1.0f), hsv(hue_ + 70.0f, 0.75f, 1.0f), vt);
                buf_.mix(x, y, c * (0.85f + 0.15f * beat_.v), std::clamp(r - d + 0.5f, 0.0f, 1.0f));
            }
    }

    // Ridges, back to front so nearer ridges occlude farther ones.
    const float frac = rowTimer_;
    std::vector<float> lineY(static_cast<size_t>(W));
    for (int i = Rows - 1; i >= 0; --i) {
        const auto& row = history_[static_cast<size_t>((head_ + i) % Rows)];
        const float depth = i == 0 ? 0.0f : static_cast<float>(i) + frac - 1.0f + 0.001f;
        const float p = 1.0f / (1.0f + depth * 0.16f);
        constexpr float pFar = 1.0f / (1.0f + (Rows - 1) * 0.16f);
        const float base = horizon + (H + 6.0f - horizon) * (p - pFar) / (1.0f - pFar);
        const float halfWidth = W * 0.85f * p + 10.0f;
        const float amp = 60.0f * p;
        const float fade = std::clamp(1.25f - depth / Rows, 0.0f, 1.0f);
        const float haze = 1.0f - p;

        for (int x = 0; x < W; ++x) {
            const float u = ((x + 0.5f) - W * 0.5f) / halfWidth;  // -1..1 across the row
            const float cf = (u * 0.5f + 0.5f) * (Cols - 1);
            float v = 0.0f;
            if (cf >= 0 && cf <= Cols - 1) {
                const int c0 = static_cast<int>(cf);
                const int c1 = std::min(c0 + 1, Cols - 1);
                const float f = cf - c0;
                v = row[static_cast<size_t>(c0)] * (1 - f) + row[static_cast<size_t>(c1)] * f;
            }
            // A little permanent terrain keeps silent passages from going flat.
            const float terrain = 0.035f * (std::sin(u * 7.0f + i * 0.7f) + 1.0f) * std::exp(-u * u * 2.0f);
            lineY[static_cast<size_t>(x)] = base - (v + terrain) * amp;
        }

        const RGB ground = lerp(RGB{3, 1, 7}, skyLow * 0.22f, haze * haze);
        for (int x = 0; x < W; ++x) {
            const float ly = lineY[static_cast<size_t>(x)];
            const float ny = lineY[static_cast<size_t>(std::min(x + 1, W - 1))];
            const float py = lineY[static_cast<size_t>(std::max(x - 1, 0))];
            // Occlude everything behind this ridge.
            for (int y = std::max(0, static_cast<int>(std::ceil(ly))); y < std::min(H, static_cast<int>(base) + 2); ++y)
                buf_.at(x, y) = ground;
            const float height = std::clamp((base - ly) / std::max(1.0f, amp), 0.0f, 1.0f);
            RGB line = lerp(hsv(hue_ - 40.0f + depth * 2.5f, 0.85f, 0.9f), RGB{255, 235, 245}, height * height * 0.9f);
            line = line * (fade * (0.55f + 0.45f * p));
            if (i == 0)
                line = line * (1.0f + beat_.v * 0.6f);
            line = lerp(line, skyLow * 0.6f, std::pow(haze, 1.5f) * 0.8f);
            // Vertical span to keep steep slopes continuous, AA at the ends.
            const float top = std::min({ly, (ly + ny) * 0.5f, (ly + py) * 0.5f});
            const float bot = std::max({ly, (ly + ny) * 0.5f, (ly + py) * 0.5f});
            for (int y = static_cast<int>(std::floor(top)); y <= static_cast<int>(std::floor(bot)); ++y) {
                buf_.set(x, y, line);
            }
        }
    }

    buf_.blit(canvas);
    return true;
}

SceneDescriptor AudioRidgelineScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "ridgeline";
    d.tags = {"music", "landscape", "spectrum", "audio-reactive", "calm", "music-showcase", "style:edm", "style:party", "drop-reactive"};
    d.intensity = 0.55f;
    d.motion = 0.6f;
    d.music_affinity = 1.0f;
    d.performance_cost = 0.3f;
    return d;
}
}  // namespace Scenes
