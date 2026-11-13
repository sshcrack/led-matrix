#include "HalftoneScene.h"

#include <algorithm>
#include <cmath>

using namespace Scenes;
using namespace Scenes::ArtworkPaint;

SpotifyArtworkScene::ArtworkOptions HalftoneScene::artwork_options() const
{
    return {matrix_width, matrix_height, std::max(matrix_width, matrix_height), 16};
}

void HalftoneScene::sample_cells(const Artwork& art, std::vector<Cell>& out, int W, int H, int cell, int cols, int rows, int x0,
                                 int y0) const
{
    out.assign(static_cast<size_t>(cols * rows), {});
    const int S = art.cover_size;
    const int ox = (S - W) / 2, oy = (S - H) / 2;
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c) {
            Rgb sum{};
            int n = 0;
            for (int y = y0 + r * cell; y < y0 + (r + 1) * cell; ++y)
                for (int x = x0 + c * cell; x < x0 + (c + 1) * cell; ++x) {
                    const int sx = std::clamp(x + ox, 0, S - 1), sy = std::clamp(y + oy, 0, S - 1);
                    sum = sum + art.cover[static_cast<size_t>(sy * S + sx)];
                    ++n;
                }
            const Rgb avg = sum * (1.0f / static_cast<float>(std::max(1, n)));
            const float value = std::max({avg.r, avg.g, avg.b}) / 255.0f;
            // Dot size carries the brightness, so the ink itself is printed vivid.
            const Rgb ink = mix(avg, avg * (1.0f / std::max(value, 0.18f)), 0.65f);
            out[static_cast<size_t>(r * cols + c)] = {ink, value};
        }
}

void HalftoneScene::draw(rgb_matrix::FrameCanvas* canvas, float dt)
{
    const int W = canvas->width(), H = canvas->height();
    const auto& art = current();
    const Artwork* prev = previous();
    const int cell = std::clamp(dot_size->get(), 3, 12);
    const int cols = (W + cell - 1) / cell, rows = (H + cell - 1) / cell;
    const int x0 = (W - cols * cell) / 2, y0 = (H - rows * cell) / 2;  // centre the grid
    sample_cells(art, cells_, W, H, cell, cols, rows, x0, y0);
    if (prev)
        sample_cells(*prev, prev_cells_, W, H, cell, cols, rows, x0, y0);

    // Rings from the centre on beats.
    const float max_d = 0.5f * std::sqrt(static_cast<float>(W * W + H * H));
    if (beat_ && ripple_strength->get() > 0.0f) {
        ripples_.push_back(0.0f);
        if (ripples_.size() > 4)
            ripples_.erase(ripples_.begin());
    }
    for (auto& age : ripples_) age += dt;
    std::erase_if(ripples_, [](float age) { return age > 2.5f; });
    const float speed = max_d / 1.1f;
    const float ring_w = cell * 1.6f;

    const float cxs = W * 0.5f, cys = H * 0.5f;
    const float breathe = breathing->get();
    const float ripple_gain = ripple_strength->get();
    const float glow = background_glow->get();
    const bool square = square_dots->get();
    const float trans = transition();
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c) {
            const float ccx = x0 + (c + 0.5f) * cell, ccy = y0 + (r + 0.5f) * cell;
            const float dx = ccx - cxs, dy = ccy - cys;
            const float d = std::sqrt(dx * dx + dy * dy);
            float ripple = 0.0f;
            for (const float age : ripples_) {
                const float off = (d - age * speed) / ring_w;
                ripple += std::exp(-off * off) * std::exp(-age / 0.8f);
            }
            ripple *= ripple_gain;
            const float breath = 0.10f * breathe * std::sin(time_ * 0.9f - d * 0.09f) * (0.6f + 0.6f * level_);

            Cell cl = cells_[static_cast<size_t>(r * cols + c)];
            float grow = 1.0f;
            if (prev) {
                // Re-print outwards: old dots shrink away, new dots grow in.
                const float t = std::clamp(trans * 1.8f - d / max_d * 0.8f, 0.0f, 1.0f);
                if (t < 0.5f) {
                    cl = prev_cells_[static_cast<size_t>(r * cols + c)];
                    grow = 1.0f - 2.0f * t;
                }
                else {
                    grow = 2.0f * t - 1.0f;
                }
                grow = smooth01(grow);
            }
            const float base = 0.22f + 0.78f * std::sqrt(cl.value);
            const float radius = std::min(cell * 0.46f * base * (1.0f + breath + 0.45f * ripple) * grow, cell * 0.72f);
            const Rgb ink = cl.color * (1.0f + 0.35f * ripple);
            const Rgb bg = cl.color * (glow * (0.6f + 0.4f * level_));

            const int px0 = static_cast<int>(ccx - cell * 0.5f), py0 = static_cast<int>(ccy - cell * 0.5f);
            for (int y = std::max(0, py0); y < std::min(H, py0 + cell); ++y)
                for (int x = std::max(0, px0); x < std::min(W, px0 + cell); ++x) {
                    const float qx = std::fabs(x + 0.5f - ccx), qy = std::fabs(y + 0.5f - ccy);
                    const float dist = square ? std::max(qx, qy) : std::sqrt(qx * qx + qy * qy);
                    const float cov = std::clamp(radius - dist + 0.5f, 0.0f, 1.0f);
                    frame_[static_cast<size_t>(y * W + x)] = mix(bg, ink, cov);
                }
        }
    present(canvas);
}

void HalftoneScene::register_properties()
{
    add_property(dot_size);
    add_property(ripple_strength);
    add_property(breathing);
    add_property(background_glow);
    add_property(square_dots);
    add_property(scene_weight_if_new_song);
}

SceneDescriptor HalftoneScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "album-art";
    d.tags = {"music", "media", "album-art", "spotify", "audio-reactive", "pixel-art", "graphic"};
    d.intensity = 0.42f;
    d.motion = 0.40f;
    d.music_affinity = 1.0f;
    d.performance_cost = 0.22f;
    d.variants = {
        {"fine",
         "Fine print",
         "Small dots that keep the artwork recognisable, soft beat rings",
         {{"dot_size", 4}, {"ripple_strength", 0.35f}, {"background_glow", 0.08f}},
         {"album-art", "calm"},
         0.34f,
         0.30f,
         1.0f,
         0.24f},
        {"tiles",
         "Pixel tiles",
         "Large square tiles that swell with every beat",
         {{"dot_size", 9}, {"square_dots", true}, {"ripple_strength", 0.8f}, {"breathing", 0.6f}},
         {"pixel-art", "energetic"},
         0.55f,
         0.52f,
         1.0f,
         0.22f},
    };
    return d;
}

std::unique_ptr<Scene> HalftoneSceneWrapper::create()
{
    return std::make_unique<HalftoneScene>();
}
