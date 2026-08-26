#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace DesktopControlProtocol {
inline constexpr std::string_view MatrixEnabledPrefix = "matrix_enabled:";

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
}  // namespace DesktopControlProtocol
