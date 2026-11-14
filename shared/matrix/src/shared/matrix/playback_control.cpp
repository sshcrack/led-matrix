#include "shared/matrix/playback_control.h"

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>

#include "shared/matrix/utils/shared.h"

namespace PlaybackControl {
namespace {
std::mutex reason_mutex;
std::string last_reason;
std::int64_t last_unix_ms = 0;
std::atomic<std::uint64_t> restart_count{0};
}  // namespace

void restart(std::string_view reason)
{
    {
        std::lock_guard lock(reason_mutex);
        last_reason.assign(reason);
        last_unix_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }
    restart_count.fetch_add(1, std::memory_order_relaxed);
    exit_canvas_update.store(true);
}

void reload_preset(std::string_view preset_id)
{
    std::string reason = "reload_preset:";
    reason += preset_id;
    restart(reason);
}

void interrupt() noexcept
{
    exit_canvas_update.store(true);
}

const std::atomic<bool>& restart_flag() noexcept
{
    return exit_canvas_update;
}

bool restart_pending() noexcept
{
    return exit_canvas_update.load(std::memory_order_relaxed);
}

void acknowledge() noexcept
{
    exit_canvas_update.store(false);
}

nlohmann::json diagnostics_json()
{
    nlohmann::json last = nullptr;
    {
        std::lock_guard lock(reason_mutex);
        if (!last_reason.empty())
            last = {{"reason", last_reason}, {"unix_ms", last_unix_ms}};
    }
    return {{"pending", restart_pending()}, {"restarts", restart_count.load(std::memory_order_relaxed)}, {"last_restart", last}};
}

void reset_for_tests()
{
    {
        std::lock_guard lock(reason_mutex);
        last_reason.clear();
        last_unix_ms = 0;
    }
    restart_count.store(0);
    acknowledge();
}

}  // namespace PlaybackControl
