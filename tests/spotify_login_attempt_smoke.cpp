#include "manager/login_attempt.h"

#include <iostream>

using namespace SpotifyLogin;

namespace {
int failures = 0;
void expect(bool ok, const char* name) {
    if (!ok) {
        std::cerr << "FAIL: " << name << '\n';
        ++failures;
    }
}
Pending pending(const char* state) { return {state, std::string("verifier-") + state, "https://relay/callback"}; }
} // namespace

int main() {
    const auto t0 = Clock::now();

    { // Fresh daemon: nothing to report.
        Attempt a;
        const auto s = a.snapshot(t0);
        expect(s.attempt == 0 && s.phase == Phase::Idle, "starts idle without an attempt id");
        expect(!a.take("x", t0), "nothing to take before begin");
    }
    { // Happy path: waiting -> exchanging -> connected, secrets handed over exactly once.
        Attempt a;
        const auto id = a.begin(pending("s1"), t0);
        expect(id == 1 && a.snapshot(t0).phase == Phase::Waiting, "begin waits");
        expect(!a.take("other", t0), "foreign state rejected");
        expect(a.snapshot(t0).phase == Phase::Waiting, "foreign state leaves the attempt waiting");
        auto taken = a.take("s1", t0);
        expect(taken && taken->verifier == "verifier-s1" && taken->redirect == "https://relay/callback", "take hands over secrets");
        expect(a.snapshot(t0).phase == Phase::Exchanging, "exchanging after take");
        expect(!a.take("s1", t0), "state is one-time");
        a.succeed();
        const auto s = a.snapshot(t0);
        expect(s.attempt == id && s.phase == Phase::Connected && s.error.empty(), "connected");
    }
    { // A declined/failed exchange is reported with its message.
        Attempt a;
        a.begin(pending("s1"), t0);
        expect(a.take("s1", t0).has_value(), "take before failing");
        a.fail("declined");
        const auto s = a.snapshot(t0);
        expect(s.phase == Phase::Failed && s.error == "declined", "failure message reported");
        a.succeed();
        expect(a.snapshot(t0).phase == Phase::Failed, "a finished attempt cannot be finished again");
    }
    { // Expiry is visible before anyone comes back, and a late callback records it.
        Attempt a;
        a.begin(pending("s1"), t0);
        const auto late = t0 + kLifetime + std::chrono::seconds(1);
        expect(a.snapshot(late).phase == Phase::Expired, "expired is derived from time");
        expect(!a.snapshot(late).error.empty(), "expired carries a message");
        expect(!a.take("s1", late), "late callback rejected");
        expect(a.snapshot(t0).phase == Phase::Expired, "late callback consumes the attempt");
    }
    { // A new attempt supersedes the old one: the old state can no longer finish.
        Attempt a;
        const auto first = a.begin(pending("s1"), t0);
        const auto second = a.begin(pending("s2"), t0);
        expect(second == first + 1, "attempt ids increase");
        expect(!a.take("s1", t0), "superseded state rejected");
        expect(a.take("s2", t0).has_value(), "current state accepted");
    }
    { // Changing the Client ID cancels a waiting attempt, but leaves a finished one alone.
        Attempt a;
        a.begin(pending("s1"), t0);
        a.cancel();
        expect(a.snapshot(t0).phase == Phase::Failed, "cancel fails the waiting attempt");
        expect(!a.take("s1", t0), "cancelled state rejected");

        Attempt done;
        done.begin(pending("s1"), t0);
        expect(done.take("s1", t0).has_value(), "take before succeeding");
        done.succeed();
        done.cancel();
        expect(done.snapshot(t0).phase == Phase::Connected, "cancel keeps a connected result");
    }
    expect(to_string(Phase::Exchanging) == "exchanging", "phase names are the wire format");
    return failures ? 1 : 0;
}
