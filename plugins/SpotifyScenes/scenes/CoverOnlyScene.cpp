#include "CoverOnlyScene.h"

#include <algorithm>
#include <array>
#include <cmath>

using namespace Scenes;
using namespace Scenes::ArtworkPaint;

SpotifyArtworkScene::ArtworkOptions CoverOnlyScene::artwork_options() const
{
    const int cover_size =
        std::clamp(std::min(matrix_width, matrix_height) * cover_size_percent->get() / 100, 8, std::min(matrix_width, matrix_height));
    return {matrix_width, matrix_height, cover_size, std::clamp(28 - background_blur->get(), 6, 28)};
}

void CoverOnlyScene::initialize(int width, int height)
{
    SpotifyArtworkScene::initialize(width, height);
    // Per-pixel backdrop is too much for the Pi next to the artwork worker;
    // start on the (visually equivalent) coarse grid there.
    set_render_quality_hint(0.75f);
}

void CoverOnlyScene::draw(rgb_matrix::FrameCanvas *canvas, float dt)
{
    const int W = canvas->width(), H = canvas->height();
    const auto& art = current();
    const Artwork* prev = previous();
    const float fade = smooth01(transition());

    const float pulse_strength = beat_pulse_strength->get();
    const int bar_h = show_progress->get() ? std::clamp(progress_bar_height->get(), 1, std::max(1, H / 8)) : 0;
    const float cx = W * 0.5f, cy = H * 0.5f - bar_h * 0.5f;

    // --- cover geometry -------------------------------------------------------
    const int size = art.cover_size;
    const float land = prev ? 0.82f + 0.18f * ease_out_back(transition() * 1.25f) : 1.0f;
    const float scale = land * (1.0f + pulse_ * pulse_strength * 0.10f);
    const float half = size * scale * 0.5f;

    // --- living backdrop: a slowly flowing, blurred copy of the artwork -------
    // The field is smooth, so at reduced quality (the Pi) it is sampled on a
    // coarse grid and interpolated; at full quality (desktop worker, previews)
    // every pixel is sampled. Pixels the opaque cover hides are skipped.
    {
        const float quality = render_quality_scale();
        const int Step = quality >= 0.95f ? 1 : quality >= 0.8f ? 2 : 4;
        const int gw = W / Step + 2, gh = H / Step + 2;
        grid_.resize(static_cast<size_t>(gw * gh));
        const float motion = background_motion->get();
        const float a = time_ * 0.12f * motion;
        const float rot = std::sin(a * 0.35f) * 0.35f, zoom = 0.72f + 0.06f * std::sin(a * 0.5f);
        const float cr = std::cos(rot) * zoom, sr = std::sin(rot) * zoom;
        const float brightness = background_brightness->get() / 100.0f *
            (0.80f + 0.40f * level_ + 0.6f * pulse_ * pulse_strength);
        for (int gy = 0; gy < gh; ++gy) {
            const float v = static_cast<float>(gy * Step) / H - 0.5f;
            const float wave_x = 0.07f * std::sin((gy * Step) * 5.0f / H + a * 1.3f);
            for (int gx = 0; gx < gw; ++gx) {
                const float u = static_cast<float>(gx * Step) / W - 0.5f;
                const float wave_y = 0.07f * std::sin((gx * Step) * 4.3f / W - a * 1.1f + 1.7f);
                const float su = 0.5f + u * cr - v * sr + wave_x;
                const float sv = 0.5f + u * sr + v * cr + wave_y;
                Rgb c = sample_bilinear(art.backdrop, art.backdrop_size, su, sv);
                if (prev && fade < 1.0f)
                    c = mix(sample_bilinear(prev->backdrop, prev->backdrop_size, su, sv), c, fade);
                const float vignette = std::max(0.25f, 1.0f - 1.1f * (u * u + v * v));
                grid_[static_cast<size_t>(gy * gw + gx)] = c * (brightness * vignette);
            }
        }
        // Opaque cover rectangle (only when no fade-in/out blends through it).
        int hx0 = W, hx1 = -1, hy0 = H, hy1 = -1;
        if (!prev) {
            hx0 = static_cast<int>(std::ceil(cx - half)) + 1;
            hx1 = static_cast<int>(std::floor(cx + half)) - 2;
            hy0 = static_cast<int>(std::ceil(cy - half)) + 1;
            hy1 = static_cast<int>(std::floor(cy + half)) - 2;
        }
        const float inv = 1.0f / static_cast<float>(Step);
        for (int y = 0; y < H; ++y) {
            const int gy = y / Step;
            const float fy = (y - gy * Step) * inv;
            const Rgb *r0 = &grid_[static_cast<size_t>(gy * gw)];
            const Rgb *r1 = r0 + gw;
            const bool row_hidden = y >= hy0 && y <= hy1;
            for (int x = 0; x < W; ++x) {
                if (row_hidden && x >= hx0 && x <= hx1) {
                    x = hx1;
                    continue;
                }
                const int gx = x / Step;
                const float fx = (x - gx * Step) * inv;
                frame_[static_cast<size_t>(y * W + x)] = mix(mix(r0[gx], r0[gx + 1], fx), mix(r1[gx], r1[gx + 1], fx), fy);
            }
        }
    }

    // --- spectrum aura around the cover, in the cover's own colours -----------
    if (spectrum_aura->get() || cover_border_glow_intensity->get() > 0.0f) {
        constexpr int Segments = 48;
        if (aura_.size() != Segments) {
            aura_.assign(Segments, 0.0f);
            aura_peak_.assign(Segments, 0.1f);
        }
        for (int i = 0; i < Segments; ++i) {
            // Mirror left/right; bass at the bottom, treble at the top.
            const float p = static_cast<float>(i) / Segments;
            const float m = 1.0f - std::fabs(p * 2.0f - 1.0f);  // 0 bottom .. 1 top
            float target = 0.0f;
            if (spectrum_aura->get()) {
                if (live_ && !spectrum_.empty()) {
                    // Skip the (often empty) lowest bins; normalise each segment
                    // against its own slowly decaying peak so every part moves.
                    const auto bin = std::min(spectrum_.size() - 1, static_cast<size_t>((0.03f + m * 0.75f) * spectrum_.size()));
                    const float v = spectrum_[bin];
                    auto &pk = aura_peak_[static_cast<size_t>(i)];
                    pk = std::max({v, pk * std::exp(-dt / 5.0f), 0.05f});
                    target = std::pow(std::clamp(v / pk, 0.0f, 1.0f), 1.6f);
                }
                else {
                    target = 0.25f + 0.25f * std::sin(time_ * 1.3f + m * 6.0f) * std::sin(time_ * 0.7f + i) + 0.4f * pulse_;
                }
            }
            auto &a = aura_[static_cast<size_t>(i)];
            a += (target - a) * (1.0f - std::exp(-dt / (target > a ? 0.05f : 0.25f)));
        }
        // Soften neighbouring segments so the aura reads as one glowing outline.
        std::array<float, Segments> soft{};
        for (int i = 0; i < Segments; ++i)
            soft[static_cast<size_t>(i)] = 0.25f * aura_[static_cast<size_t>((i + Segments - 1) % Segments)] +
                                           0.5f * aura_[static_cast<size_t>(i)] + 0.25f * aura_[static_cast<size_t>((i + 1) % Segments)];
        const float base = cover_border_glow_intensity->get() * 0.6f;
        const float reach = 3.0f + 9.0f;
        const int x0 = std::max(0, static_cast<int>(cx - half - reach)), x1 = std::min(W - 1, static_cast<int>(cx + half + reach));
        const int y0 = std::max(0, static_cast<int>(cy - half - reach)), y1 = std::min(H - 1, static_cast<int>(cy + half + reach));
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x) {
                const float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
                // Distance to the cover square (rounded corners).
                const float qx = std::max(std::fabs(dx) - half, 0.0f), qy = std::max(std::fabs(dy) - half, 0.0f);
                const float d = std::hypot(qx, qy);
                if (d <= 0.0f || d > reach) continue;
                // Angle around the cover, 0 at the bottom centre.
                const float ang = std::atan2(dx, dy);
                const float p = (ang < 0 ? ang + 2 * Pi : ang) / (2 * Pi);
                const float fs = p * Segments;
                const auto seg = static_cast<size_t>(fs) % Segments;
                const float len = soft[seg] + (soft[(seg + 1) % Segments] - soft[seg]) * (fs - std::floor(fs));
                const float glow = (base + len * 0.9f) * std::exp(-d / (0.8f + len * 4.0f));
                const float m = 1.0f - std::fabs(p * 2.0f - 1.0f);
                Rgb col = sample_accent(art, m * 0.999f + time_ * 0.02f);
                if (prev && fade < 1.0f)
                    col = mix(sample_accent(*prev, m * 0.999f), col, fade);
                auto &px = frame_[static_cast<size_t>(y * W + x)];
                px = px + col * (glow * 0.85f);
            }
    }

    // --- sparkles thrown off the cover edge on beats ---------------------------
    if (beat_sparkles->get() && beat_) {
        const int count = 3 + static_cast<int>(level_ * 6.0f);
        for (int i = 0; i < count; ++i) {
            const float t = random01() * 4.0f;
            const int side = static_cast<int>(t);
            const float f = t - side;
            float sx = cx, sy = cy, nx = 0, ny = 0;
            if (side == 0) { sx += (f - 0.5f) * 2 * half; sy -= half; ny = -1; }
            else if (side == 1) { sx += half; sy += (f - 0.5f) * 2 * half; nx = 1; }
            else if (side == 2) { sx += (f - 0.5f) * 2 * half; sy += half; ny = 1; }
            else { sx -= half; sy += (f - 0.5f) * 2 * half; nx = -1; }
            const float speed = 8.0f + random01() * 14.0f;
            sparkles_.push_back({sx, sy, nx * speed + (random01() - 0.5f) * 6.0f, ny * speed + (random01() - 0.5f) * 6.0f,
                                 0.0f, 0.7f + random01() * 0.9f, static_cast<int>(random01() * art.accent.size()) % static_cast<int>(art.accent.size())});
        }
    }
    for (auto &s : sparkles_) {
        s.life += dt;
        s.x += s.vx * dt;
        s.y += s.vy * dt;
        s.vx *= std::exp(-dt * 1.2f);
        s.vy *= std::exp(-dt * 1.2f);
    }
    std::erase_if(sparkles_, [](const Sparkle &s) { return s.life >= s.max_life; });
    if (sparkles_.size() > 160) sparkles_.erase(sparkles_.begin(), sparkles_.begin() + static_cast<long>(sparkles_.size() - 160));
    for (const auto &s : sparkles_) {
        const int x = static_cast<int>(s.x), y = static_cast<int>(s.y);
        if (x < 0 || y < 0 || x >= W || y >= H) continue;
        const float a = 1.0f - s.life / s.max_life;
        auto &px = frame_[static_cast<size_t>(y * W + x)];
        px = px + art.accent[static_cast<size_t>(s.color)] * (a * 0.9f);
    }

    // --- covers: the previous one sinks away, the new one lands -----------------
    const auto draw_cover = [&](const Artwork &c, float s, float alpha) {
        if (alpha <= 0.01f || c.cover.empty()) return;
        const float h = c.cover_size * s * 0.5f;
        const int x0 = static_cast<int>(std::floor(cx - h)), x1 = static_cast<int>(std::ceil(cx + h));
        const int y0 = static_cast<int>(std::floor(cy - h)), y1 = static_cast<int>(std::ceil(cy + h));
        // Two pixels of soft shadow keep the edge readable on bright artwork.
        for (int y = y0 - 1; y <= y1 + 1; ++y)
            for (int x = x0 - 1; x <= x1 + 1; ++x) {
                if (x < 0 || y < 0 || x >= W || y >= H) continue;
                auto &px = frame_[static_cast<size_t>(y * W + x)];
                px = px * (1.0f - 0.55f * alpha);
            }
        const bool exact = std::fabs(s - 1.0f) < 0.004f;
        const int ix0 = static_cast<int>(std::round(cx - c.cover_size * 0.5f)), iy0 = static_cast<int>(std::round(cy - c.cover_size * 0.5f));
        for (int y = std::max(0, y0); y <= std::min(H - 1, y1); ++y)
            for (int x = std::max(0, x0); x <= std::min(W - 1, x1); ++x) {
                Rgb col;
                if (exact) {
                    // Pixel-exact copy: the artwork stays perfectly crisp at rest.
                    const int sx = x - ix0, sy = y - iy0;
                    if (sx < 0 || sy < 0 || sx >= c.cover_size || sy >= c.cover_size) continue;
                    col = c.cover[static_cast<size_t>(sy * c.cover_size + sx)];
                } else {
                    const float u = (x + 0.5f - (cx - h)) / (2 * h), v = (y + 0.5f - (cy - h)) / (2 * h);
                    if (u < 0 || v < 0 || u >= 1 || v >= 1) continue;
                    col = sample_bilinear(c.cover, c.cover_size, u, v);
                }
                auto &px = frame_[static_cast<size_t>(y * W + x)];
                px = alpha >= 0.999f ? col : mix(px, col, alpha);
            }
    };
    if (prev && fade < 1.0f)
        draw_cover(*prev, 1.0f - 0.3f * fade, 1.0f - fade);
    draw_cover(art, scale, prev ? std::min(1.0f, transition() * 3.0f) : 1.0f);

    // --- progress bar in the cover's palette ------------------------------------
    if (bar_h > 0) {
        const float track_progress = progress();
        const float filled = track_progress * W;
        for (int y = H - bar_h; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                auto &px = frame_[static_cast<size_t>(y * W + x)];
                if (x + 0.5f < filled) {
                    const Rgb col = sample_accent(art, 0.15f + 0.7f * static_cast<float>(x) / W);
                    const float head = std::exp(-(filled - x) / 2.5f) * (0.5f + 0.5f * pulse_);
                    px = col * ((y == H - bar_h ? 0.95f : 0.65f) + head * 0.6f);
                } else {
                    px = px * 0.25f + Rgb{4, 5, 8};
                }
            }
    }

    present(canvas);
}

