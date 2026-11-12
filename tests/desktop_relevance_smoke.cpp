// Producer relevance of a desktop with audio: it must not flip to 0 in quiet
// passages or between tracks, and a playing media session counts on its own.
#include <cstdio>

#include "DesktopRelevance.h"

namespace {
int failures = 0;
void expect(const bool ok, const char* what)
{
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}
}  // namespace

int main()
{
    using namespace std::chrono_literals;
    using T = DesktopRelevanceTracker;
    const auto t0 = T::clock::time_point{} + 1h;

    T r;
    expect(r.score(t0, false) == 0, "silent and nothing playing scores 0");
    expect(r.score(t0, true) == T::MediaPlayingScore, "a playing media session counts while capture is unavailable");

    r.note_level(0.05, t0);
    expect(r.score(t0, false) == T::AudibleScore, "audible audio scores 100");
    r.note_level(0.0, t0 + 2s);  // quiet passage / gap between tracks
    expect(r.score(t0 + 2s, false) == T::AudibleScore, "a quiet read does not drop the score");
    expect(r.score(t0 + T::AudibleHold - 1ms, false) == T::AudibleScore, "audible holds for AudibleHold");
    r.note_level(0.0, t0 + T::AudibleHold);
    expect(r.score(t0 + T::AudibleHold, false) == 0, "long silence drops the score");
    expect(r.score(t0 + T::AudibleHold, true) == T::MediaPlayingScore, "media bridges a long quiet stretch");
    r.note_level(0.0, t0 + T::MediaBridge);
    expect(r.score(t0 + T::MediaBridge, true) == 0, "media no longer bridges after MediaBridge of silence");

    // Spotify Connect mirror: the session says Playing, the capture works but
    // never hears anything on this PC.
    T mirror;
    for (auto t = 0s; t <= 30s; t += 2s)
        mirror.note_level(0.0, t0 + t);
    expect(mirror.score(t0 + 30s, true) == 0, "a mirrored session on a silent PC scores 0");
    expect(mirror.score(t0 + 30s + T::CaptureStale, true) == T::MediaPlayingScore,
           "media counts again once capture stops delivering");

    r.note_level(T::AudibleRms / 2, t0 + 90s);
    expect(r.score(t0 + 90s, false) == 0, "noise floor is not audio");

    r.note_level(0.05, t0 + 100s);
    expect(r.score(t0 + 100s, true) == T::AudibleScore, "audible audio beats a media session");
    r.reset();
    expect(r.score(t0 + 100s, false) == 0, "reset forgets audio");

    if (failures)
        return 1;
    std::puts("desktop relevance OK");
    return 0;
}
