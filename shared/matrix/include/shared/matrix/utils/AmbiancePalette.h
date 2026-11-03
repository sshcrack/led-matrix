#pragma once

#include <graphics.h>
#include <algorithm>
#include <array>

namespace Ambiance {
enum class Palette { ORIGINAL, EMBER, RAIN, SPACE };

// Sample within the scene's existing draw loop: no extra framebuffer pass or
// hardware brightness changes are needed to keep adjacent looks cohesive.
inline rgb_matrix::Color sample(Palette palette, float position, float brightness) {
    using RGB = std::array<float, 3>;
    std::array<RGB, 3> colors;
    switch (palette) {
        case Palette::EMBER: colors = {{{70,16,6}, {210,79,24}, {255,178,78}}}; break;
        case Palette::RAIN: colors = {{{8,28,64}, {28,93,129}, {88,158,181}}}; break;
        default: colors = {{{22,10,61}, {67,57,139}, {124,160,205}}}; break;
    }
    const float offset = std::clamp(position, 0.0f, 1.0f) * 2;
    const int index = std::min(1, static_cast<int>(offset));
    const float mix = offset - index;
    brightness = std::clamp(brightness, 0.0f, 1.0f);
    const auto channel = [&](int c) {
        return static_cast<uint8_t>((colors[index][c] * (1 - mix) + colors[index+1][c] * mix) * brightness);
    };
    return {channel(0), channel(1), channel(2)};
}
} // namespace Ambiance
