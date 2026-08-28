#pragma once

#include <string>
#include <atomic>
#include "shared/matrix/post_processor.h"
#include "shared/matrix/transition_manager.h"
#include "shared/matrix/export.h"

namespace Constants {
    extern SHARED_MATRIX_API std::atomic<int> width;
    extern SHARED_MATRIX_API std::atomic<int> height;
    extern SHARED_MATRIX_API PostProcessor* global_post_processor;
    extern SHARED_MATRIX_API TransitionManager* global_transition_manager;
}
