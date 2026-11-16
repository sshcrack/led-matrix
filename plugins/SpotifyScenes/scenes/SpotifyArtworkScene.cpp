#ifdef _WIN32
#include "shared/common/win_compat.h"
#endif
#include <array>
#include <chrono>
#include <cmath>
#include <exception>
#include <filesystem>
#include <numeric>
#include <vector>

#include "Magick++.h"
#include "SpotifyArtworkScene.h"
#include "led-matrix.h"
#include "matrix/manager/shared_spotify.h"
#include "matrix/manager/song_bpm_getter.h"
#include "shared/matrix/audio_state.h"
#include "shared/matrix/input_ids.h"
#include "shared/matrix/media_artwork_state.h"
#include "shared/matrix/runtime_inputs.h"
#include "shared/matrix/utils/canvas_image.h"
#include "shared/matrix/utils/image_fetch.h"
#include "shared/matrix/utils/rgb_blit.h"
#include "spdlog/spdlog.h"

using namespace spdlog;
using namespace std;
using namespace Scenes;
using namespace Scenes::ArtworkPaint;

namespace {
struct PaletteBin {
    uint32_t count = 0;
    uint32_t r = 0;
    uint32_t g = 0;
    uint32_t b = 0;
    float saturation_sum = 0.0f;
};

float color_hue(const rgb_matrix::Color& color)
{
    const float r = color.r / 255.0f;
    const float g = color.g / 255.0f;
    const float b = color.b / 255.0f;
    const float hi = std::max({r, g, b});
    const float lo = std::min({r, g, b});
    const float delta = hi - lo;
    if (delta < 0.0001f)
        return 0.0f;
    float hue = 0.0f;
    if (hi == r)
        hue = std::fmod((g - b) / delta, 6.0f);
    else if (hi == g)
        hue = (b - r) / delta + 2.0f;
    else
        hue = (r - g) / delta + 4.0f;
    hue /= 6.0f;
    if (hue < 0.0f)
        hue += 1.0f;
    return hue;
}

MediaArtworkState::Palette extract_artwork_palette(const Magick::Image& image)
{
    MediaArtworkState::Palette fallback{rgb_matrix::Color(44, 90, 210), rgb_matrix::Color(35, 190, 210), rgb_matrix::Color(95, 210, 115),
                                        rgb_matrix::Color(235, 170, 65), rgb_matrix::Color(215, 70, 155)};
    if (image.columns() == 0 || image.rows() == 0)
        return fallback;

    constexpr int QuantizedLevels = 16;
    constexpr int BinCount = QuantizedLevels * QuantizedLevels * QuantizedLevels;
    std::array<PaletteBin, BinCount> bins{};
    const auto* pixels = image.getConstPixels(0, 0, image.columns(), image.rows());
    if (!pixels)
        return fallback;

    const size_t step_x = std::max<size_t>(1, image.columns() / 32);
    const size_t step_y = std::max<size_t>(1, image.rows() / 32);
    for (size_t y = 0; y < image.rows(); y += step_y) {
        for (size_t x = 0; x < image.columns(); x += step_x) {
            const auto& pixel = pixels[y * image.columns() + x];
            if (pixel.opacity == MaxRGB)
                continue;
            const uint8_t r = ScaleQuantumToChar(pixel.red);
            const uint8_t g = ScaleQuantumToChar(pixel.green);
            const uint8_t b = ScaleQuantumToChar(pixel.blue);
            const uint8_t hi = std::max({r, g, b});
            const uint8_t lo = std::min({r, g, b});
            const float luma = (0.2126f * r + 0.7152f * g + 0.0722f * b) / 255.0f;
            const float saturation = hi > 0 ? static_cast<float>(hi - lo) / hi : 0.0f;
            if (luma < 0.035f || luma > 0.97f)
                continue;

            const int index = ((r >> 4) << 8) | ((g >> 4) << 4) | (b >> 4);
            auto& bin = bins[static_cast<size_t>(index)];
            ++bin.count;
            bin.r += r;
            bin.g += g;
            bin.b += b;
            bin.saturation_sum += saturation;
        }
    }

    std::array<int, BinCount> order{};
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](int lhs, int rhs) {
        const auto score = [&](int index) {
            const auto& bin = bins[static_cast<size_t>(index)];
            if (bin.count == 0)
                return 0.0f;
            const float saturation = bin.saturation_sum / bin.count;
            return bin.count * (0.58f + saturation * 1.35f);
        };
        return score(lhs) > score(rhs);
    });

    std::vector<rgb_matrix::Color> selected;
    selected.reserve(MediaArtworkState::PaletteSize);
    for (const int index : order) {
        const auto& bin = bins[static_cast<size_t>(index)];
        if (bin.count == 0)
            break;
        const rgb_matrix::Color candidate(static_cast<uint8_t>(bin.r / bin.count), static_cast<uint8_t>(bin.g / bin.count),
                                          static_cast<uint8_t>(bin.b / bin.count));
        bool distinct = true;
        for (const auto& existing : selected) {
            const int dr = static_cast<int>(candidate.r) - existing.r;
            const int dg = static_cast<int>(candidate.g) - existing.g;
            const int db = static_cast<int>(candidate.b) - existing.b;
            if (dr * dr + dg * dg + db * db < 34 * 34) {
                distinct = false;
                break;
            }
        }
        if (!distinct)
            continue;
        selected.push_back(candidate);
        if (selected.size() == MediaArtworkState::PaletteSize)
            break;
    }

    if (selected.empty())
        return fallback;
    const size_t discovered = selected.size();
    while (selected.size() < MediaArtworkState::PaletteSize) selected.push_back(selected[selected.size() % discovered]);
    std::sort(selected.begin(), selected.end(), [](const auto& lhs, const auto& rhs) { return color_hue(lhs) < color_hue(rhs); });
    MediaArtworkState::Palette result{};
    std::copy_n(selected.begin(), MediaArtworkState::PaletteSize, result.begin());
    return result;
}
}  // namespace

