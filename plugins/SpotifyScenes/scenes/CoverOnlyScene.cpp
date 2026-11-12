#ifdef _WIN32
#include "shared/common/win_compat.h"
#endif
#include "CoverOnlyScene.h"
#include "Magick++.h"
#include "spdlog/spdlog.h"
#include "matrix/manager/shared_spotify.h"
#include "shared/matrix/utils/canvas_image.h"
#include "shared/matrix/utils/image_fetch.h"
#include "shared/matrix/utils/rgb_blit.h"
#include "shared/matrix/audio_state.h"
#include "shared/matrix/media_artwork_state.h"
#include "led-matrix.h"
#include <cmath>
#include <array>
#include <chrono>
#include <exception>
#include <shared_mutex>
#include <vector>
#include <numeric>

#include "matrix/manager/song_bpm_getter.h"
#include "shared/matrix/input_ids.h"
#include "shared/matrix/runtime_inputs.h"

using namespace spdlog;
using namespace std;
using namespace Scenes;

namespace {
struct PaletteBin {
    uint32_t count = 0;
    uint32_t r = 0;
    uint32_t g = 0;
    uint32_t b = 0;
    float saturation_sum = 0.0f;
};

float color_hue(const rgb_matrix::Color &color)
{
    const float r = color.r / 255.0f;
    const float g = color.g / 255.0f;
    const float b = color.b / 255.0f;
    const float hi = std::max({r, g, b});
    const float lo = std::min({r, g, b});
    const float delta = hi - lo;
    if (delta < 0.0001f) return 0.0f;
    float hue = 0.0f;
    if (hi == r) hue = std::fmod((g - b) / delta, 6.0f);
    else if (hi == g) hue = (b - r) / delta + 2.0f;
    else hue = (r - g) / delta + 4.0f;
    hue /= 6.0f;
    if (hue < 0.0f) hue += 1.0f;
    return hue;
}

MediaArtworkState::Palette extract_artwork_palette(const Magick::Image &image)
{
    MediaArtworkState::Palette fallback{
        rgb_matrix::Color(44, 90, 210), rgb_matrix::Color(35, 190, 210),
        rgb_matrix::Color(95, 210, 115), rgb_matrix::Color(235, 170, 65),
        rgb_matrix::Color(215, 70, 155)};
    if (image.columns() == 0 || image.rows() == 0) return fallback;

    constexpr int QuantizedLevels = 16;
    constexpr int BinCount = QuantizedLevels * QuantizedLevels * QuantizedLevels;
    std::array<PaletteBin, BinCount> bins{};
    const auto *pixels = image.getConstPixels(0, 0, image.columns(), image.rows());
    if (!pixels) return fallback;

    const size_t step_x = std::max<size_t>(1, image.columns() / 32);
    const size_t step_y = std::max<size_t>(1, image.rows() / 32);
    for (size_t y = 0; y < image.rows(); y += step_y) {
        for (size_t x = 0; x < image.columns(); x += step_x) {
            const auto &pixel = pixels[y * image.columns() + x];
            if (pixel.opacity == MaxRGB) continue;
            const uint8_t r = ScaleQuantumToChar(pixel.red);
            const uint8_t g = ScaleQuantumToChar(pixel.green);
            const uint8_t b = ScaleQuantumToChar(pixel.blue);
            const uint8_t hi = std::max({r, g, b});
            const uint8_t lo = std::min({r, g, b});
            const float luma = (0.2126f * r + 0.7152f * g + 0.0722f * b) / 255.0f;
            const float saturation = hi > 0 ? static_cast<float>(hi - lo) / hi : 0.0f;
            if (luma < 0.035f || luma > 0.97f) continue;

            const int index = ((r >> 4) << 8) | ((g >> 4) << 4) | (b >> 4);
            auto &bin = bins[static_cast<size_t>(index)];
            ++bin.count;
            bin.r += r; bin.g += g; bin.b += b;
            bin.saturation_sum += saturation;
        }
    }

    std::array<int, BinCount> order{};
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](int lhs, int rhs) {
        const auto score = [&](int index) {
            const auto &bin = bins[static_cast<size_t>(index)];
            if (bin.count == 0) return 0.0f;
            const float saturation = bin.saturation_sum / bin.count;
            return bin.count * (0.58f + saturation * 1.35f);
        };
        return score(lhs) > score(rhs);
    });

    std::vector<rgb_matrix::Color> selected;
    selected.reserve(MediaArtworkState::PaletteSize);
    for (const int index : order) {
        const auto &bin = bins[static_cast<size_t>(index)];
        if (bin.count == 0) break;
        const rgb_matrix::Color candidate(
            static_cast<uint8_t>(bin.r / bin.count),
            static_cast<uint8_t>(bin.g / bin.count),
            static_cast<uint8_t>(bin.b / bin.count));
        bool distinct = true;
        for (const auto &existing : selected) {
            const int dr = static_cast<int>(candidate.r) - existing.r;
            const int dg = static_cast<int>(candidate.g) - existing.g;
            const int db = static_cast<int>(candidate.b) - existing.b;
            if (dr * dr + dg * dg + db * db < 34 * 34) { distinct = false; break; }
        }
        if (!distinct) continue;
        selected.push_back(candidate);
        if (selected.size() == MediaArtworkState::PaletteSize) break;
    }

    if (selected.empty()) return fallback;
    const size_t discovered = selected.size();
    while (selected.size() < MediaArtworkState::PaletteSize)
        selected.push_back(selected[selected.size() % discovered]);
    std::sort(selected.begin(), selected.end(), [](const auto &lhs, const auto &rhs) {
        return color_hue(lhs) < color_hue(rhs);
    });
    MediaArtworkState::Palette result{};
    std::copy_n(selected.begin(), MediaArtworkState::PaletteSize, result.begin());
    return result;
}
}


