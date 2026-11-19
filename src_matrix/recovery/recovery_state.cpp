#include "recovery_state.h"

#include <algorithm>

#include <nlohmann/json.hpp>

namespace Recovery {
    std::optional<Reason> parse_reason(std::string_view id)
    {
        if (id == "storage")
            return Reason::Storage;
        if (id == "crash")
            return Reason::Crash;
        return std::nullopt;
    }

    std::string_view reason_id(Reason reason)
    {
        return reason == Reason::Storage ? "storage" : "crash";
    }

    std::string_view describe(Reason reason)
    {
        if (reason == Reason::Storage)
            return "The data partition (settings, uploads, updates) is missing, damaged or read-only. "
                   "The panel is running the installed package without your settings.";
        return "The panel app kept stopping right after it started, so it was started in recovery mode. "
               "Your settings are untouched.";
    }

    std::optional<ActionState> parse_action_state(std::string_view text)
    {
        const auto json = nlohmann::json::parse(text, nullptr, false);
        if (!json.is_object() || !json.contains("action") || !json.contains("state") || !json["action"].is_string() ||
            !json["state"].is_string())
            return std::nullopt;
        ActionState state;
        state.action = json["action"].get<std::string>();
        state.state = json["state"].get<std::string>();
        if (json.contains("message") && json["message"].is_string())
            state.message = json["message"].get<std::string>();
        if (json.contains("progress") && json["progress"].is_number_integer())
            state.progress = std::clamp(json["progress"].get<int>(), -1, 100);
        if (state.state != "running" && state.state != "done" && state.state != "failed")
            return std::nullopt;
        return state;
    }

    bool is_helper_action(std::string_view action)
    {
        return action == "repair" || action == "reset" || action == "retry";
    }

    std::vector<std::string> address_lines(const std::string &ip, uint16_t port, std::size_t max_chars)
    {
        const std::string address = ip + ":" + std::to_string(port);
        if (address.size() <= max_chars || max_chars == 0)
            return {address};
        // Split after a dot so each part fits: "192.168." / "178.65:8080".
        std::vector<std::string> lines;
        std::string line;
        std::size_t start = 0;
        while (start < address.size()) {
            auto dot = address.find('.', start);
            std::string part = dot == std::string::npos ? address.substr(start) : address.substr(start, dot - start + 1);
            start += part.size();
            if (!line.empty() && line.size() + part.size() > max_chars) {
                lines.push_back(line);
                line.clear();
            }
            line += part;
        }
        if (!line.empty())
            lines.push_back(line);
        return lines;
    }

    Screen screen_for(Reason reason, const std::string &ip, uint16_t port, const std::optional<ActionState> &action,
                      std::size_t max_chars)
    {
        constexpr Color amber{255, 150, 0}, red{230, 30, 30}, blue{40, 120, 255}, grey{150, 150, 150};
        Screen screen = reason == Reason::Storage ? Screen{Icon::Disk, amber, "DATA", {}, -1}
                                                  : Screen{Icon::Crash, red, "CRASH", {}, -1};
        if (action && action->running()) {
            screen.icon = Icon::Wrench;
            screen.color = blue;
            screen.word = "FIX";
            screen.progress = std::max(action->progress, 0);
        }
        if (ip.empty()) {
            // The address would be useless without a network.
            screen.icon = Icon::Wifi;
            screen.color = grey;
            screen.word = "WIFI";
            return screen;
        }
        if (screen.progress < 0)
            screen.lines = address_lines(ip, port, max_chars);
        return screen;
    }
}  // namespace Recovery
