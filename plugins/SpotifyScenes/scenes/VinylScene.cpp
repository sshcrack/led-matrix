#include "VinylScene.h"

#include <algorithm>
#include <cmath>

using namespace Scenes;
using namespace Scenes::ArtworkPaint;

namespace {
constexpr float DiscFraction = 0.46f;  // disc radius relative to the shorter side
constexpr float LightAngle = -2.35f;   // fixed light from the upper left
constexpr float SpindleR = 0.035f;

float label_fraction(int percent)
{
    return std::clamp(percent, 30, 100) / 100.0f;
}
}  // namespace

SpotifyArtworkScene::ArtworkOptions VinylScene::artwork_options() const
{
    const float radius = std::min(matrix_width, matrix_height) * DiscFraction;
    const int size = std::max(8, static_cast<int>(std::round(2.0f * radius * label_fraction(label_percent->get()))));
    return {matrix_width, matrix_height, size, 20};
}

void VinylScene::build_geometry(int W, int H)
{
    disc_w_ = W;
    disc_h_ = H;
    radius_ = std::min(W, H) * DiscFraction;
    // Leave room on the right for the tonearm pivot on wide panels.
    cx_ = W > H ? W * 0.5f - std::min(radius_ * 0.25f, (W - H) * 0.5f) : W * 0.5f;
    cy_ = H * 0.5f;
    disc_.clear();
    inside_.assign(static_cast<size_t>(W * H), 0);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const float px = x + 0.5f - cx_, py = y + 0.5f - cy_;
            const float dist = std::sqrt(px * px + py * py);
            const float edge = std::clamp(radius_ - dist + 0.5f, 0.0f, 1.0f);
            if (edge <= 0.0f)
                continue;
            const float r = dist / radius_;
            // Grooves: a fine concentric relief with a few quiet gaps between "tracks".
            float groove = 0.5f + 0.5f * std::sin(dist * 2.6f + 2.0f * std::sin(r * 23.0f));
            for (const float gap : {0.58f, 0.71f, 0.84f}) groove *= std::clamp(std::fabs(r - gap) / 0.012f, 0.25f, 1.0f);
            // Two opposite light wedges that stay put while the record turns.
            const float theta = std::atan2(py, px);
            const float wedge = std::pow(std::fabs(std::cos(theta - LightAngle)), 10.0f);
            const float sheen = wedge * std::clamp((r - 0.15f) / 0.35f, 0.0f, 1.0f);
            disc_.push_back({y * W + x, px / radius_, py / radius_, r, groove, sheen, edge});
            if (edge >= 1.0f)
                inside_[static_cast<size_t>(y * W + x)] = 1;
        }
}

