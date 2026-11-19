// Recovery mode's pure logic: reasons, the panel screen and the helper protocol.
#include <cstdio>
#include <cstdlib>
#include <string>

#include "recovery/recovery_state.h"

using namespace Recovery;

static int failures = 0;
#define CHECK(condition)                                                     \
    do {                                                                     \
        if (!(condition)) {                                                  \
            std::fprintf(stderr, "%s:%d: CHECK(%s)\n", __FILE__, __LINE__, #condition); \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

int main()
{
    CHECK(parse_reason("storage") == Reason::Storage);
    CHECK(parse_reason("crash") == Reason::Crash);
    CHECK(!parse_reason("other"));
    CHECK(reason_id(Reason::Storage) == "storage");

    CHECK(is_helper_action("repair") && is_helper_action("reset") && is_helper_action("retry"));
    CHECK(!is_helper_action("install") && !is_helper_action("repair\n") && !is_helper_action("../x"));

    auto state = parse_action_state(R"({"action":"repair","state":"running","message":"Checking","progress":140})");
    CHECK(state && state->running() && state->progress == 100 && state->message == "Checking");
    CHECK(!parse_action_state(R"({"action":"repair","state":"weird"})"));
    CHECK(!parse_action_state("not json"));
    CHECK(!parse_action_state(R"(["repair"])"));

    CHECK(address_lines("192.168.178.65", 8080, 19) == std::vector<std::string>{"192.168.178.65:8080"});
    // 128 px wide panel with a 7 px font: 18 characters per line.
    CHECK((address_lines("192.168.178.65", 8080, 18) == std::vector<std::string>{"192.168.178.", "65:8080"}));
    // 64 px: 9 characters.
    const auto split = address_lines("192.168.178.65", 8080, 9);
    CHECK((split == std::vector<std::string>{"192.168.", "178.", "65:8080"}));
    for (const auto &line : split)
        CHECK(line.size() <= 9);

    auto screen = screen_for(Reason::Storage, "10.0.0.2", 8080, std::nullopt, 18);
    CHECK(screen.word == "DATA" && screen.icon == Icon::Disk && screen.lines.size() == 1 && screen.progress < 0);
    screen = screen_for(Reason::Crash, "10.0.0.2", 8080, std::nullopt, 18);
    CHECK(screen.word == "CRASH" && screen.icon == Icon::Crash);
    // Running action: progress bar instead of the address.
    screen = screen_for(Reason::Storage, "10.0.0.2", 8080, state, 18);
    CHECK(screen.word == "FIX" && screen.progress == 100 && screen.lines.empty());
    // Finished action: back to the reason screen.
    auto done = parse_action_state(R"({"action":"repair","state":"failed","message":"x"})");
    screen = screen_for(Reason::Storage, "10.0.0.2", 8080, done, 18);
    CHECK(screen.word == "DATA");
    // No network: the address would be useless.
    screen = screen_for(Reason::Storage, "", 8080, std::nullopt, 18);
    CHECK(screen.word == "WIFI" && screen.icon == Icon::Wifi && screen.lines.empty());

    if (failures)
        std::fprintf(stderr, "%d check(s) failed\n", failures);
    else
        std::puts("recovery_smoke: OK");
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
