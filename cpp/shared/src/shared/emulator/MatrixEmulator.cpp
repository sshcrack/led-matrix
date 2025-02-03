//
// Created by hendrik on 11/14/24.
//
#include "shared/emulator/MatrixEmulator.h"

MatrixEmulator *MatrixEmulator::CreateFromOptions(const rgb_matrix::RGBMatrix::Options &options,
                                                  [[maybe_unused]] const rgb_matrix::RuntimeOptions &runtime_options) {
    return new MatrixEmulator(options);
}

void MatrixEmulator::SetPixel(int x, int y, uint8_t red, uint8_t green, uint8_t blue) {
    CanvasEmulator::SetPixel(x, y, red, green, blue);
}

void MatrixEmulator::Clear() {
    CanvasEmulator::Clear();
}

void MatrixEmulator::Fill(uint8_t red, uint8_t green, uint8_t blue) {
    CanvasEmulator::Fill(red, green, blue);
}

FrameCanvasEmulator *MatrixEmulator::CreateFrameCanvas() {
    return nullptr;
}

