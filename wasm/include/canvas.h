#pragma once
// WASM shim: minimal Canvas base class replacing rpi-rgb-led-matrix canvas.h

#include <cstdint>

namespace rgb_matrix {

class Canvas {
public:
    virtual ~Canvas() = default;
    virtual int width() const = 0;
    virtual int height() const = 0;
    virtual void SetPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) = 0;
    virtual void Clear() = 0;
    virtual void Fill(uint8_t r, uint8_t g, uint8_t b) = 0;
};

} // namespace rgb_matrix
