#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace DesktopControlProtocol {
inline constexpr std::string_view MatrixEnabledPrefix = "matrix_enabled:";
inline constexpr std::string_view DesktopProducerPrefix = "desktop_producer:";

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
}  // namespace DesktopControlProtocol
