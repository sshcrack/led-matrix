#pragma once

#include <memory>
#include <nlohmann/json.hpp>
#include <string_view>

#include "shared/matrix/preview.h"

/// Offline fixture for the image scene's previews. The scene normally scrapes
/// pixeljoint.com, so a slow or blocked network made it stop early, which
/// `--strict` counts as a skipped scene. The fixture seeds the post cache with a
/// synthetic image, so previews exercise the real download-free load path.
namespace ImagePreviewFixture {
inline constexpr std::string_view InputId = "pixeljoint.images";

/// Provider list for the scene's `providers` property: one collection holding
/// the fixture image.
[[nodiscard]] nlohmann::json providers_json();

[[nodiscard]] std::unique_ptr<Previews::DataProvider> make_provider();
}  // namespace ImagePreviewFixture
