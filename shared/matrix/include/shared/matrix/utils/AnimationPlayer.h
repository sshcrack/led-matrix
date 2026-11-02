#pragma once

#include <content-streamer.h>
#include <led-matrix.h>
#include <algorithm>
#include <cstdint>

// Deadlines advance from the previous frame, rather than from a render tick,
// so a late render does not add latency to every subsequent animation frame.
class AnimationPlayer {
public:
    explicit AnimationPlayer(rgb_matrix::StreamIO *stream) : reader_(stream) {}

    bool render(rgb_matrix::FrameCanvas *canvas, std::int64_t now_ms) {
        std::uint32_t hold_us = 0;
        if (!reader_.PeekNext(canvas, &hold_us)) return false;
        if (!started_) {
            deadline_ms_ = now_ms + duration_ms(hold_us);
            started_ = true;
            return true;
        }
        unsigned advanced = 0;
        while (now_ms >= deadline_ms_ && advanced++ < 256) {
            if (!reader_.GetNext(canvas, nullptr)) return false;
            if (!reader_.PeekNext(canvas, &hold_us)) {
                reader_.Rewind();
                if (!reader_.PeekNext(canvas, &hold_us)) return false;
            }
            deadline_ms_ += duration_ms(hold_us);
        }
        // Bound work after a long pause or pathological zero-delay animation.
        if (now_ms >= deadline_ms_) deadline_ms_ = now_ms + duration_ms(hold_us);
        return true;
    }

private:
    static std::int64_t duration_ms(std::uint32_t hold_us) {
        return std::max<std::int64_t>(1, (static_cast<std::int64_t>(hold_us) + 999) / 1000);
    }
    rgb_matrix::StreamReader reader_;
    bool started_ = false;
    std::int64_t deadline_ms_ = 0;
};
