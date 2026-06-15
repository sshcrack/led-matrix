#pragma once

#include <cstdint>

namespace rgb_matrix {
class FrameCanvas;
}

namespace LoadingAnimation {

void render(rgb_matrix::FrameCanvas *canvas, int frame,
            uint8_t r, uint8_t g, uint8_t b);

}
