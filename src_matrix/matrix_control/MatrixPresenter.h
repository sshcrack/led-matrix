#pragma once

#include "led-matrix.h"

#include "emulator.h"

class MatrixPresenter {
public:
    virtual ~MatrixPresenter() = default;
    virtual void present() = 0;
};

class NoopPresenter : public MatrixPresenter {
public:
    explicit NoopPresenter(rgb_matrix::RGBMatrixBase *) {}
    void present() override {}
};

/// Shows what was drawn straight onto the matrix (no swap, e.g. the "No scene
/// available" fallback) in the emulator. `SwapOnVSync()` shows swapped frames
/// itself (panel library v0.1.16+), so after a swap this skips an already shown
/// frame; on hardware it does nothing.
class EmulatorPresenter : public MatrixPresenter {
    rgb_matrix::RGBMatrixBase *matrix_;
public:
    explicit EmulatorPresenter(rgb_matrix::RGBMatrixBase *matrix)
        : matrix_(matrix) {}
    void present() override {
        if (auto *emulator = matrix_->AsEmulator())
            emulator->Render();
    }
};