void VinylScene::draw(rgb_matrix::FrameCanvas* canvas, float dt)
{
    const int W = canvas->width(), H = canvas->height();
    if (W != disc_w_ || H != disc_h_)
        build_geometry(W, H);
    const auto& art = current();
    const Artwork* prev = previous();
    const float fade = smooth01(transition());

    // A new record spins up from a slow start.
    const float speed = rpm->get() / 60.0f * 2.0f * Pi * (0.2f + 0.8f * fade);
    angle_ = std::fmod(angle_ + speed * dt, 2.0f * Pi);
    const float ca = std::cos(angle_), sa = std::sin(angle_);

    // --- backdrop: a dim, slowly drifting wash of the cover colours ------------
    const float bright = background_brightness->get() / 100.0f * (0.85f + 0.3f * level_);
    const float drift = time_ * 0.05f;
    const float shadow_dx = 1.5f, shadow_dy = 2.5f;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const auto i = static_cast<size_t>(y * W + x);
            if (inside_[i])
                continue;
            const float u = static_cast<float>(x) / W, v = static_cast<float>(y) / H;
            const float su = 0.5f + (u - 0.5f) * 0.8f + 0.06f * std::sin(drift + v * 2.0f);
            const float sv = 0.5f + (v - 0.5f) * 0.8f + 0.06f * std::cos(drift * 0.8f + u * 2.0f);
            Rgb c = sample_bilinear(art.backdrop, art.backdrop_size, su, sv);
            if (prev && fade < 1.0f)
                c = mix(sample_bilinear(prev->backdrop, prev->backdrop_size, su, sv), c, fade);
            const float vu = u - 0.5f, vv = v - 0.5f;
            const float vignette = std::max(0.3f, 1.0f - 1.2f * (vu * vu + vv * vv));
            // Soft drop shadow of the record.
            const float sx = x + 0.5f - cx_ - shadow_dx, sy = y + 0.5f - cy_ - shadow_dy;
            const float out = std::sqrt(sx * sx + sy * sy) - radius_;
            const float shadow = out < 0.0f ? 0.35f : 1.0f - 0.65f * std::exp(-out / 2.5f);
            frame_[i] = c * (bright * vignette * shadow);
        }

    // --- the record --------------------------------------------------------------
    const float label = label_fraction(label_percent->get());
    const bool picture = label >= 0.99f;
    const bool smooth = render_quality_scale() >= 0.8f;
    const float sheen_gain = sheen_strength->get() * (0.45f + 0.35f * pulse_ + 0.2f * level_);
    const auto sample_art = [&](const Artwork& a, float dx, float dy) {
        const float u = 0.5f + dx * 0.5f / label, v = 0.5f + dy * 0.5f / label;
        return smooth ? sample_bilinear(a.cover, a.cover_size, u, v) : sample_nearest(a.cover, a.cover_size, u, v);
    };
    for (const auto& p : disc_) {
        // Rotate into record space: the artwork turns, light and grooves do not.
        const float rx = p.dx * ca + p.dy * sa, ry = -p.dx * sa + p.dy * ca;
        Rgb col;
        float relief = 0.0f;
        if (p.r < SpindleR) {
            col = {6, 6, 8};
        }
        else if (p.r < label) {
            col = sample_art(art, rx, ry);
            if (prev && fade < 1.0f)
                col = mix(sample_art(*prev, rx, ry), col, fade);
            if (picture) {
                // Picture disc: the print shows through the grooved clear vinyl.
                relief = p.r > 0.3f ? 1.0f : 0.0f;
                col = col * (0.84f + 0.16f * p.groove * relief);
                if (p.r > 0.965f)
                    col = col * 0.45f;
            }
            else if (p.r > label - 0.025f) {
                col = col * 0.55f;  // the label's pressed edge
            }
        }
        else {
            // Black vinyl: dead wax right after the label, grooves from there on.
            relief = std::clamp((p.r - label - 0.04f) / 0.03f, 0.0f, 1.0f);
            const float g = 0.35f + 0.65f * p.groove * relief;
            col = {13 + 9 * g, 13 + 9 * g, 16 + 10 * g};
            if (p.r > 0.975f)
                col = col + Rgb{10, 10, 12};
        }
        if (p.sheen > 0.0f && p.r >= SpindleR) {
            const float wave = picture ? 0.55f : 1.0f;
            const float s = p.sheen * sheen_gain * wave * (0.35f + 0.65f * (relief > 0 ? p.groove : 0.4f));
            const Rgb tint = mix(Rgb{235, 235, 242}, sample_accent(art, rx * 0.25f + 0.5f), 0.35f);
            col = col + tint * (s * 0.55f);
        }
        auto& px = frame_[static_cast<size_t>(p.index)];
        px = p.edge >= 1.0f ? col : mix(px, col, p.edge);
    }

    if (show_tonearm->get()) {
        const float inner = std::max(0.33f, picture ? 0.36f : label + 0.07f);
        draw_tonearm(W, H, 0.93f + (inner - 0.93f) * progress());
    }
    present(canvas);
}

