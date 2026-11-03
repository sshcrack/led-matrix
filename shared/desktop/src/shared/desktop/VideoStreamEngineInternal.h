#pragma once

// Private helpers shared by the VideoStreamEngine*.cpp translation units.

#include <string>

namespace Shared::detail {

inline const char* null_device() {
#ifdef _WIN32
    return "NUL";
#else
    return "/dev/null";
#endif
}

// Condenses yt-dlp/ffmpeg output into one short, human-readable error line.
// Defined in VideoStreamEngineCommands.cpp.
std::string command_error_summary(const std::string& output);

} // namespace Shared::detail
