#pragma once
#include <string>

namespace Autostart {
    // Enable autostart for the application
    // exePath: path to the executable
    // appName: name for the autostart entry
    bool enable(const std::string& exePath, const std::string& appName);

    // Disable autostart for the application
    // appName: name for the autostart entry
    bool disable(const std::string& appName);

    // Check if autostart is enabled
    // appName: name for the autostart entry
    bool isEnabled(const std::string& appName);

    // Get the last error message from enable/disable
    std::string getLastError();
}
