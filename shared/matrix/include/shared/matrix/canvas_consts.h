#pragma once

#include <string>
#include <atomic>
#include "shared/matrix/post_processor.h"
#include "shared/matrix/transition_manager.h"

namespace Constants {
    extern std::atomic<int> width;
    extern std::atomic<int> height;
    extern PostProcessor* global_post_processor;
    extern TransitionManager* global_transition_manager;

    /// If this bool is true, SleepMillis and wait_until_next_frame will not do anything and render directly.
    extern bool isRenderingSceneInitially;
}