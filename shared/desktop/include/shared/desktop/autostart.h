#pragma once
#include <string>
#include <expected>
#include "shared/desktop/macro.h"

namespace Autostart
{
    // Enable autostart for the application
    // exePath: path to the executable
    // appName: name for the autostart entry
    SHARED_DESKTOP_API std::expected<void, std::string> enable(const std::string &exePath, const std::string &appName);

    // Disable autostart for the application
    // appName: name for the autostart entry
    SHARED_DESKTOP_API std::expected<void, std::string> disable(const std::string &appName);

    // Check if autostart is enabled
    // appName: name for the autostart entry
    SHARED_DESKTOP_API bool isEnabled(const std::string &appName);
}
