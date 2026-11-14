#include "WeatherPreviewFixture.h"

#include <Magick++.h>
#include <spdlog/spdlog.h>

#include <atomic>
#include <functional>
#include <mutex>
#include <nlohmann/json.hpp>
#include <system_error>

namespace fs = std::filesystem;

namespace {
std::mutex fixture_mutex;
std::optional<fs::path> fixture_icon_dir;

nlohmann::json make_response()
{
    return {
        {"current_units", {{"temperature_2m", "°C"}, {"wind_speed_10m", "km/h"}}},
        {"current",
         {{"time", "2026-11-14T12:00"},
          {"temperature_2m", 11.4},
          {"relative_humidity_2m", 72},
          {"is_day", 1},
          {"precipitation", 0.3},
          {"weather_code", 61},
          {"cloud_cover", 78},
          {"wind_speed_10m", 14.0}}},
        {"daily_units", {{"temperature_2m_max", "°C"}}},
        {"daily",
         {{"time", {"2026-11-14", "2026-11-15", "2026-11-16", "2026-11-17"}},
          {"weather_code", {61, 3, 0, 71}},
          {"temperature_2m_max", {12.0, 10.0, 13.0, 2.0}},
          {"temperature_2m_min", {6.0, 5.0, 4.0, -2.0}},
          {"precipitation_probability_max", {80, 40, 5, 65}},
          {"sunrise", {"2026-11-14T07:31", "2026-11-15T07:33", "2026-11-16T07:34", "2026-11-17T07:36"}},
          {"sunset", {"2026-11-14T16:32", "2026-11-15T16:31", "2026-11-16T16:29", "2026-11-17T16:28"}}}},
    };
}

class Provider final : public Previews::DataProvider {
public:
    [[nodiscard]] std::string_view id() const override { return WeatherPreviewFixture::InputId; }

    void begin(const Previews::RunContext&) override
    {
        std::error_code ec;
        const auto dir = fs::temp_directory_path(ec) / "led-matrix-weather-preview";
        fs::remove_all(dir, ec);
        fs::create_directories(dir, ec);
        if (ec)
            throw std::runtime_error("Could not create the weather preview icon directory: " + ec.message());
        std::lock_guard lock(fixture_mutex);
        fixture_icon_dir = dir;
    }

    void update(const Scenes::SceneFrameContext&) override {}

    void end() noexcept override
    {
        try {
            std::lock_guard lock(fixture_mutex);
            if (fixture_icon_dir) {
                std::error_code ec;
                fs::remove_all(*fixture_icon_dir, ec);
            }
            fixture_icon_dir.reset();
        }
        catch (...) {
            // Cleanup must never mask a scene-rendering result.
        }
    }
};
}  // namespace

namespace WeatherPreviewFixture {
std::optional<std::string> api_response()
{
    std::lock_guard lock(fixture_mutex);
    if (!fixture_icon_dir)
        return std::nullopt;
    return make_response().dump();
}

std::optional<fs::path> icon_dir()
{
    std::lock_guard lock(fixture_mutex);
    return fixture_icon_dir;
}

bool write_icon(const fs::path& path, const std::string& url)
{
    try {
        constexpr size_t size = 64;
        const auto h = std::hash<std::string>{}(url);
        const double hue = static_cast<double>(h % 360) / 360.0;
        Magick::Image image(Magick::Geometry(size, size), Magick::Color(0, 0, 0, MaxRGB));
        image.matte(true);
        image.fillColor(Magick::ColorHSL(hue, 0.55, 0.62));
        image.draw(Magick::DrawableCircle(size / 2.0, size / 2.0, size / 2.0, size * 0.12));
        image.write(path.string());
        return true;
    }
    catch (const std::exception& e) {
        spdlog::warn("Could not write the weather preview icon: {}", e.what());
        return false;
    }
}

std::unique_ptr<Previews::DataProvider> make_provider()
{
    return std::make_unique<Provider>();
}
}  // namespace WeatherPreviewFixture
