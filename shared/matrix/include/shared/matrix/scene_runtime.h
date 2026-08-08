#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>

namespace Scenes {

struct SceneFrameContext {
    double delta_seconds = 0.0;
    double elapsed_seconds = 0.0;
    std::uint64_t frame_index = 0;
    std::uint64_t now_ms = 0;
    bool deterministic = false;
};

/// Helper for simulations that should advance at a fixed physics frequency
/// independently from render FPS. It deliberately drops excessive backlog
/// after max_steps to avoid a spiral-of-death after a stall/debugger pause.
class FixedStepAccumulator {
public:
    explicit FixedStepAccumulator(double hz = 60.0, int max_steps = 4)
        : step_seconds_(1.0 / std::max(1.0, hz)), max_steps_(std::max(1, max_steps)) {}

    void configure(double hz, int max_steps = 4) {
        step_seconds_ = 1.0 / std::max(1.0, hz);
        max_steps_ = std::max(1, max_steps);
        accumulator_ = std::fmod(accumulator_, step_seconds_);
    }

    void reset() { accumulator_ = 0.0; }

    template <typename Fn>
    int advance(double delta_seconds, Fn &&step) {
        accumulator_ += std::clamp(delta_seconds, 0.0, 0.25);
        int steps = 0;
        while (accumulator_ >= step_seconds_ && steps < max_steps_) {
            std::invoke(step, step_seconds_);
            accumulator_ -= step_seconds_;
            ++steps;
        }
        if (steps == max_steps_ && accumulator_ >= step_seconds_)
            accumulator_ = std::fmod(accumulator_, step_seconds_);
        return steps;
    }

    [[nodiscard]] double alpha() const {
        return step_seconds_ > 0.0 ? std::clamp(accumulator_ / step_seconds_, 0.0, 1.0) : 0.0;
    }

    [[nodiscard]] double step_seconds() const { return step_seconds_; }

private:
    double step_seconds_ = 1.0 / 60.0;
    double accumulator_ = 0.0;
    int max_steps_ = 4;
};

struct SceneCapabilities {
    bool requires_desktop = false;
    bool requires_audio = false;
    bool requires_network = false;
    bool interactive = false;
    bool can_generate_preview = true;
    bool supports_audio = false;
    bool music_director_eligible = true;
};

} // namespace Scenes
