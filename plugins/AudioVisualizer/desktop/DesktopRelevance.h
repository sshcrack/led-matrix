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
/// `AudibleHold`.
///
/// A media session reporting "Playing" is no proof on its own: Spotify shows
/// playback running on another device (Spotify Connect) as "Playing" on every
/// PC signed in to the account. It bridges longer quiet stretches only while
/// this PC heard audio within `MediaBridge`, and counts alone only when the
/// capture delivers nothing (unavailable). It stays below audible audio so
/// that a desktop which can actually be heard still wins a tie.
class DesktopRelevanceTracker {
public:
    using clock = std::chrono::steady_clock;

    static constexpr double AudibleRms = 0.003;  // about -50 dBFS
    static constexpr auto AudibleHold = std::chrono::seconds(10);
    static constexpr auto MediaBridge = std::chrono::seconds(60);
    /// Capture counts as working while it delivered a level this recently.
    static constexpr auto CaptureStale = std::chrono::seconds(6);
    static constexpr int AudibleScore = 100;
    static constexpr int MediaPlayingScore = 90;

    void note_level(const double rms, const clock::time_point now)
    {
        lastLevel_ = now;
        if (rms > AudibleRms)
            lastAudible_ = now;
    }

    [[nodiscard]] bool audible(const clock::time_point now) const { return lastAudible_ && now - *lastAudible_ < AudibleHold; }

    [[nodiscard]] bool capturing(const clock::time_point now) const { return lastLevel_ && now - *lastLevel_ < CaptureStale; }

    /// Whether a playing media session counts here: no working capture to
    /// contradict it, or this PC heard audio recently.
    [[nodiscard]] bool media_trusted(const clock::time_point now) const
    {
        return !capturing(now) || (lastAudible_ && now - *lastAudible_ < MediaBridge);
    }

    [[nodiscard]] int score(const clock::time_point now, const bool mediaPlaying) const
    {
        if (audible(now))
            return AudibleScore;
        return mediaPlaying && media_trusted(now) ? MediaPlayingScore : 0;
    }

    void reset()
    {
        lastAudible_.reset();
        lastLevel_.reset();
    }

private:
    std::optional<clock::time_point> lastAudible_;
    std::optional<clock::time_point> lastLevel_;
};