namespace {
using Rgb = CoverOnlyScene::Rgb;

constexpr float Pi = 3.14159265358979323846f;

Rgb operator+(Rgb a, Rgb b) { return {a.r + b.r, a.g + b.g, a.b + b.b}; }
Rgb operator*(Rgb a, float s) { return {a.r * s, a.g * s, a.b * s}; }
Rgb mix(Rgb a, Rgb b, float t) { return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t}; }

float smooth01(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// Ease-out with a small overshoot: the new cover "lands".
float ease_out_back(float t)
{
    t = std::clamp(t, 0.0f, 1.0f) - 1.0f;
    constexpr float k = 1.6f;
    return 1.0f + t * t * ((k + 1.0f) * t + k);
}


// Vivid version of a palette colour for light effects: full value, a little
// more saturation. Greyscale artwork stays greyscale (no invented colours).
Rgb accent_of(const rgb_matrix::Color &c)
{
    const float hue = color_hue(c) * 6.0f;
    const float hi = std::max({c.r, c.g, c.b}) / 255.0f, lo = std::min({c.r, c.g, c.b}) / 255.0f;
    if (hi <= 0.0001f) return {200, 200, 210};
    float s = (hi - lo) / hi;
    s = s < 0.12f ? s : std::min(1.0f, std::max(s * 1.35f, 0.55f));
    const float x = 1.0f - std::fabs(std::fmod(hue, 2.0f) - 1.0f);
    Rgb o{};
    switch (static_cast<int>(hue) % 6) {
    case 0: o = {1, x, 0}; break;
    case 1: o = {x, 1, 0}; break;
    case 2: o = {0, 1, x}; break;
    case 3: o = {0, x, 1}; break;
    case 4: o = {x, 0, 1}; break;
    default: o = {1, 0, x}; break;
    }
    // Blend from white (s=0) to the pure hue (s=1).
    return {255.0f * (1 - s + s * o.r), 255.0f * (1 - s + s * o.g), 255.0f * (1 - s + s * o.b)};
}

Rgb sample_accent(const CoverOnlyScene::Artwork &art, float pos)
{
    pos = pos - std::floor(pos);
    const float f = pos * static_cast<float>(art.accent.size());
    const auto i0 = static_cast<size_t>(f) % art.accent.size();
    const auto i1 = (i0 + 1) % art.accent.size();
    return mix(art.accent[i0], art.accent[i1], smooth01(f - std::floor(f)));
}

Rgb sample_bilinear(const std::vector<Rgb> &img, int size, float u, float v)
{
    const float x = std::clamp(u * size - 0.5f, 0.0f, size - 1.0f);
    const float y = std::clamp(v * size - 0.5f, 0.0f, size - 1.0f);
    const int x0 = static_cast<int>(x), y0 = static_cast<int>(y);
    const int x1 = std::min(x0 + 1, size - 1), y1 = std::min(y0 + 1, size - 1);
    const float fx = x - x0, fy = y - y0;
    const auto at = [&](int xx, int yy) { return img[static_cast<size_t>(yy * size + xx)]; };
    return mix(mix(at(x0, y0), at(x1, y0), fx), mix(at(x0, y1), at(x1, y1), fx), fy);
}

std::vector<Rgb> to_rgb(const Magick::Image &image)
{
    const size_t w = image.columns(), h = image.rows();
    std::vector<Rgb> out(w * h);
    const auto *pixels = image.getConstPixels(0, 0, w, h);
    if (!pixels) return out;
    for (size_t i = 0; i < w * h; ++i) {
        const auto &p = pixels[i];
        if (p.opacity == MaxRGB) continue;
        out[i] = {static_cast<float>(ScaleQuantumToChar(p.red)), static_cast<float>(ScaleQuantumToChar(p.green)),
                  static_cast<float>(ScaleQuantumToChar(p.blue))};
    }
    return out;
}

std::optional<SpotifyState> current_spotify_state()
{
    if (auto mirrored = spotify_state_from_runtime_input(RuntimeInputs::snapshot()); mirrored.has_value())
        return mirrored;
    return spotify != nullptr ? spotify->get_currently_playing() : std::nullopt;
}
} // namespace

