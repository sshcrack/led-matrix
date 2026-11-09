#pragma once

#include <algorithm>
#include <charconv>
#include <optional>
#include <string>
#include <string_view>

namespace DesktopControlProtocol {
inline constexpr std::string_view MatrixEnabledPrefix = "matrix_enabled:";
inline constexpr std::string_view DesktopProducerPrefix = "desktop_producer:";
/// Desktop -> matrix: how useful this desktop is as the producer right now
/// (0 = nothing to offer, 100 = audio playing / music analysed). Sent every
/// couple of seconds; the matrix elects the most relevant desktop.
inline constexpr std::string_view DesktopRelevancePrefix = "desktop_relevance:";

[[nodiscard]] inline std::string matrix_enabled(bool enabled)
{
    return std::string(MatrixEnabledPrefix) + (enabled ? "1" : "0");
}

[[nodiscard]] inline std::optional<bool> parse_matrix_enabled(std::string_view message)
{
    if (!message.starts_with(MatrixEnabledPrefix))
        return std::nullopt;

    const auto value = message.substr(MatrixEnabledPrefix.size());
    if (value == "1" || value == "true")
        return true;
    if (value == "0" || value == "false")
        return false;
    return std::nullopt;
}

[[nodiscard]] inline std::string desktop_producer(bool active)
{
    return std::string(DesktopProducerPrefix) + (active ? "1" : "0");
}

[[nodiscard]] inline std::optional<bool> parse_desktop_producer(std::string_view message)
{
    if (!message.starts_with(DesktopProducerPrefix))
        return std::nullopt;

    const auto value = message.substr(DesktopProducerPrefix.size());
    if (value == "1" || value == "true")
        return true;
    if (value == "0" || value == "false")
        return false;
    return std::nullopt;
}

[[nodiscard]] inline std::string desktop_relevance(int score)
{
    return std::string(DesktopRelevancePrefix) + std::to_string(std::clamp(score, 0, 100));
}

[[nodiscard]] inline std::optional<int> parse_desktop_relevance(std::string_view message)
{
    if (!message.starts_with(DesktopRelevancePrefix))
        return std::nullopt;

    const auto value = message.substr(DesktopRelevancePrefix.size());
    int score = 0;
    const auto [end, ec] = std::from_chars(value.data(), value.data() + value.size(), score);
    if (ec != std::errc{} || end != value.data() + value.size())
        return std::nullopt;
    return std::clamp(score, 0, 100);
}
}  // namespace DesktopControlProtocol
