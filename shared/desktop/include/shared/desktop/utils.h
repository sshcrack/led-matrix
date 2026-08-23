#pragma once

#include "shared/desktop/macro.h"
#include <filesystem>
#include <shared/common/utils/utils.h>
#include <atomic>
#include <string>

SHARED_DESKTOP_API bool isWritableExistingFile(const std::filesystem::path &path);
SHARED_DESKTOP_API std::filesystem::path get_data_dir();
SHARED_DESKTOP_API int run_command(const std::string& cmd,
                                    const std::atomic<bool>* running = nullptr);
SHARED_DESKTOP_API std::string run_command_and_get_output(const std::string& cmd);

/// Returns the globally configured yt-dlp executable as a shell-safe command
/// token. An empty configured path means `yt-dlp` is resolved from PATH.
SHARED_DESKTOP_API std::string get_ytdlp_command();

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