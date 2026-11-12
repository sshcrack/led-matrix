#pragma once

#include <algorithm>
#include <chrono>
#include <optional>

/// How relevant this desktop is as audio producer (0..100), from what the
/// capture hears and what the OS media sessions say.
///
/// A single capture read is a poor verdict: quiet intros, breakdowns and the
/// gap between tracks read as silence, so the score used to flip between 0
/// and 100 while music played. Audible audio therefore holds its score for
/// `AudibleHold`, and a media session reporting "Playing" keeps a high score
/// on its own (also when capture is unavailable). It stays below audible audio
/// so that a desktop which can actually be heard still wins a tie.
class DesktopRelevanceTracker {
public:
    using clock = std::chrono::steady_clock;

    static constexpr double AudibleRms = 0.003; // about -50 dBFS
    static constexpr auto AudibleHold = std::chrono::seconds(10);
    static constexpr int AudibleScore = 100;
    static constexpr int MediaPlayingScore = 90;

    void note_level(const double rms, const clock::time_point now) {
        if (rms > AudibleRms) lastAudible_ = now;
    }

    [[nodiscard]] bool audible(const clock::time_point now) const {
        return lastAudible_ && now - *lastAudible_ < AudibleHold;
    }

    [[nodiscard]] int score(const clock::time_point now, const bool mediaPlaying) const {
        return std::max(audible(now) ? AudibleScore : 0, mediaPlaying ? MediaPlayingScore : 0);
    }

    void reset() { lastAudible_.reset(); }

private:
    std::optional<clock::time_point> lastAudible_;
};
