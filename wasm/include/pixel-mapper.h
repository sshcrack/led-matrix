#pragma once
// WASM shim: minimal pixel-mapper.h replacing rpi-rgb-led-matrix pixel-mapper.h
// Pixel mapping is not needed for WASM scene rendering.

#include <string>
#include <vector>

namespace rgb_matrix {

class Canvas;

// Stub pixel mapper interface
class PixelMapper {
public:
    virtual ~PixelMapper() = default;
    virtual const char *GetName() const = 0;
    virtual bool SetParameters(int, int, const char *) { return true; }
    virtual bool GetSizeMapping(int, int, int *, int *) const { return false; }
    virtual void MapVisibleToMatrix(int, int, int, int, int *, int *) const {}
};

} // namespace rgb_matrix
