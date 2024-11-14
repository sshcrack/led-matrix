//
// Created by hendrik on 11/14/24.
//
#include "shared/emulator/MatrixEmulator.h"

MatrixEmulator *MatrixEmulator::CreateFromOptions(const rgb_matrix::RGBMatrix::Options &options,
                                                  [[maybe_unused]] const rgb_matrix::RuntimeOptions &runtime_options) {
    return new MatrixEmulator(options);
}