void VinylScene::draw_tonearm(int W, int H, float needle_r)
{
    // The pivot sits off the upper-right of the record; the needle is where the
    // arm (fixed length) meets the requested groove radius.
    const float R = radius_;
    const float px = std::min(cx_ + R * 1.02f, W - 4.5f), py = std::max(cy_ - R * 0.95f, 4.5f);
    const float dxp = px - cx_, dyp = py - cy_;
    const float d = std::sqrt(dxp * dxp + dyp * dyp);
    const float arm = d * 0.8f;
    const float nr = std::clamp(needle_r * R, std::fabs(d - arm) + 0.5f, d + arm - 0.5f);
    // Circle-circle intersection; take the lower of the two solutions.
    const float a = (nr * nr - arm * arm + d * d) / (2.0f * d);
    const float h = std::sqrt(std::max(0.0f, nr * nr - a * a));
    const float mx = cx_ + a * dxp / d, my = cy_ + a * dyp / d;
    float nx = mx + h * dyp / d, ny = my - h * dxp / d;
    const float ox = mx - h * dyp / d, oy = my + h * dxp / d;
    if (oy > ny) {
        nx = ox;
        ny = oy;
    }

    const float ax = nx - px, ay = ny - py;
    const float len = std::sqrt(ax * ax + ay * ay);
    const float ux = ax / len, uy = ay / len;

    const auto segment = [&](float x0, float y0, float x1, float y1, float width, Rgb col, float alpha, float off) {
        const int bx0 = std::max(0, static_cast<int>(std::min(x0, x1) - width - 2 + off));
        const int bx1 = std::min(W - 1, static_cast<int>(std::max(x0, x1) + width + 2 + off));
        const int by0 = std::max(0, static_cast<int>(std::min(y0, y1) - width - 2 + off));
        const int by1 = std::min(H - 1, static_cast<int>(std::max(y0, y1) + width + 2 + off));
        const float sx = x1 - x0, sy = y1 - y0, sl = std::max(1e-4f, sx * sx + sy * sy);
        for (int y = by0; y <= by1; ++y)
            for (int x = bx0; x <= bx1; ++x) {
                const float qx = x + 0.5f - off - x0, qy = y + 0.5f - off - y0;
                const float t = std::clamp((qx * sx + qy * sy) / sl, 0.0f, 1.0f);
                const float ex = qx - sx * t, ey = qy - sy * t;
                const float cov = std::clamp(width * 0.5f - std::sqrt(ex * ex + ey * ey) + 0.5f, 0.0f, 1.0f) * alpha;
                if (cov <= 0.0f)
                    continue;
                auto& p = frame_[static_cast<size_t>(y * W + x)];
                p = mix(p, col, cov);
            }
    };
    const Rgb silver{178, 180, 190}, dark{40, 40, 46};
    const float head = std::min(5.0f, len * 0.18f);
    // Shadow first, then the arm, headshell, counterweight and pivot.
    segment(px, py, nx - ux * head, ny - uy * head, 1.8f, {0, 0, 0}, 0.45f, 1.5f);
    segment(px, py, nx - ux * head, ny - uy * head, 1.6f, silver, 1.0f, 0.0f);
    segment(nx - ux * head - uy * 1.2f, ny - uy * head + ux * 1.2f, nx, ny, 2.6f, {210, 212, 220}, 1.0f, 0.0f);
    segment(px - ux * 3.5f, py - uy * 3.5f, px - ux * 6.0f, py - uy * 6.0f, 3.6f, dark, 1.0f, 0.0f);
    segment(px, py, px, py, 6.5f, dark, 1.0f, 0.0f);
    segment(px, py, px, py, 3.0f, silver, 1.0f, 0.0f);
}

void VinylScene::register_properties()
{
    add_property(label_percent);
    add_property(rpm);
    add_property(sheen_strength);
    add_property(background_brightness);
    add_property(show_tonearm);
    add_property(scene_weight_if_new_song);
}

SceneDescriptor VinylScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "album-art";
    d.tags = {"music", "media", "album-art", "spotify", "audio-reactive", "retro", "calm"};
    d.intensity = 0.30f;
    d.motion = 0.30f;
    d.music_affinity = 1.0f;
    d.performance_cost = 0.30f;
    d.variants = {
        {"classic",
         "Black vinyl",
         "Black record with the album art as its centre label and bright groove reflections",
         {{"label_percent", 52}, {"sheen_strength", 0.7f}, {"rpm", 8.0f}},
         {"retro", "album-art"},
         0.30f,
         0.30f,
         1.0f,
         0.28f},
        {"slow_spin",
         "Slow spin",
         "Picture disc turning very slowly, without a tonearm",
         {{"rpm", 2.0f}, {"show_tonearm", false}, {"sheen_strength", 0.35f}, {"background_brightness", 22}},
         {"calm", "minimal", "album-art"},
         0.20f,
         0.14f,
         1.0f,
         0.28f},
    };
    return d;
}

std::unique_ptr<Scene> VinylSceneWrapper::create()
{
    return std::make_unique<VinylScene>();
}
