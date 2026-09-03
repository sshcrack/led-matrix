#pragma once

#include <filesystem>
#include <string_view>

#include "shared/common/macro.h"

namespace CrashReporter {

struct Config {
    std::string_view process_name;
    std::filesystem::path report_directory;
};

// Installs process-wide fatal crash handlers. The implementation is deliberately
// platform-specific behind this small seam: POSIX fatal signals on Linux/Pi and
// unhandled SEH exceptions + minidumps on Windows. Setup failure is non-fatal so
// diagnostics can never prevent the application from starting.
SHARED_COMMON_API void install(const Config& config) noexcept;

// Captures subsequent spdlog records into a fixed in-memory ring. Fatal crash
// reports can dump this ring without depending on the normal logger still being
// usable. Call after replacing spdlog's default logger, if the process does so.
SHARED_COMMON_API void attach_to_default_logger() noexcept;

// A compact, persistent description of the operation most useful when reading a
// crash after the fact (for example the currently rendering scene). Updates are
// cheap and are intended for lifecycle boundaries, not every frame.
SHARED_COMMON_API void set_activity(std::string_view activity) noexcept;

// Writes a diagnostic report for a caught fatal/top-level exception. This is
// useful for failures that would otherwise return an error code rather than hit
// an OS crash handler.
SHARED_COMMON_API void report_exception(std::string_view context, std::string_view message) noexcept;

}  // namespace CrashReporter
