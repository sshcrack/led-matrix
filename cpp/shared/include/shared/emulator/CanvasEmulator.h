#pragma once

#include "canvas.h"

class CanvasEmulator : public rgb_matrix::Canvas {
public:
    CanvasEmulator(int width, int height) {
        _width = width;
        _height = height;

        buffer = new uint8_t[width * height * 3];
    }

    void SetBrightness(uint8_t brightness) {};

    static uint8_t brightness() { return 255; };

    [[nodiscard]] int width() const override { return _width; };

    [[nodiscard]] int height() const override { return _height; };

    void SetPixel(int x, int y, uint8_t red, uint8_t green, uint8_t blue) override {};

    void Clear() override {};

    void Fill(uint8_t red, uint8_t green, uint8_t blue) override {};
private:
    int _height;
    int _width;

    uint8_t *buffer;
};