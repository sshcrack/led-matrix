#pragma once
// WASM override: shared/matrix/canvas_consts.h
// Uses forward declarations instead of including post_processor.h and
// transition_manager.h (which have heavy native dependencies).

#include <string>
#include <atomic>

// Forward declare – full types are not needed for the pointer declarations.
class PostProcessor;
class TransitionManager;

namespace Constants {
    extern std::atomic<int> width;
    extern std::atomic<int> height;
    /// Always nullptr in WASM builds.
    extern PostProcessor *global_post_processor;
    /// Always nullptr in WASM builds.
    extern TransitionManager *global_transition_manager;
} // namespace Constants
