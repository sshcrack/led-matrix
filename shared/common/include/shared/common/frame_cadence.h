#pragma once

#include <algorithm>
#include <cstdint>

// Absolute, fractional frame deadlines. Quantization/jitter in a presentation
// must not become part of every subsequent frame's interval.
class FrameCadence {
public:
    FrameCadence(std::int64_t start_us, int fps)
        : start_us_(start_us), fps_(std::clamp(fps, 1, 1000)) {}

    std::int64_t deadline_us() const {
        return start_us_ + (index_ * 1000000 + fps_ - 1) / fps_;
    }

    void frame_started(std::int64_t now_us) {
        ++index_;
        // Skip missed slots after a stall; do not render a burst to catch up.
        if (deadline_us() <= now_us)
            index_ = (std::max<std::int64_t>(0, now_us - start_us_) * fps_) / 1000000 + 1;
    }

private:
    std::int64_t start_us_;
    std::int64_t index_ = 0;
    int fps_;
};