float CoverOnlyScene::random01()
{
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return static_cast<float>(rng_ & 0xFFFFFF) / static_cast<float>(0xFFFFFF);
}

void CoverOnlyScene::adopt(Artwork artwork)
{
    MediaArtworkState::update(artwork.track_id, artwork.palette);
    if (current_ && current_->track_id == artwork.track_id) {
        // Same track, e.g. the final result after the early cover: keep the
        // running presentation and only take the refined data (BPM).
        current_->bpm = artwork.bpm;
        return;
    }
    previous_ = std::move(current_);
    current_ = std::move(artwork);
    transition_ = previous_ ? 0.0f : 0.35f;
    sparkles_.clear();
}

void CoverOnlyScene::initialize(int width, int height)
{
    Scene::initialize(width, height);
    // Per-pixel backdrop is too much for the Pi next to the artwork worker;
    // start on the (visually equivalent) coarse grid there.
    set_render_quality_hint(0.75f);
}

bool CoverOnlyScene::render(rgb_matrix::FrameCanvas *canvas)
{
    auto temp = current_spotify_state();
    if (!temp.has_value())
    {
        spdlog::debug("Tried to render CoverOnlyScene, but no current track");
        hold_current_frame();
        return false;
    }

    auto track = std::move(temp.value());
    const auto track_id = track.get_track().get_id();
    if (!track_id.has_value())
    {
        spdlog::debug("No track id, exiting");
        hold_current_frame();
        return false;
    }

    curr_state.emplace(track);
    if (requested_track_id != track_id && refresh_future.valid()) artwork_stop.request_stop();

    if (cover_future.valid() && cover_future.wait_for(std::chrono::seconds(0)) == future_status::ready) {
        try {
            auto artwork = cover_future.get();
            if (requested_track_id == track_id)
                adopt(std::move(artwork));
        } catch (const std::future_error &) {
            // An early worker failure is reported by the final result below.
        }
    }

    if (refresh_future.valid() && refresh_future.wait_for(std::chrono::seconds(0)) == future_status::ready) {
        auto result = refresh_future.get();
        if (requested_track_id == track_id) {
            if (!result) {
                spdlog::error("Failed to refresh info: {}", result.error());
                requested_track_id.reset();
                hold_current_frame();
                return false;
            }
            adopt(std::move(*result));
        }
    }

    if (!refresh_future.valid() && requested_track_id != track_id) {
        const int cover_size = std::clamp(std::min(matrix_width, matrix_height) * cover_size_percent->get() / 100, 8,
                                          std::min(matrix_width, matrix_height));
        ArtworkOptions options{matrix_width, matrix_height, cover_size, std::clamp(28 - background_blur->get(), 6, 28)};
        requested_track_id = track_id;
        artwork_stop = std::stop_source{};
        std::promise<ArtworkResult> cover_ready;
        cover_future = cover_ready.get_future();
        refresh_future = std::async(launch::async,
            [snapshot = track.get_track(), options, stop = artwork_stop.get_token(),
             cover_ready = std::move(cover_ready)]() mutable -> std::expected<ArtworkResult, std::string> {
                try {
                    return refresh_info(std::move(snapshot), options, stop, std::move(cover_ready));
                } catch (const std::exception &e) {
                    return unexpected(e.what());
                }
            });
    }

    if (!current_)
    {
        hold_current_frame();
        return true;
    }

    if (!curr_state->is_playing())
    {
        // Preserve exactly the last presented cover while paused. Swapping an
        // untouched double buffer here used to alternate two historical frames
        // on the physical matrix, which looked like severe flicker.
        hold_current_frame();
        return true;
    }

    wait_until_next_frame();
    const float dt = std::clamp(static_cast<float>(frame_context().delta_seconds), 0.0f, 0.1f);
    draw(canvas, dt > 0.0f ? dt : 1.0f / static_cast<float>(std::max(1, get_target_fps())));
    return true;
}

