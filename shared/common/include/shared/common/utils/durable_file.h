#pragma once

#include <chrono>
#include <filesystem>
#include <string>
#include <string_view>

#include "shared/common/macro.h"

/// Writes that survive a power cut: after `replace()` returns true, the target
/// holds the complete new content even if power is lost right afterwards, and
/// at no point can a reader or a power cut leave it truncated (it holds either
/// the old or the new content).
namespace DurableFile {
/// Prefix of the sibling temp files `replace()` writes (`<target>.tmp-<random>`).
/// A power cut can leave one behind; see `remove_stale_temps()`.
SHARED_COMMON_API std::string temp_prefix(const std::filesystem::path& target);

/// Writes `content` to a sibling temp file, syncs it, renames it over
/// `target` and syncs the directory. An existing target keeps its
/// permissions (and owner, when running as root). On failure the target is
/// unchanged, the temp file is removed and `error` (if given) says why.
SHARED_COMMON_API bool replace(const std::filesystem::path& target, std::string_view content, std::string* error = nullptr);

/// Removes temp files of `target` left by an interrupted `replace()` that are
/// older than `min_age`, so a concurrent writer's live temp file is kept.
/// Returns how many were removed.
SHARED_COMMON_API int remove_stale_temps(const std::filesystem::path& target, std::chrono::seconds min_age = std::chrono::seconds(60));
}  // namespace DurableFile
