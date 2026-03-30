#pragma once
// WASM shim: minimal graphics.h replacing rpi-rgb-led-matrix graphics.h
// Provides rgb_matrix::Color and stub drawing functions.

#include <cstdint>
#include "canvas.h"

namespace rgb_matrix {

struct Color {
    uint8_t r, g, b;
    Color() : r(0), g(0), b(0) {}
    Color(uint8_t r, uint8_t g, uint8_t b) : r(r), g(g), b(b) {}
};

// Stub drawing functions - not used by WASM-targeted scenes but needed to satisfy headers
inline void DrawLine(Canvas *, int, int, int, int, const Color &) {}
inline void DrawCircle(Canvas *, int, int, int, const Color &) {}

// Font/text rendering stubs
struct Font {
    bool LoadFont(const char *) { return false; }
    int height() const { return 0; }
    int baseline() const { return 0; }
    int CharacterWidth(uint32_t) const { return 0; }
};

inline int DrawText(Canvas *, const Font &, int, int, const Color &, const char *, int = 0) { return 0; }
inline int VerticalScrollText(Canvas *, const Font &, int, int, int, const Color &, const char *, int = 0) { return 0; }
inline int DrawGlyph(Canvas *, const Font &, int, int, const Color &, const Color *, uint32_t) { return 0; }

} // namespace rgb_matrix
