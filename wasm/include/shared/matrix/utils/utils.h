#pragma once
// WASM override: shared/matrix/utils/utils.h without Magick++ / execute_process / magick_to_rgb.
// These are not needed for pure-canvas scene rendering in WASM.

#include "shared/common/utils/utils.h"
#include "led-matrix.h"
#include <optional>
#include <vector>

// Timing helpers (implemented in wasm/runtime/wasm_stubs.cpp)
void SleepMillis(tmillis_t milli_seconds);

// Canvas helpers (implemented in wasm/runtime/wasm_stubs.cpp)
void floatPixelSet(rgb_matrix::FrameCanvas *canvas, int x, int y, float r, float g, float b);
void SetPixelAlpha(rgb_matrix::FrameCanvas *canvas, int x, int y,
                   uint8_t r, uint8_t g, uint8_t b, float alpha);