void CoverOnlyScene::on_track_changed()
{
    sparkles_.clear();
}

string CoverOnlyScene::get_name() const
{
    return "spotify";
}

std::unique_ptr<Scene> CoverOnlySceneWrapper::create()
{
    return std::make_unique<CoverOnlyScene>();
}

void CoverOnlyScene::register_properties()
{
    add_property(cover_border_glow_intensity);
    add_property(cover_size_percent);
    add_property(background_blur);
    add_property(background_brightness);
    add_property(background_motion);
    add_property(progress_bar_height);
    add_property(beat_pulse_strength);
    add_property(show_progress);
    add_property(spectrum_aura);
    add_property(beat_sparkles);
    add_property(scene_weight_if_new_song);
}

Scenes::SceneDescriptor Scenes::CoverOnlyScene::get_descriptor() const {
    auto d = Scene::get_descriptor(); d.automatic_eligible = true;
    d.family = "album-art";
    d.tags = {"music", "media", "album-art", "spotify", "audio-reactive"};
    d.intensity = 0.40f; d.motion = 0.36f; d.music_affinity = 1.0f; d.performance_cost = 0.40f;
    d.variants = {
        {"focus",
         "Cover focus",
         "Large crisp artwork, calm backdrop and a thin aura",
         {{"cover_size_percent", 84},
          {"background_brightness", 34},
          {"background_motion", 0.6f},
          {"cover_border_glow_intensity", 0.20f},
          {"beat_pulse_strength", 0.15f},
          {"beat_sparkles", false},
          {"progress_bar_height", 2}},
         {"minimal", "album-art"},
         0.28f,
         0.20f,
         1.0f,
         0.36f},
        {"ambient",
         "Ambient cover",
         "Smaller artwork floating in a flowing colour field with a lively spectrum aura",
         {{"cover_size_percent", 62},
          {"background_blur", 14},
          {"background_brightness", 46},
          {"background_motion", 1.4f},
          {"cover_border_glow_intensity", 0.45f},
          {"beat_pulse_strength", 0.32f}},
         {"ambient", "glow", "spectrum"},
         0.48f,
         0.42f,
         1.0f,
         0.42f},
        {"poster",
         "Poster",
         "Edge-to-edge artwork with only a hairline glow and no progress bar",
         {{"cover_size_percent", 94},
          {"show_progress", false},
          {"cover_border_glow_intensity", 0.10f},
          {"spectrum_aura", false},
          {"beat_pulse_strength", 0.06f},
          {"beat_sparkles", false},
          {"background_brightness", 30},
          {"background_motion", 0.5f}},
         {"minimal", "album-art", "calm"},
         0.22f,
         0.12f,
         1.0f,
         0.30f},
    };
    return d;
}
