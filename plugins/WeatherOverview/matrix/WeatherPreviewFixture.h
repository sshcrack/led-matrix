#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include "shared/matrix/preview.h"

/// Offline fixture for the weather scene's previews. preview_gen (and so CI's
/// sanitizer sweep) must not depend on Open-Meteo or openweathermap.org being
/// reachable: a slow or blocked network made the scene stop early, which
/// `--strict` counts as a skipped scene.
namespace WeatherPreviewFixture {
inline constexpr std::string_view InputId = "weather.forecast";

/// Open-Meteo response served instead of the live API while the fixture is
/// active (it still goes through the real parser).
[[nodiscard]] std::optional<std::string> api_response();

/// Directory for synthetic icons while the fixture is active, so previews never
/// write into (or read from) the real icon cache.
[[nodiscard]] std::optional<std::filesystem::path> icon_dir();

/// Writes a synthetic weather icon for `url` to `path` (deterministic per URL).
bool write_icon(const std::filesystem::path& path, const std::string& url);

[[nodiscard]] std::unique_ptr<Previews::DataProvider> make_provider();
}  // namespace WeatherPreviewFixture
