// Producer relevance of a desktop with audio: it must not flip to 0 in quiet
// passages or between tracks, and a playing media session counts on its own.
#include "DesktopRelevance.h"

#include <cstdio>

namespace {
int failures = 0;
void expect(const bool ok, const char *what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}
} // namespace

int main() {
    using namespace std::chrono_literals;
    using T = DesktopRelevanceTracker;
    const auto t0 = T::clock::time_point{} + 1h;

    T r;
    expect(r.score(t0, false) == 0, "silent and nothing playing scores 0");
    expect(r.score(t0, true) == T::MediaPlayingScore, "a playing media session counts without audio");

    r.note_level(0.05, t0);
    expect(r.score(t0, false) == T::AudibleScore, "audible audio scores 100");
    r.note_level(0.0, t0 + 2s); // quiet passage / gap between tracks
    expect(r.score(t0 + 2s, false) == T::AudibleScore, "a quiet read does not drop the score");
    expect(r.score(t0 + T::AudibleHold - 1ms, false) == T::AudibleScore, "audible holds for AudibleHold");
    expect(r.score(t0 + T::AudibleHold, false) == 0, "long silence drops the score");
    expect(r.score(t0 + T::AudibleHold, true) == T::MediaPlayingScore, "media keeps it high through long quiet");

    r.note_level(T::AudibleRms / 2, t0 + 30s);
    expect(r.score(t0 + 30s, false) == 0, "noise floor is not audio");

    r.note_level(0.05, t0 + 40s);
    expect(r.score(t0 + 40s, true) == T::AudibleScore, "audible audio beats a media session");
    r.reset();
    expect(r.score(t0 + 40s, false) == 0, "reset forgets audio");

    if (failures) return 1;
    std::puts("desktop relevance OK");
    return 0;
}
