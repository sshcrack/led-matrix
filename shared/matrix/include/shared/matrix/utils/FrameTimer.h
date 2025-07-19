#pragma once

#include <chrono>

struct FrameTime {
    std::chrono::high_resolution_clock::time_point now;

    std::chrono::duration<double> deltaStart;
    std::chrono::duration<double> deltaFrame;

    float t = 0;
    float dt = 0;
};

class FrameTimer {
public:
    FrameTimer();

    const FrameTime tick();

private:
    std::chrono::high_resolution_clock::time_point startTick;
    std::chrono::high_resolution_clock::time_point lastTick;
};