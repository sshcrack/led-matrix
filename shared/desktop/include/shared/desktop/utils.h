#pragma once

#include "shared/desktop/macro.h"
#include <filesystem>
#include <shared/common/utils/utils.h>
#include <atomic>
#include <cstddef>
#include <string>
#include <string_view>

SHARED_DESKTOP_API bool isWritableExistingFile(const std::filesystem::path &path);
SHARED_DESKTOP_API std::filesystem::path get_data_dir();
SHARED_DESKTOP_API int run_command(const std::string& cmd,
                                    const std::atomic<bool>* running = nullptr);

struct SHARED_DESKTOP_API CommandResult {
    int exit_code = -1;
    std::string output;
};

/// Runs a child command while retaining a bounded tail of combined stdout and
/// stderr. This is for external-tool diagnostics where an exit code alone is
/// not actionable. Cancellation semantics match run_command().
SHARED_DESKTOP_API CommandResult run_command_capture(
    const std::string& cmd,
    const std::atomic<bool>* running = nullptr,
    std::size_t max_output_bytes = 16384);
SHARED_DESKTOP_API std::string run_command_and_get_output(const std::string& cmd);

/// Returns the globally configured yt-dlp executable as a shell-safe command
/// token. An empty configured path means `yt-dlp` is resolved from PATH.
SHARED_DESKTOP_API std::string get_ytdlp_command();

/// Returns the configured yt-dlp executable plus the network policy shared by
/// all online yt-dlp calls. Keep transport workarounds here instead of letting
/// individual plugins drift apart.
SHARED_DESKTOP_API std::string get_ytdlp_network_command();

/// Format selector used for display-only video streaming. SpotifyMV never
/// consumes the video's audio track, so this policy can prefer video-only
/// formats and avoid unnecessary merge/download work.
SHARED_DESKTOP_API std::string_view get_ytdlp_video_format_selector();

/// Validates the globally configured yt-dlp executable. Returns an empty
/// string on success or a user-facing error message on failure.
SHARED_DESKTOP_API std::string check_ytdlp_available();

/// Validates the external tools required by video-based desktop plugins.
/// Returns an empty string when both ffmpeg and the shared yt-dlp executable
/// are available.
SHARED_DESKTOP_API std::string check_video_tools_available();

/// Opens a native file-open dialog (Windows: GetOpenFileNameA;
/// Linux: zenity, falling back to kdialog). Returns the selected path, or an
/// empty string if cancelled or no dialog tool is available.
SHARED_DESKTOP_API std::string open_file_dialog(const std::string& title);