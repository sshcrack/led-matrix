#include "SpotifyPreviewProvider.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>

#include <Magick++.h>
#include <nlohmann/json.hpp>

#include "manager/shared_spotify.h"
#include "manager/spotify.h"
#include "shared/matrix/media_artwork_state.h"

namespace {

long system_now_ms()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

nlohmann::json make_state(const long progress_ms, const long duration_ms)
{
    return {
        {"timestamp", system_now_ms()},
        {"progress_ms", progress_ms},
        {"is_playing", true},
        {"item", {
            {"id", "preview-track"},
            {"name", "Preview Track"},
            {"duration_ms", duration_ms},
            {"artists", nlohmann::json::array({{{"name", "Matrix Studio"}}})},
            {"album", {{"images", nlohmann::json::array({{{"url", "preview://album-art"}}})}}}
        }}
    };
}

void write_preview_cover(const std::filesystem::path &path)
{
    constexpr std::size_t size = 256;
    Magick::Image image(Magick::Geometry(size, size), Magick::ColorRGB(0.025, 0.03, 0.07));
    image.modifyImage();
    auto *pixels = image.getPixels(0, 0, size, size);

    for (std::size_t y = 0; y < size; ++y) {
        for (std::size_t x = 0; x < size; ++x) {
            const float nx = (static_cast<float>(x) - size * 0.5f) / (size * 0.5f);
            const float ny = (static_cast<float>(y) - size * 0.5f) / (size * 0.5f);
            const float radius = std::sqrt(nx * nx + ny * ny);
            const float angle = std::atan2(ny, nx);
            const float ring = 0.5f + 0.5f * std::sin(radius * 28.0f - angle * 5.0f);
            const float glow = std::clamp(1.0f - radius * 0.72f, 0.0f, 1.0f);
            const float r = std::clamp(0.08f + glow * (0.25f + ring * 0.70f), 0.0f, 1.0f);
            const float g = std::clamp(0.04f + glow * (0.12f + (1.0f - ring) * 0.58f), 0.0f, 1.0f);
            const float b = std::clamp(0.16f + glow * (0.52f + ring * 0.40f), 0.0f, 1.0f);
            auto &pixel = pixels[y * size + x];
            pixel.red = static_cast<MagickLib::Quantum>(r * MaxRGB);
            pixel.green = static_cast<MagickLib::Quantum>(g * MaxRGB);
            pixel.blue = static_cast<MagickLib::Quantum>(b * MaxRGB);
            pixel.opacity = 0;
        }
    }
    image.syncPixels();
    image.quality(88);
    image.write(path.string());
}

} // namespace

void SpotifyPreviewProvider::begin(const Previews::RunContext &context)
{
    if (spotify == nullptr)
        throw std::runtime_error("Spotify preview provider requires the Spotify plugin runtime");

    const auto options = context.options_for(id());
    duration_ms_ = std::max<long>(10000, options.value("duration_ms", 180000L));
    cover_path_ = std::filesystem::path("/tmp") /
        ("spotify_cover." + std::string(TrackId) + ".jpg");
    write_preview_cover(cover_path_);
    spotify->set_preview_currently_playing(make_state(0, duration_ms_));
}

void SpotifyPreviewProvider::update(const Scenes::SceneFrameContext &frame)
{
    if (spotify == nullptr) return;
    const auto progress = static_cast<long>(
        std::fmod(std::max(0.0, frame.elapsed_seconds) * 1000.0,
                  static_cast<double>(duration_ms_)));
    spotify->set_preview_currently_playing(make_state(progress, duration_ms_));
}

void SpotifyPreviewProvider::end() noexcept
{
    try {
        if (spotify != nullptr)
            spotify->clear_preview_currently_playing();
        MediaArtworkState::clear();
        if (!cover_path_.empty()) {
            std::error_code ec;
            std::filesystem::remove(cover_path_, ec);
        }
    } catch (...) {
        // Preview fixture cleanup must never mask a scene-rendering result.
    }
}
