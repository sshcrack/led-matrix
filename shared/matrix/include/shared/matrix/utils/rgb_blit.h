#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "led-matrix.h"

// Full-frame presentation through the panel library's bulk RGB888 write.
//
// Scenes that render into their own buffer should present it with one
// SetRGB() call instead of a SetPixel() per pixel: on the Pi the bulk path
// groups pixels by GPIO word and costs roughly 40% less per frame.
namespace RgbBlit {
    [[nodiscard]] inline std::uint8_t from_unit(float v)
    {
        return static_cast<std::uint8_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f);
    }

    [[nodiscard]] inline std::uint8_t from_byte(float v)
    {
        return static_cast<std::uint8_t>(std::clamp(v, 0.0f, 255.0f));
    }

    // Fills `scratch` (reused between frames) row by row through
    // `pixel(x, y, out)`, which writes out[0..2] = R, G, B, then writes the
    // w×h frame at the canvas origin.
    template <class PixelFn>
    void present(rgb_matrix::FrameCanvas *canvas, int w, int h, std::vector<std::uint8_t> &scratch, PixelFn &&pixel)
    {
        if (w <= 0 || h <= 0) return;
        scratch.resize(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 3);
        std::uint8_t *out = scratch.data();
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x, out += 3)
                pixel(x, y, out);
        canvas->SetRGB(0, 0, w, h, scratch.data());
    }
}
