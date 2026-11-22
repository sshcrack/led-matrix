#pragma once

#include "led-matrix.h"
#include <cstdint>
#include <string>

using rgb_matrix::FrameCanvas;

/// Base class for a scene-to-scene transition effect.
/// Implementations blend the 'from' and 'to' frames into 'dst' based on the
/// normalized progress value alpha (0.0 = fully from, 1.0 = fully to).
///
/// Effects work on plain RGB888 buffers, not canvases: reading a pixel back from
/// a canvas decodes it from the panel's bit planes, which is expensive on the Pi.
/// `TransitionComposer` decodes each input only when it changes and writes the
/// result to the panel with one bulk `SetRGB()`.
class TransitionEffect {
public:
    virtual ~TransitionEffect() = default;

    virtual std::string get_name() const = 0;

    /// Apply the transition. All buffers are row-major RGB888,
    /// `width * height * 3` bytes, pixel (x, y) at `(y * width + x) * 3`.
    /// @param dst    Buffer to write the blended frame into (every pixel).
    /// @param from   The outgoing scene's frame.
    /// @param to     The incoming scene's frame.
    /// @param alpha  Progress in [0.0, 1.0].
    /// @param width  Frame width in pixels.
    /// @param height Frame height in pixels.
    virtual void apply(std::uint8_t *dst, const std::uint8_t *from, const std::uint8_t *to,
                       float alpha, int width, int height) = 0;
};
