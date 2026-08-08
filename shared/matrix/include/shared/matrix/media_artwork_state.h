#pragma once

#include "led-matrix.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>

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

/// Sample the five-color artwork palette continuously. The palette is ordered
/// by hue after extraction so interpolation remains visually coherent.
[[nodiscard]] rgb_matrix::Color sample(const Snapshot &snapshot, float position);

} // namespace MediaArtworkState
