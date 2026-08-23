#pragma once

#include "led-matrix.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>

#include <nlohmann/json_fwd.hpp>

namespace MediaArtworkState {

constexpr std::size_t PaletteSize = 5;
using Palette = std::array<rgb_matrix::Color, PaletteSize>;

struct Snapshot {
    Palette colors{};
    std::uint64_t generation = 0;
    float transition = 1.0f;
    bool valid = false;
};

/// Publish the dominant colors for a media item. Updating with the same key is
/// ignored, while a new key cross-fades from the previous artwork over a short
/// interval so live visual scenes do not hard-cut when a track changes.
void update(const std::string &media_key, const Palette &colors);
void clear();
[[nodiscard]] Snapshot snapshot();

/// Serialize/restore the currently visible palette for another scene runtime.
/// The snapshot contains already-blended colors, so the receiving runtime can
/// match the Pi immediately without needing the original image or transition history.
[[nodiscard]] nlohmann::json to_json(const Snapshot &snapshot);
void replace_from_json(const nlohmann::json &snapshot_json);

/// Sample the five-color artwork palette continuously. The palette is ordered
/// by hue after extraction so interpolation remains visually coherent.
[[nodiscard]] rgb_matrix::Color sample(const Snapshot &snapshot, float position);

} // namespace MediaArtworkState
