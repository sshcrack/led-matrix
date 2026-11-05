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

nlohmann::json make_state(const std::string &track_id, const long progress_ms, const long duration_ms)
{
    return {
        {"timestamp", system_now_ms()},
        {"progress_ms", progress_ms},
        {"is_playing", true},
        {"item", {
            {"id", track_id},
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

size_t SpotifyPreviewProvider::track_index(const double elapsed_seconds) const
{
    if (track_ids_.size() <= 1 || track_seconds_ <= 0.0) return 0;
    return static_cast<size_t>(std::max(0.0, elapsed_seconds) / track_seconds_) % track_ids_.size();
}

void SpotifyPreviewProvider::begin(const Previews::RunContext &context)
{
    if (spotify == nullptr)
        throw std::runtime_error("Spotify preview provider requires the Spotify plugin runtime");

    const auto options = context.options_for(id());
    duration_ms_ = std::max<long>(10000, options.value("duration_ms", 180000L));
    track_seconds_ = std::max(0.0, options.value("track_seconds", 0.0));
    cover_paths_.clear();
    track_ids_.clear();

    std::vector<std::filesystem::path> sources;
    if (const auto it = options.find("covers"); it != options.end() && it->is_array())
        for (const auto &entry : *it)
            if (entry.is_string()) sources.emplace_back(entry.get<std::string>());

    // The scene downloads "/tmp/spotify_cover.<track id>.jpg" unless it already
    // exists, so pre-seeding that path keeps the fixture fully offline.
    const size_t count = std::max<size_t>(1, sources.size());
    for (size_t i = 0; i < count; ++i) {
        const auto track_id = count == 1 ? std::string(TrackId) : std::string(TrackId) + "-" + std::to_string(i);
        const auto path = std::filesystem::path("/tmp") / ("spotify_cover." + track_id + ".jpg");
        if (i < sources.size()) {
            Magick::Image image(sources[i].string());
            image.magick("JPEG");
            image.quality(92);
            image.write(path.string());
        } else {
            write_preview_cover(path);
        }
        track_ids_.push_back(track_id);
        cover_paths_.push_back(path);
    }
    spotify->set_preview_currently_playing(make_state(track_ids_.front(), 0, duration_ms_));
}

void SpotifyPreviewProvider::update(const Scenes::SceneFrameContext &frame)
{
    if (spotify == nullptr || track_ids_.empty()) return;
    const auto progress = static_cast<long>(
        std::fmod(std::max(0.0, frame.elapsed_seconds) * 1000.0 + 42000.0,
                  static_cast<double>(duration_ms_)));
    spotify->set_preview_currently_playing(make_state(track_ids_[track_index(frame.elapsed_seconds)], progress, duration_ms_));
}

void SpotifyPreviewProvider::end() noexcept
{
    try {
        if (spotify != nullptr)
            spotify->clear_preview_currently_playing();
        MediaArtworkState::clear();
        for (const auto &path : cover_paths_) {
            std::error_code ec;
            std::filesystem::remove(path, ec);
        }
    } catch (...) {
        // Preview fixture cleanup must never mask a scene-rendering result.
    }
}
