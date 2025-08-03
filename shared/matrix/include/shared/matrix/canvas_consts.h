#pragma once

#include <string>
#include <atomic>

namespace Constants {
    extern std::atomic<int> width;
    extern std::atomic<int> height;

    /// If this bool is true, SleepMillis and wait_until_next_frame will not do anything and render directly.
    extern bool isRenderingSceneInitially;
}