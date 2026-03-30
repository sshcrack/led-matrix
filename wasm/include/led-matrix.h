#pragma once
// WASM shim: led-matrix.h replacement for browser-side preview builds.
// Provides FrameCanvas backed by a flat RGBA pixel buffer and stub matrix types.

#include <cstdint>
#include <cstring>
#include <vector>
#include <string>

#include "canvas.h"
#include "thread.h"
#include "pixel-mapper.h"
#include "graphics.h"

namespace rgb_matrix {

/// Concrete canvas implementation for WASM builds.
/// Pixel data is stored as a flat RGBA byte buffer (4 bytes per pixel).
/// JavaScript reads the buffer via wasm_get_frame_buffer() / wasm_get_buffer_size().
class FrameCanvas : public Canvas {
public:
    FrameCanvas() : width_(0), height_(0) {}
    FrameCanvas(int w, int h) : width_(w), height_(h), buffer_(w * h * 4, 0) {}

    // Canvas interface
    int width()  const override { return width_; }
    int height() const override { return height_; }

    void SetPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) override {
        if (x < 0 || x >= width_ || y < 0 || y >= height_) return;
        const int idx = (y * width_ + x) * 4;
        buffer_[idx + 0] = r;
        buffer_[idx + 1] = g;
        buffer_[idx + 2] = b;
        buffer_[idx + 3] = 255;
    }

    void Clear() override {
        std::fill(buffer_.begin(), buffer_.end(), 0);
    }

    void Fill(uint8_t r, uint8_t g, uint8_t b) override {
        for (int i = 0; i < width_ * height_; ++i) {
            buffer_[i * 4 + 0] = r;
            buffer_[i * 4 + 1] = g;
            buffer_[i * 4 + 2] = b;
            buffer_[i * 4 + 3] = 255;
        }
    }

    // WASM-specific: direct buffer access
    const uint8_t *data()  const { return buffer_.data(); }
    uint8_t       *data()        { return buffer_.data(); }
    size_t         size()  const { return buffer_.size(); }

private:
    int width_, height_;
    std::vector<uint8_t> buffer_;
};

// ---------------------------------------------------------------------------
// Stub types required by shared headers but unused in WASM scene rendering
// ---------------------------------------------------------------------------

struct RuntimeOptions {};

class RGBMatrixBase : public Canvas {
public:
    ~RGBMatrixBase() override = default;
    int width()  const override { return 0; }
    int height() const override { return 0; }
    void SetPixel(int, int, uint8_t, uint8_t, uint8_t) override {}
    void Clear() override {}
    void Fill(uint8_t, uint8_t, uint8_t) override {}

    virtual FrameCanvas *CreateFrameCanvas() { return nullptr; }
    virtual FrameCanvas *SwapOnVSync(FrameCanvas *other, unsigned = 1) { return other; }
    virtual bool SetPWMBits(uint8_t) { return false; }
    virtual uint8_t pwmbits() { return 0; }
    virtual void SetBrightness(uint8_t) {}
    virtual uint8_t brightness() { return 100; }
    virtual void set_luminance_correct(bool) {}
    virtual bool luminance_correct() const { return false; }
    virtual bool StartRefresh() { return false; }
};

class RGBMatrix : public RGBMatrixBase {
public:
    struct Options {
        Options() : rows(32), cols(32), chain_length(1), parallel(1),
                    hardware_mapping("regular") {}
        int rows, cols, chain_length, parallel;
        const char *hardware_mapping;
        bool Validate(std::string *) const { return true; }
    };

    static RGBMatrix *CreateMatrixFromOptions(const Options &, const RuntimeOptions &) {
        return nullptr;
    }
};

// StreamWriter stub
class StreamWriter {
public:
    StreamWriter() = default;
};

} // namespace rgb_matrix
