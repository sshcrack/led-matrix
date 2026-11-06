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

    r.clear();
    expect(!r.current() && !r.pending() && r.status_generation() == 0, "stop clears both slots");
    expect(r.request("A").outcome == Outcome::NewRequest, "after stop the old current track is requested anew");

    if (failures == 0) std::cout << "SpotifyMV track request transitions OK\n";
    return failures == 0 ? 0 : 1;
}
