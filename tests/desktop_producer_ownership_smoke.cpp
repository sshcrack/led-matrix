#include <shared/common/desktop_control_protocol.h>
#include <shared/matrix/server/common.h>

#include <cstdint>
#include <iostream>

namespace {
bool expect_owner(std::uint64_t expected, const char *label)
{
    const auto actual = Server::desktop_producer_owner();
    if (actual == expected)
        return true;
    std::cerr << label << ": expected owner " << expected << ", got " << actual << '\n';
    return false;
}
}

int main()
{
    Server::clear_desktop_producers();

    auto change = Server::register_desktop_producer(10, "desktop-a");
    if (!change.changed || change.owner != 10 || !expect_owner(10, "first desktop"))
        return 1;

    // A second healthy desktop is a standby controller. Merely connecting must
    // not steal the physical UDP producer role.
    change = Server::register_desktop_producer(20, "desktop-b");
    if (change.changed || change.owner != 10 || !expect_owner(10, "second desktop"))
        return 2;

    Server::register_desktop_worker(11, "desktop-a");
    Server::register_desktop_worker(21, "desktop-b");
    if (!Server::accepts_desktop_producer_message(11)
        || Server::accepts_desktop_producer_message(21)) {
        std::cerr << "scene workers were not paired to the elected desktop producer\n";
        return 7;
    }
    const auto initial_targets = Server::desktop_producer_targets();
    if (initial_targets.size() != 2 || initial_targets[0] != 10 || initial_targets[1] != 11) {
        std::cerr << "exclusive plugin targets included a standby desktop worker\n";
        return 8;
    }

    // Additional transports from another standby desktop also stay standby.
    change = Server::register_desktop_producer(30, "desktop-b");
    if (change.changed || change.owner != 10 || !expect_owner(10, "standby reconnect"))
        return 3;

    // An automatic reconnect from the current logical owner supersedes only its
    // own stale socket immediately.
    change = Server::register_desktop_producer(40, "desktop-a");
    if (!change.changed || change.previous_owner != 10 || change.owner != 40
        || !expect_owner(40, "owner reconnect"))
        return 4;
    if (!Server::accepts_desktop_producer_message(11)
        || Server::accepts_desktop_producer_message(21)) {
        std::cerr << "owner reconnect lost its paired worker identity\n";
        return 9;
    }

    // Closing the superseded stale transport must not change ownership.
    change = Server::unregister_desktop_producer(10);
    if (change.changed || !expect_owner(40, "stale close"))
        return 5;

    // When the active desktop really disconnects, promote a remaining standby.
    change = Server::unregister_desktop_producer(40);
    if (!change.changed || change.owner != 30 || !expect_owner(30, "owner close"))
        return 6;
    if (Server::accepts_desktop_producer_message(11)
        || !Server::accepts_desktop_producer_message(21)) {
        std::cerr << "worker ownership did not follow promoted desktop\n";
        return 10;
    }

    Server::unregister_desktop_worker(11);
    Server::unregister_desktop_worker(21);
    Server::unregister_desktop_producer(30);
    Server::unregister_desktop_producer(20);
    Server::clear_desktop_producers();

    // ---- relevance election ----
    Server::clear_desktop_producers();
    namespace S = Server;
    S::register_desktop_producer(1, "laptop");
    S::register_desktop_producer(2, "winpc");
    constexpr std::int64_t T = 100000;
    // Equal relevance never moves the sticky owner.
    if (S::report_desktop_relevance(1, 0, T).changed || S::report_desktop_relevance(2, 0, T).changed
        || !expect_owner(1, "equal relevance"))
        return 20;
    // winpc has audio: it must stay ahead for the hold time before it takes over.
    if (S::report_desktop_relevance(2, 100, T + 1000).changed || !expect_owner(1, "challenger starts"))
        return 21;
    if (S::report_desktop_relevance(2, 100, T + 1000 + S::DesktopRelevanceHoldMs - 1).changed
        || !expect_owner(1, "hold not elapsed"))
        return 22;
    // A dip ends the challenge; the clock restarts.
    S::report_desktop_relevance(2, 0, T + 8000);
    S::report_desktop_relevance(2, 100, T + 9000);
    if (S::report_desktop_relevance(2, 100, T + 9000 + S::DesktopRelevanceHoldMs - 1).changed)
        return 23;
    auto moved = S::report_desktop_relevance(2, 100, T + 9000 + S::DesktopRelevanceHoldMs);
    if (!moved.changed || moved.previous_owner != 1 || moved.owner != 2 || !expect_owner(2, "takeover"))
        return 24;
    // Reports from unknown connections are ignored; a margin below 20 does not flip.
    if (S::report_desktop_relevance(99, 100, T + 40000).changed)
        return 25;
    S::report_desktop_relevance(1, 90, T + 40000);
    S::report_desktop_relevance(2, 100, T + 40000);
    if (S::report_desktop_relevance(1, 90, T + 60000).changed || !expect_owner(2, "within margin"))
        return 26;
    // A stale owner report counts as 0: the laptop is then clearly ahead.
    S::report_desktop_relevance(1, 100, T + 70000);
    S::report_desktop_relevance(1, 100, T + 70000 + S::DesktopRelevanceHoldMs + 1);
    if (!expect_owner(1, "stale owner report"))
        return 27;
    // When the owner leaves, the most relevant remaining desktop is promoted.
    S::register_desktop_producer(3, "tablet");
    S::report_desktop_relevance(3, 50, T + 90000);
    S::report_desktop_relevance(2, 0, T + 90000);
    const auto left = S::unregister_desktop_producer(1);
    if (!left.changed || left.owner != 3)
        return 28;
    S::clear_desktop_producers();

    if (DesktopControlProtocol::parse_desktop_relevance("desktop_relevance:42") != 42
        || DesktopControlProtocol::parse_desktop_relevance("desktop_relevance:250") != 100
        || DesktopControlProtocol::parse_desktop_relevance("desktop_relevance:x").has_value()
        || DesktopControlProtocol::parse_desktop_relevance("matrix_enabled:1").has_value()
        || DesktopControlProtocol::desktop_relevance(-5) != "desktop_relevance:0")
        return 29;

    std::cout << "desktop producer ownership stays sticky across competing reconnects\n";
    return 0;
}
