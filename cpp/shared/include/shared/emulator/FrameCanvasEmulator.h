#pragma once

#include "CanvasEmulator.h"

class FrameCanvasEmulator : public CanvasEmulator {
public:
    void SetBrightness(uint8_t brightness) {}
    uint8_t brightness() { return 0; }
};