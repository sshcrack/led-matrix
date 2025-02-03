#pragma once

#include "led-matrix.h"
#include "CanvasEmulator.h"
#include "FrameCanvasEmulator.h"

using Options = rgb_matrix::RGBMatrix::Options;

//TODO: Implement this class
class MatrixEmulator : public CanvasEmulator {
public:
    explicit MatrixEmulator(const Options &options) : CanvasEmulator(options.rows, options.cols) {

    }

    static MatrixEmulator *CreateFromOptions(const Options &options,
                                             [[maybe_unused]] const rgb_matrix::RuntimeOptions &runtime_options);

    void SetPixel(int x, int y, uint8_t red, uint8_t green, uint8_t blue) override;

    void Clear() override;

    void Fill(uint8_t red, uint8_t green, uint8_t blue) override;

    FrameCanvasEmulator *CreateFrameCanvas();

};