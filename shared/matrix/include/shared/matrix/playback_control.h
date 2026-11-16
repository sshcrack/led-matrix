#pragma once

#include <atomic>
#include <string_view>

#include "nlohmann/json.hpp"
#include "shared/matrix/export.h"

/// Named intents that interrupt the running scene.
///
/// Saving configuration and changing playback are separate: `MainConfig`
/// setters only persist. A call site that really needs the display to start
/// over (power, preset switch, Scene Lab, ...) asks for it here with a reason,
/// which `/diagnostics` reports as `playback.last_restart`.
///
/// The render loop still polls one `std::atomic<bool>` (`restart_flag()`), so
/// the per-frame cost is a single relaxed load. Only `restart()` takes a lock,
/// and only when somebody asks for a restart.
namespace PlaybackControl {

/// Ask the coordinator to leave the current scene and start `run()` again.
SHARED_MATRIX_API void restart(std::string_view reason);

/// The active preset's content changed (or another preset became the active one).
SHARED_MATRIX_API void reload_preset(std::string_view preset_id);

/// Wake a running scene for process shutdown. Lock-free and allocation-free,
/// so it is safe inside a signal handler (no reason is recorded).
SHARED_MATRIX_API void interrupt() noexcept;

/// The one flag the render loop polls.
[[nodiscard]] SHARED_MATRIX_API const std::atomic<bool>& restart_flag() noexcept;

[[nodiscard]] SHARED_MATRIX_API bool restart_pending() noexcept;

/// Called by the outer playback loop before it samples schedule/power state, so
/// every request made after that point ends the run it can no longer affect.
SHARED_MATRIX_API void acknowledge() noexcept;

/// `{pending, restarts, last_restart: {reason, unix_ms} | null}`
[[nodiscard]] SHARED_MATRIX_API nlohmann::json diagnostics_json();

/// Forget the recorded reason and count (tests).
SHARED_MATRIX_API void reset_for_tests();

}  // namespace PlaybackControl
