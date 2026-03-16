#pragma once

#include "led-matrix.h"
#include <string>

using rgb_matrix::FrameCanvas;

/// Base class for a scene-to-scene transition effect.
/// Implementations blend 'from' and 'to' canvases into 'dst' based on the
/// normalized progress value alpha (0.0 = fully from, 1.0 = fully to).
class TransitionEffect {
public:
    virtual ~TransitionEffect() = default;

    virtual std::string get_name() const = 0;

    /// Apply the transition.
    /// @param dst    Target canvas to write the blended frame into.
    /// @param from   The outgoing scene canvas (fully rendered).
    /// @param to     The incoming scene canvas (fully rendered).
    /// @param alpha  Progress in [0.0, 1.0].
    /// @param width  Canvas width in pixels.
    /// @param height Canvas height in pixels.
    virtual void apply(FrameCanvas *dst, FrameCanvas *from, FrameCanvas *to,
                       float alpha, int width, int height) = 0;
};