namespace {
// Vivid version of a palette colour for light effects: full value, a little
// more saturation. Greyscale artwork stays greyscale (no invented colours).
Rgb accent_of(const rgb_matrix::Color& c)
{
    const float hue = color_hue(c) * 6.0f;
    const float hi = std::max({c.r, c.g, c.b}) / 255.0f, lo = std::min({c.r, c.g, c.b}) / 255.0f;
    if (hi <= 0.0001f)
        return {200, 200, 210};
    float s = (hi - lo) / hi;
    s = s < 0.12f ? s : std::min(1.0f, std::max(s * 1.35f, 0.55f));
    const float x = 1.0f - std::fabs(std::fmod(hue, 2.0f) - 1.0f);
    Rgb o{};
    switch (static_cast<int>(hue) % 6) {
        case 0:
            o = {1, x, 0};
            break;
        case 1:
            o = {x, 1, 0};
            break;
        case 2:
            o = {0, 1, x};
            break;
        case 3:
            o = {0, x, 1};
            break;
        case 4:
            o = {x, 0, 1};
            break;
        default:
            o = {1, 0, x};
            break;
    }
    // Blend from white (s=0) to the pure hue (s=1).
    return {255.0f * (1 - s + s * o.r), 255.0f * (1 - s + s * o.g), 255.0f * (1 - s + s * o.b)};
}

std::vector<Rgb> to_rgb(const Magick::Image& image)
{
    const size_t w = image.columns(), h = image.rows();
    std::vector<Rgb> out(w * h);
    const auto* pixels = image.getConstPixels(0, 0, w, h);
    if (!pixels)
        return out;
    for (size_t i = 0; i < w * h; ++i) {
        const auto& p = pixels[i];
        if (p.opacity == MaxRGB)
            continue;
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
}  // namespace

float SpotifyArtworkScene::random01()
{
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return static_cast<float>(rng_ & 0xFFFFFF) / static_cast<float>(0xFFFFFF);
}

float SpotifyArtworkScene::progress()
{
    return curr_state ? std::clamp(curr_state->get_progress().value_or(0.0f), 0.0f, 1.0f) : 0.0f;
}

void SpotifyArtworkScene::adopt(Artwork artwork)
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
    on_track_changed();
}

bool SpotifyArtworkScene::render(rgb_matrix::FrameCanvas* canvas)
{
    auto temp = current_spotify_state();
    if (!temp.has_value()) {
        spdlog::debug("Tried to render {}, but no current track", get_name());
        hold_current_frame();
        return false;
    }

    auto track = std::move(temp.value());
    const auto track_id = track.get_track().get_id();
    if (!track_id.has_value()) {
        spdlog::debug("No track id, exiting");
        hold_current_frame();
        return false;
    }

    curr_state.emplace(track);
    if (requested_track_id != track_id && refresh_future.valid())
        artwork_stop.request_stop();

    if (cover_future.valid() && cover_future.wait_for(std::chrono::seconds(0)) == future_status::ready) {
        try {
            auto artwork = cover_future.get();
            if (requested_track_id == track_id)
                adopt(std::move(artwork));
        }
        catch (const std::future_error&) {
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
        const ArtworkOptions options = artwork_options();
        requested_track_id = track_id;
        artwork_stop = std::stop_source{};
        std::promise<Artwork> cover_ready;
        cover_future = cover_ready.get_future();
        refresh_future = std::async(launch::async,
                                    [snapshot = track.get_track(), options, stop = artwork_stop.get_token(),
                                     cover_ready = std::move(cover_ready)]() mutable -> std::expected<Artwork, std::string> {
                                        try {
                                            return refresh_info(std::move(snapshot), options, stop, std::move(cover_ready));
                                        }
                                        catch (const std::exception& e) {
                                            return unexpected(e.what());
                                        }
                                    });
    }

    if (!current_) {
        hold_current_frame();
        return true;
    }

    if (!curr_state->is_playing()) {
        // Preserve exactly the last presented frame while paused. Swapping an
        // untouched double buffer here used to alternate two historical frames
        // on the physical matrix, which looked like severe flicker.
        hold_current_frame();
        return true;
    }

    wait_until_next_frame();
    float dt = std::clamp(static_cast<float>(frame_context().delta_seconds), 0.0f, 0.1f);
    if (dt <= 0.0f)
        dt = 1.0f / static_cast<float>(std::max(1, get_target_fps()));
    update_music(dt);
    transition_ = std::min(1.0f, transition_ + dt / transition_seconds());
    if (transition_ >= 1.0f)
        previous_.reset();
    frame_.assign(static_cast<size_t>(canvas->width() * canvas->height()), {});
    draw(canvas, dt);
    return true;
}

void SpotifyArtworkScene::update_music(float dt)
{
    time_ += dt;
    const auto audio = AudioState::snapshot();
    live_ = audio.fresh();
    float target_level = 0.45f + 0.10f * std::sin(time_ * 0.7f);
    beat_ = false;
    if (live_) {
        if (!beat_primed_) {
            seen_beat_ = audio.beat_counter;
            beat_primed_ = true;
        }
        beat_ = audio.beat_counter > seen_beat_;
        seen_beat_ = audio.beat_counter;
        target_level = std::clamp(audio.feature(AudioProtocol::Feature::LoudnessFast), 0.0f, 1.0f);
        spectrum_.assign(audio.spectrum.begin(), audio.spectrum.end());
    }
    else {
        beat_primed_ = false;
        spectrum_.clear();
        beat_phase_ += dt * std::clamp(current_->bpm, 40.0f, 220.0f) / 60.0f;
        if (beat_phase_ >= 1.0f) {
            beat_phase_ -= std::floor(beat_phase_);
            beat_ = true;
        }
    }
    if (beat_)
        pulse_ = 1.0f;
    pulse_ *= std::exp(-dt / 0.16f);
    level_ += (target_level - level_) * (1.0f - std::exp(-dt / 0.35f));
}

void SpotifyArtworkScene::present(rgb_matrix::FrameCanvas* canvas)
{
    const int W = canvas->width(), H = canvas->height();
    RgbBlit::present(canvas, W, H, rgb_, [this, W](int x, int y, uint8_t* out) {
        const auto& p = frame_[static_cast<size_t>(y * W + x)];
        out[0] = RgbBlit::from_byte(p.r);
        out[1] = RgbBlit::from_byte(p.g);
        out[2] = RgbBlit::from_byte(p.b);
    });
}

std::expected<SpotifyArtworkScene::Artwork, std::string> SpotifyArtworkScene::refresh_info(SpotifyTrack track, ArtworkOptions options,
                                                                                           std::stop_token stop,
                                                                                           std::promise<Artwork> cover_ready)
{
    if (stop.stop_requested())
        return unexpected("Artwork request cancelled");
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
    Artwork result;
    result.track_id = track_id;
    result.palette = extract_artwork_palette(source);
    for (size_t i = 0; i < result.palette.size(); ++i) result.accent[i] = accent_of(result.palette[i]);

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

    if (stop.stop_requested())
        return unexpected("Artwork request cancelled");
    cover_ready.set_value(result);

    const std::string bpm_key = spotify != nullptr ? spotify->song_bpm_api_key() : std::string();
    auto bpm_res = SongBpmApi::get_bpm(track.get_song_name().value_or(""), track.get_artist_name().value_or(""), bpm_key, stop);
    if (!bpm_res.has_value())
        spdlog::debug("Couldn't get bpm {}", bpm_res.error());
    result.bpm = bpm_res.value_or(120);
    if (!std::isfinite(result.bpm) || result.bpm <= 0)
        result.bpm = 120;
    result.bpm = std::clamp(result.bpm, 30.0f, 300.0f);
    return result;
}

int SpotifyArtworkScene::get_weight() const
{
    if (spotify != nullptr) {
        if (spotify->has_changed(false))
            return scene_weight_if_new_song->get();

        if (spotify->get_currently_playing().has_value())
            return Scene::get_weight();
    }

    // Don't display this scene if no song is playing
    return 0;
}

SpotifyArtworkScene::~SpotifyArtworkScene()
{
    artwork_stop.request_stop();
    if (refresh_future.valid())
        refresh_future.wait();
}
