#include "plugins/SpotifyMV/desktop/SpotifyMVTrackRequests.h"

#include <iostream>

namespace {
int failures = 0;
void expect(bool ok, const char *what)
{
    if (!ok) {
        std::cerr << "FAIL: " << what << '\n';
        ++failures;
    }
}
using Outcome = SpotifyMVTrackRequests::Outcome;
}

int main()
{
    SpotifyMVTrackRequests r;
    expect(r.status_generation() == 0, "idle forwards no status");

    const auto a = r.request("A");
    expect(a.outcome == Outcome::NewRequest && r.is_pending(a.generation, "A"), "first track becomes pending");
    expect(r.status_generation() == a.generation, "pending engine status is forwarded");
    expect(r.request("A").outcome == Outcome::AlreadyPending, "duplicate pending request is deduplicated");
    expect(!r.promote(a.generation, "B"), "promotion needs the matching track");
    expect(r.promote(a.generation, "A") && r.current()->track_id == "A" && !r.pending(), "first frame promotes");
    expect(r.request("A").outcome == Outcome::AlreadyCurrent, "current track is deduplicated");

    const auto b = r.request("B");
    expect(b.outcome == Outcome::NewRequest && b.generation > a.generation, "next track gets a newer generation");
    expect(r.status_generation() == b.generation, "while B loads only B's status is forwarded");
    const auto c = r.request("C");
    expect(!r.is_pending(b.generation), "C supersedes B's search");
    expect(!r.promote(b.generation, "B"), "superseded first frame cannot promote");

    expect(r.request("A").outcome == Outcome::ReturnToCurrent && !r.pending(), "returning to A cancels C");
    expect(!r.promote(c.generation, "C"), "cancelled request cannot promote");
    // Regression: the old single request counter left A's engine muted after
    // a skip-and-return, so its later download/error status never reached the Pi.
    expect(r.status_generation() == a.generation, "A's status is forwarded again after returning to it");

    // A failed search (no video found) must not leave a stale pending slot:
    // that swallowed every retry of the track as AlreadyPending and kept the
    // current engine's status muted.
    const auto g = r.request("G");
    const auto h = r.request("H");
    expect(!r.abandon(g.generation), "a superseded failure is ignored");
    expect(r.abandon(h.generation) && !r.pending() && r.current()->track_id == "A",
           "a failed request drops only the pending slot");
    expect(r.status_generation() == a.generation, "current status is forwarded again after a failure");
    const auto retry = r.request("H");
    expect(retry.outcome == Outcome::NewRequest && retry.generation > h.generation,
           "a failed track can be requested again");
    r.abandon(retry.generation);
    expect(r.request("A").outcome == Outcome::AlreadyCurrent, "after a failure the current track stays current");

    r.clear();
    expect(!r.current() && !r.pending() && r.status_generation() == 0, "stop clears both slots");
    expect(r.request("A").outcome == Outcome::NewRequest, "after stop the old current track is requested anew");

    if (failures == 0) std::cout << "SpotifyMV track request transitions OK\n";
    return failures == 0 ? 0 : 1;
}
