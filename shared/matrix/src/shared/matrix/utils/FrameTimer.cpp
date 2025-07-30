//
// Created by hendrik on 3/28/24.
//

#include "shared/matrix/utils/FrameTimer.h"
#include <chrono>


FrameTimer::FrameTimer() {
    startTick = std::chrono::high_resolution_clock::now();
    lastTick = startTick;
}

const FrameTime FrameTimer::tick() {
    auto now = std::chrono::high_resolution_clock::now();

    struct FrameTime frameTime;
    frameTime.now = now;
    frameTime.deltaStart = now - startTick;
    frameTime.deltaFrame = now - lastTick;
    frameTime.t = frameTime.deltaStart.count();
    frameTime.dt = frameTime.deltaFrame.count();

    lastTick = now;
    return frameTime;
}