void CoverOnlyScene::draw(rgb_matrix::FrameCanvas *canvas, float dt)
{
    const int W = canvas->width(), H = canvas->height();
    const auto &art = *current_;
    frame_.assign(static_cast<size_t>(W * H), {});
    time_ += dt;

    // --- beat & loudness: live analysis when available, BPM simulation otherwise
    const auto audio = AudioState::snapshot();
    const bool live = audio.fresh();
    float target_level = 0.45f + 0.10f * std::sin(time_ * 0.7f);
    bool beat = false;
    if (live) {
        if (!beat_primed_) { seen_beat_ = audio.beat_counter; beat_primed_ = true; }
        beat = audio.beat_counter > seen_beat_;
        seen_beat_ = audio.beat_counter;
        target_level = std::clamp(audio.feature(AudioProtocol::Feature::LoudnessFast), 0.0f, 1.0f);
    } else {
        beat_primed_ = false;
        beat_phase_ += dt * std::clamp(art.bpm, 40.0f, 220.0f) / 60.0f;
        if (beat_phase_ >= 1.0f) {
            beat_phase_ -= std::floor(beat_phase_);
            beat = true;
        }
    }
    if (beat) pulse_ = 1.0f;
    pulse_ *= std::exp(-dt / 0.16f);
    level_ += (target_level - level_) * (1.0f - std::exp(-dt / 0.35f));

    transition_ = std::min(1.0f, transition_ + dt / 1.1f);
    if (transition_ >= 1.0f) previous_.reset();
    const float fade = smooth01(transition_);

    const float pulse_strength = beat_pulse_strength->get();
    const int bar_h = show_progress->get() ? std::clamp(progress_bar_height->get(), 1, std::max(1, H / 8)) : 0;
    const float cx = W * 0.5f, cy = H * 0.5f - bar_h * 0.5f;

    // --- cover geometry -------------------------------------------------------
    const int size = art.cover_size;
    const float land = previous_ ? 0.82f + 0.18f * ease_out_back(transition_ * 1.25f) : 1.0f;
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
                if (previous_ && fade < 1.0f)
                    c = mix(sample_bilinear(previous_->backdrop, previous_->backdrop_size, su, sv), c, fade);
                const float vignette = std::max(0.25f, 1.0f - 1.1f * (u * u + v * v));
                grid_[static_cast<size_t>(gy * gw + gx)] = c * (brightness * vignette);
            }
        }
        // Opaque cover rectangle (only when no fade-in/out blends through it).
        int hx0 = W, hx1 = -1, hy0 = H, hy1 = -1;
        if (!previous_) {
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
                if (live && !audio.spectrum.empty()) {
                    // Skip the (often empty) lowest bins; normalise each segment
                    // against its own slowly decaying peak so every part moves.
                    const auto bin = std::min(audio.spectrum.size() - 1, static_cast<size_t>((0.03f + m * 0.75f) * audio.spectrum.size()));
                    const float v = audio.spectrum[bin];
                    auto &pk = aura_peak_[static_cast<size_t>(i)];
                    pk = std::max({v, pk * std::exp(-dt / 5.0f), 0.05f});
                    target = std::pow(std::clamp(v / pk, 0.0f, 1.0f), 1.6f);
                } else {
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
                if (previous_ && fade < 1.0f) col = mix(sample_accent(*previous_, m * 0.999f), col, fade);
                auto &px = frame_[static_cast<size_t>(y * W + x)];
                px = px + col * (glow * 0.85f);
            }
    }

    // --- sparkles thrown off the cover edge on beats ---------------------------
    if (beat_sparkles->get() && beat) {
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
    if (previous_ && fade < 1.0f)
        draw_cover(*previous_, 1.0f - 0.3f * fade, 1.0f - fade);
    draw_cover(art, scale, previous_ ? std::min(1.0f, transition_ * 3.0f) : 1.0f);

    // --- progress bar in the cover's palette ------------------------------------
    if (bar_h > 0) {
        const float progress = std::clamp(curr_state->get_progress().value_or(0.0f), 0.0f, 1.0f);
        const float filled = progress * W;
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

    RgbBlit::present(canvas, W, H, rgb_, [this, W](int x, int y, uint8_t *out) {
        const auto &p = frame_[static_cast<size_t>(y * W + x)];
        out[0] = RgbBlit::from_byte(p.r);
        out[1] = RgbBlit::from_byte(p.g);
        out[2] = RgbBlit::from_byte(p.b);
    });
}

std::expected<CoverOnlyScene::ArtworkResult, std::string> CoverOnlyScene::refresh_info(
    SpotifyTrack track, ArtworkOptions options, std::stop_token stop,
    std::promise<ArtworkResult> cover_ready)
{
    if (stop.stop_requested()) return unexpected("Artwork request cancelled");
    auto opt_track = track.get_id();
    if (!opt_track.has_value())
        return unexpected("No track id");

    const string track_id = opt_track.value();
    trace("New track, refreshing state: {}", track_id);

    auto cover_opt = track.get_cover();
    if (!cover_opt.has_value())
        return unexpected("No track cover for track '" + track_id + "'");

    const string out_file = "/tmp/spotify_cover." + track_id + ".jpg";
    if (!std::filesystem::exists(out_file)) {
        const auto res = utils::download_image(cover_opt.value(), out_file, stop);
        if (!res.has_value())
            return unexpected(res.error());
    }

    auto res = LoadImageAndScale(out_file, options.width, options.height, true, true, true);
    try_remove(out_file);
    if (!res)
        return unexpected(res.error());

    Magick::Image source = res.value()[0];
    ArtworkResult result;
    result.track_id = track_id;
    result.palette = extract_artwork_palette(source);
    for (size_t i = 0; i < result.palette.size(); ++i)
        result.accent[i] = accent_of(result.palette[i]);

    Magick::Geometry cover_geometry(options.cover_size, options.cover_size);
    cover_geometry.aspect(true);
    Magick::Image cover = source;
    cover.filterType(Magick::LanczosFilter);
    cover.resize(cover_geometry);
    cover.modulate(100.0, 110.0, 100.0);
    result.cover_size = static_cast<int>(cover.columns());
    result.cover = to_rgb(cover);
    if (static_cast<int>(cover.rows()) != result.cover_size)
        return unexpected("Unexpected cover geometry");

    // The backdrop is a tiny, saturated thumbnail; bilinear upsampling at render
    // time turns it into a smooth colour field.
    Magick::Geometry backdrop_geometry(options.backdrop_size, options.backdrop_size);
    backdrop_geometry.aspect(true);
    Magick::Image backdrop = source;
    backdrop.filterType(Magick::BoxFilter);
    backdrop.resize(backdrop_geometry);
    backdrop.modulate(100.0, 130.0, 100.0);
    result.backdrop_size = static_cast<int>(backdrop.columns());
    result.backdrop = to_rgb(backdrop);
    if (static_cast<int>(backdrop.rows()) != result.backdrop_size)
        return unexpected("Unexpected backdrop geometry");

    if (stop.stop_requested()) return unexpected("Artwork request cancelled");
    cover_ready.set_value(result);

    auto bpm_res = SongBpmApi::get_bpm(track.get_song_name().value_or(""), track.get_artist_name().value_or(""), stop);
    if (!bpm_res.has_value())
        spdlog::debug("Couldn't get bpm {}", bpm_res.error());
    result.bpm = bpm_res.value_or(120);
    if (!std::isfinite(result.bpm) || result.bpm <= 0) result.bpm = 120;
    result.bpm = std::clamp(result.bpm, 30.0f, 300.0f);
    return result;
}

int CoverOnlyScene::get_weight() const
{
    if (spotify != nullptr)
    {
        if (spotify->has_changed(false))
            return scene_weight_if_new_song->get();

        if (spotify->get_currently_playing().has_value())
            return Scene::get_weight();
    }

    // Don't display this scene if no song is playing
    return 0;
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

CoverOnlyScene::~CoverOnlyScene()
{
    artwork_stop.request_stop();
    if (refresh_future.valid())
        refresh_future.wait();
}

Scenes::SceneDescriptor Scenes::CoverOnlyScene::get_descriptor() const {
    auto d = Scene::get_descriptor(); d.automatic_eligible = true;
    d.family = "album-art";
    d.tags = {"music", "media", "album-art", "spotify", "audio-reactive"};
    d.intensity = 0.40f; d.motion = 0.36f; d.music_affinity = 1.0f; d.performance_cost = 0.40f;
    d.variants = {
        {"focus", "Cover focus", "Large crisp artwork, calm backdrop and a thin aura",
         {{"cover_size_percent", 84}, {"background_brightness", 34}, {"background_motion", 0.6f}, {"cover_border_glow_intensity", 0.20f},
          {"beat_pulse_strength", 0.15f}, {"beat_sparkles", false}, {"progress_bar_height", 2}},
         {"minimal", "album-art"}, 0.28f, 0.20f, 1.0f, 0.36f},
        {"ambient", "Ambient cover", "Smaller artwork floating in a flowing colour field with a lively spectrum aura",
         {{"cover_size_percent", 62}, {"background_blur", 14}, {"background_brightness", 46}, {"background_motion", 1.4f},
          {"cover_border_glow_intensity", 0.45f}, {"beat_pulse_strength", 0.32f}},
         {"ambient", "glow", "spectrum"}, 0.48f, 0.42f, 1.0f, 0.42f},
    };
    return d;
}
