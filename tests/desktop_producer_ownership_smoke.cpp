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

    std::cout << "desktop producer ownership stays sticky across competing reconnects\n";
    return 0;
}
