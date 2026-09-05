#pragma once

#include <cstdint>
#include <string_view>

class VisualJourney {
public:
    struct Frame {
        std::string_view phase;
        std::string_view motif;
        float progress;
        float phase_progress;
        float intensity;
        float motion;
        float dwell_scale;
    };

    explicit VisualJourney(std::uint64_t seed = 1);
    void advance(std::uint64_t elapsed_ms);
    [[nodiscard]] Frame frame() const;
    [[nodiscard]] std::uint64_t cycle() const { return cycle_; }
    [[nodiscard]] std::uint64_t duration_ms() const { return duration_ms_; }
    [[nodiscard]] std::uint64_t elapsed_ms() const { return elapsed_ms_; }

private:
    std::uint64_t seed_;
    std::uint64_t duration_ms_;
    std::uint64_t elapsed_ms_ = 0;
    std::uint64_t cycle_ = 0;
};
