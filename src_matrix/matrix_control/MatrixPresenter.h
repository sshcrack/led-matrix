#pragma once

#include "led-matrix.h"

#ifdef ENABLE_EMULATOR
#include "emulator.h"
#endif

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

#ifdef ENABLE_EMULATOR
class EmulatorPresenter : public MatrixPresenter {
    rgb_matrix::RGBMatrixBase *matrix_;
public:
    explicit EmulatorPresenter(rgb_matrix::RGBMatrixBase *matrix)
        : matrix_(matrix) {}
    void present() override {
        static_cast<rgb_matrix::EmulatorMatrix *>(matrix_)->Render();
    }
};
#endif
