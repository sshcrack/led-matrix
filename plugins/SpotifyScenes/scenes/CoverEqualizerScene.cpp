#include "CoverEqualizerScene.h"

#include <algorithm>
#include <cmath>

using namespace Scenes;
using namespace Scenes::ArtworkPaint;

SpotifyArtworkScene::ArtworkOptions CoverEqualizerScene::artwork_options() const
{
    // Full-bleed: a square cover as large as the longer side, centre-cropped.
    return {matrix_width, matrix_height, std::max(matrix_width, matrix_height), 16};
}

void CoverEqualizerScene::draw(rgb_matrix::FrameCanvas* canvas, float dt)
{
    const int W = canvas->width(), H = canvas->height();
    const auto& art = current();
    const Artwork* prev = previous();
    const float fade = smooth01(transition());
    const int N = std::clamp(bands->get(), 6, std::max(6, W / 2));
    if (static_cast<int>(level_bands_.size()) != N) {
        level_bands_.assign(static_cast<size_t>(N), 0.0f);
        band_peak_.assign(static_cast<size_t>(N), 0.1f);
        cap_.assign(static_cast<size_t>(N), 0.0f);
        cap_hold_.assign(static_cast<size_t>(N), 0.0f);
    }

    // --- band levels ---------------------------------------------------------------
    const float resp = responsiveness->get();
    for (int b = 0; b < N; ++b) {
        const auto i = static_cast<size_t>(b);
        const float pos = N > 1 ? static_cast<float>(b) / (N - 1) : 0.0f;
        float target;
        if (live_ && !spectrum_.empty()) {
            const size_t S = spectrum_.size();
            const auto s0 = std::min(S - 1, static_cast<size_t>((0.02f + 0.80f * b / N) * S));
            const auto s1 = std::clamp(static_cast<size_t>((0.02f + 0.80f * (b + 1) / N) * S), s0 + 1, S);
            float v = 0.0f;
            for (size_t s = s0; s < s1; ++s) v += spectrum_[s];
            v /= static_cast<float>(s1 - s0);
            auto& pk = band_peak_[i];
            pk = std::max({v, pk * std::exp(-dt / 4.0f), 0.04f});
            target = std::pow(std::clamp(v / pk, 0.0f, 1.0f), 1.3f) * (0.55f + 0.45f * std::clamp(level_ * 1.4f, 0.0f, 1.0f));
        }
        else {
            // Simulated: bass bands kick on the beat, the rest wander.
            const float wander = 0.30f + 0.20f * std::sin(time_ * 1.9f + b * 0.8f) * std::sin(time_ * 0.55f + b * 1.7f) +
                                 0.06f * std::sin(time_ * 7.3f + b * 2.9f);
            const float kick = pulse_ * (0.55f * (1.0f - pos) * (1.0f - pos) + 0.12f);
            target = std::clamp((wander + kick) * (0.75f + 0.45f * level_), 0.04f, 0.97f);
        }
        auto& l = level_bands_[i];
        const float tau = (target > l ? 0.035f : 0.20f) / resp;
        l += (target - l) * (1.0f - std::exp(-dt / tau));
        if (l >= cap_[i]) {
            cap_[i] = l;
            cap_hold_[i] = 0.35f;
        }
        else if ((cap_hold_[i] -= dt) <= 0.0f) {
            cap_[i] = std::max(l, cap_[i] - dt * 0.55f);
        }
    }

    // --- pixels --------------------------------------------------------------------
    const auto cover_at = [&](const Artwork& a, int x, int y) {
        const int sx = std::clamp(x + (a.cover_size - W) / 2, 0, a.cover_size - 1);
        const int sy = std::clamp(y + (a.cover_size - H) / 2, 0, a.cover_size - 1);
        return a.cover[static_cast<size_t>(sy * a.cover_size + sx)];
    };
    const float dim = dim_level->get();
    const int gap = std::clamp(band_gap->get(), 0, 3);
    const bool mirrored = mirror->get();
    const bool caps = peak_caps->get();
    const float boost = 1.0f + 0.18f * pulse_;
    for (int x = 0; x < W; ++x) {
        const int b = std::min(N - 1, x * N / W);
        const int band_end = (b + 1) * W / N;  // first column of the next band
        const bool is_gap = gap > 0 && x >= band_end - gap && b < N - 1;
        const float h = level_bands_[static_cast<size_t>(b)] * (mirrored ? H * 0.5f : static_cast<float>(H));
        const float cap_h = cap_[static_cast<size_t>(b)] * (mirrored ? H * 0.5f : static_cast<float>(H));
        const Rgb accent = sample_accent(art, 0.1f + 0.8f * static_cast<float>(b) / N + time_ * 0.01f);
        for (int y = 0; y < H; ++y) {
            Rgb c = cover_at(art, x, y);
            if (prev && fade < 1.0f)
                c = mix(cover_at(*prev, x, y), c, fade);
            // Height of this pixel above the bar base, in pixels.
            const float above = mirrored ? std::fabs(y + 0.5f - H * 0.5f) : static_cast<float>(H - y) - 0.5f;
            const float lit = is_gap ? 0.0f : std::clamp(h - above + 0.5f, 0.0f, 1.0f);
            const float grey = luma(c) * 255.0f;
            const Rgb dimmed = mix(c, Rgb{grey, grey, grey}, 0.45f) * (dim * (is_gap ? 0.6f : 1.0f));
            Rgb out = mix(dimmed, c * boost, lit);
            if (!is_gap && lit > 0.0f && h - above < 2.0f)
                out = out + accent * (0.30f * lit);  // the bar's bright top edge
            if (caps && !is_gap && cap_h > 1.5f && std::fabs(above - (cap_h + 1.0f)) < 0.5f)
                out = mix(out, accent, 0.9f);
            frame_[static_cast<size_t>(y * W + x)] = out;
        }
    }
    present(canvas);
}

void CoverEqualizerScene::register_properties()
{
    add_property(bands);
    add_property(dim_level);
    add_property(mirror);
    add_property(peak_caps);
    add_property(band_gap);
    add_property(responsiveness);
    add_property(scene_weight_if_new_song);
}

SceneDescriptor CoverEqualizerScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "album-art";
    d.tags = {"music", "media", "album-art", "spotify", "audio-reactive", "spectrum", "energetic"};
    d.intensity = 0.62f;
    d.motion = 0.62f;
    d.music_affinity = 1.0f;
    d.performance_cost = 0.25f;
    d.variants = {
        {"mirror",
         "Mirrored",
         "Bands open from the middle of the cover in both directions",
         {{"mirror", true}, {"bands", 20}, {"dim_level", 0.24f}},
         {"spectrum", "energetic"},
         0.66f,
         0.66f,
         1.0f,
         0.25f},
        {"wide",
         "Wide bands",
         "Eight broad bands over a brighter cover, gentle and slow",
         {{"bands", 8}, {"dim_level", 0.40f}, {"band_gap", 2}, {"responsiveness", 0.6f}, {"peak_caps", false}},
         {"calm", "album-art"},
         0.40f,
         0.38f,
         1.0f,
         0.24f},
    };
    return d;
}

std::unique_ptr<Scene> CoverEqualizerSceneWrapper::create()
{
    return std::make_unique<CoverEqualizerScene>();
}
