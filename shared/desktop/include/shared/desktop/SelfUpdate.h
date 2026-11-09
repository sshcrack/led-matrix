#pragma once

// Per-user, self-updating install of the desktop app (no root/admin, no .deb/.msi).
//
// Install root: $XDG_DATA_HOME/led-matrix-desktop (default ~/.local/share/...)
// on Linux, %LOCALAPPDATA%\led-matrix-desktop on Windows:
//
//   versions/<version>/{bin,lib,share}   one extracted release package each
//   current -> versions/<version>        symlink (Linux) / junction (Windows), swapped on update
//   downloads/, staging-*/               in-flight updates
//
// Launchers (desktop entry + ~/.local/bin link, or Start Menu shortcut + "Apps"
// uninstall entry) and the autostart entry all start current/bin/led-matrix-desktop.
// A running process resolves its own path to versions/<v>/bin, so installing a
// new version never touches files it uses; the new version runs from the next
// start (restart_into_current() does that right away).

#include "shared/desktop/macro.h"
#include <cstddef>
#include <expected>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>

namespace SelfUpdate
{
    /// Release asset name for this platform, e.g. led-matrix-desktop-1.2.3-Linux.tar.gz
    SHARED_DESKTOP_API std::string package_asset_name(const std::string &version);

    /// Install root, whether or not anything is installed there yet.
    SHARED_DESKTOP_API std::filesystem::path install_root();

    /// True when the running binary is a per-user install that can update itself.
    SHARED_DESKTOP_API bool is_managed_install();

    /// A .deb in /usr or the old admin install in Program Files (can move to a per-user install).
    SHARED_DESKTOP_API bool is_recognized_system_install();

    /// Running from a CMake build tree: automatic update checks stay off.
    SHARED_DESKTOP_API bool is_dev_build();

    /// Marks the running version as in use for this process's lifetime, so pruning by
    /// another process (an update, `--install`) never deletes it. Call once at startup.
    /// False when this version was removed while the app was starting: start current instead.
    SHARED_DESKTOP_API bool hold_running_version();

    /// Executable that launchers should start: current/bin/... in the install root.
    SHARED_DESKTOP_API std::filesystem::path current_exec_path();

    /// Moves (or copies) an extracted package prefix (the directory holding bin/)
    /// to versions/<version>, makes it current, writes the launcher entries and
    /// prunes old versions. `first_install` also creates a desktop shortcut on Windows.
    SHARED_DESKTOP_API std::expected<void, std::string> install_prefix(const std::filesystem::path &prefix,
                                                                       const std::string &version,
                                                                       bool move, bool first_install);

    /// Downloads a release package, verifies its sha256 (required) and installs it.
    SHARED_DESKTOP_API std::expected<void, std::string> download_and_install(
        const std::string &url, const std::string &sha256, const std::string &version,
        const std::function<void(size_t downloaded, size_t total)> &progress);

    /// Removes versions other than current and the running one, and leftover downloads.
    SHARED_DESKTOP_API void prune();

    /// Handles --install / --uninstall before the app starts. Returns an exit
    /// code when one of them was given, nullopt to start normally.
    ///   --install [--launch]  installs the package the binary was started from
    ///   --uninstall           removes the per-user install (keeps settings)
    SHARED_DESKTOP_API std::optional<int> handle_command_line(int argc, char *argv[]);

    /// Starts current/bin/led-matrix-desktop (Linux: replaces this process).
    /// Call after shutdown released the single-instance lock. False on failure.
    /// `after_update` passes --after-update (skips "Turn Matrix On on Start").
    SHARED_DESKTOP_API bool restart_into_current(bool start_minimized, bool after_update = true);

    /// The installation the app was migrated away from (a .deb in /usr, the old
    /// admin NSIS install in Program Files), if it still exists.
    SHARED_DESKTOP_API std::optional<std::filesystem::path> legacy_install();
    /// Removes it (asks for elevation once: pkexec / UAC). False if that failed to start.
    SHARED_DESKTOP_API bool remove_legacy_install();
    /// Stop offering to remove it.
    SHARED_DESKTOP_API void forget_legacy_install();
} // namespace SelfUpdate
