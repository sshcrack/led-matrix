#include "PanelTestScene.h"

#include <algorithm>
#include <cmath>

using namespace Scenes;

bool PanelTestScene::render(rgb_matrix::FrameCanvas* canvas)
{
    const auto frame = timer_.tick();
    const int w = matrix_width;
    const int h = matrix_height;
    canvas->Fill(0, 0, 0);

    // Grid every 32 pixels, dim.
    for (int x = 32; x < w; x += 32)
        for (int y = 0; y < h; ++y) canvas->SetPixel(x, y, 40, 40, 40);
    for (int y = 32; y < h; y += 32)
        for (int x = 0; x < w; ++x) canvas->SetPixel(x, y, 40, 40, 40);

    // Diagonal from top-left to bottom-right.
    const int steps = std::max(w, h);
    for (int i = 0; i < steps; ++i)
        canvas->SetPixel(i * (w - 1) / std::max(1, steps - 1), i * (h - 1) / std::max(1, steps - 1), 200, 0, 200);

    // Grey ramp above the bottom edge.
    const int ramp = std::max(2, h / 16);
    for (int x = 1; x < w - 1; ++x) {
        const auto v = static_cast<uint8_t>(255 * (x - 1) / std::max(1, w - 3));
        for (int y = h - 1 - ramp; y < h - 1; ++y) canvas->SetPixel(x, y, v, v, v);
    }

    // Sweeping line, left to right in 4 s.
    const int sweep = static_cast<int>(std::fmod(frame.t / 4.0, 1.0) * w);
    for (int y = 1; y < h - 1 - ramp; ++y) canvas->SetPixel(sweep, y, 0, 200, 200);

    // Frame around the canvas.
    for (int x = 0; x < w; ++x) {
        canvas->SetPixel(x, 0, 255, 255, 255);
        canvas->SetPixel(x, h - 1, 255, 255, 255);
    }
    for (int y = 0; y < h; ++y) {
        canvas->SetPixel(0, y, 255, 255, 255);
        canvas->SetPixel(w - 1, y, 255, 255, 255);
    }

    // Corner squares.
    const int s = std::max(3, std::min(w, h) / 10);
    auto square = [&](int x0, int y0, uint8_t r, uint8_t g, uint8_t b) {
        for (int y = y0; y < y0 + s; ++y)
            for (int x = x0; x < x0 + s; ++x) canvas->SetPixel(x, y, r, g, b);
    };
    square(2, 2, 255, 0, 0);
    square(w - 2 - s, 2, 0, 255, 0);
    square(2, h - 3 - ramp - s, 0, 0, 255);
    square(w - 2 - s, h - 3 - ramp - s, 255, 200, 0);

    // Arrow at the top centre, pointing up.
    const int cx = w / 2;
    const int size = std::max(3, std::min(w, h) / 8);
    for (int row = 0; row < size; ++row)
        for (int dx = -row; dx <= row; ++dx) canvas->SetPixel(cx + dx, 3 + row, 255, 255, 255);
    for (int y = 3 + size; y < 3 + 2 * size; ++y)
        for (int dx = -size / 4; dx <= size / 4; ++dx) canvas->SetPixel(cx + dx, y, 255, 255, 255);
    return true;
}
