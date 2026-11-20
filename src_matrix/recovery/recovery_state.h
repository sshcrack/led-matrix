#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// Recovery mode (docs/RECOVERY.md): what went wrong, what the panel shows and
/// which repairs the web page may ask for. Pure logic, unit-tested by
/// tests/recovery_smoke.cpp.
namespace Recovery {
/// Why the launcher started recovery mode (`--recovery=<id>`).
enum class Reason { Storage, Crash };

std::optional<Reason> parse_reason(std::string_view id);
std::string_view reason_id(Reason reason);
/// One sentence for the web page.
std::string_view describe(Reason reason);

/// Progress of the root-side action, from $RUN_DIR/state.json.
struct ActionState {
    std::string action;  // repair | reset | retry | install
    std::string state;   // running | done | failed
    std::string message;
    int progress = -1;  // 0..100, -1 when unknown

    [[nodiscard]] bool running() const { return state == "running"; }
};
/// Empty when the text is not a valid state object.
std::optional<ActionState> parse_action_state(std::string_view json);

/// Actions the root helper accepts (scripts/recovery_action.sh).
bool is_helper_action(std::string_view action);

enum class Icon { Disk, Crash, Wrench, Wifi };
struct Color {
    uint8_t r, g, b;
};

/// What the panel shows: an icon, one word and the address to open (or a
/// progress bar while an action runs). Recovery is an error screen, so the
/// scenes' no-text rule does not apply; it stays at a word and the address.
struct Screen {
    Icon icon;
    Color color;
    std::string word;
    std::vector<std::string> lines;  // the address, split to fit
    int progress = -1;
};

/// `ip` is empty without a network: the screen then says WIFI instead.
Screen screen_for(Reason reason, const std::string& ip, uint16_t port, const std::optional<ActionState>& action, std::size_t max_chars);

/// `http://<ip>:<port>` without the scheme, split at dots so each line has at most `max_chars`.
std::vector<std::string> address_lines(const std::string& ip, uint16_t port, std::size_t max_chars);
}  // namespace Recovery
