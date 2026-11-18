#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>

#include "shared/matrix/halo/protocol.h"
#include "shared/matrix/halo/sampler.h"
#include "shared/matrix/halo/settings.h"

namespace rgb_matrix {
class FrameCanvas;
}

namespace Halo {

/// Drives the rear halo light: sends checksummed colour frames to the Pro
/// Micro over a UART at ~15 Hz (also the firmware's heartbeat).
///
/// "match" mode costs nothing in the render path unless it is active: the
/// render thread only does one relaxed atomic load per presented frame, and
/// reads a few edge pixels of the *already composed* canvas (no /live_frame
/// capture involved) when the sender thread asked for a new sample, at most
/// once per send tick.
class Service {
public:
    static Service& instance();

    /// Opens the UART before the matrix drops root: the library's privilege
    /// drop (setresgid/setresuid, no initgroups) leaves the process without
    /// the `dialout` group, so /dev/serial0 could not be opened afterwards.
    /// Reads "halo.device" from the config file, defaulting to /dev/serial0.
    /// Silently does nothing when the device does not exist.
    void preopen(const std::string& config_file);

    void start(const Settings& settings);
    void stop();
    void update_settings(const Settings& settings);
    [[nodiscard]] nlohmann::json status();

    /// Render-thread hook, called with the composed canvas about to be shown.
    void offer(rgb_matrix::FrameCanvas* canvas, int width, int height)
    {
        if (sample_wanted_.load(std::memory_order_relaxed))
            sample(canvas, width, height);
    }
    /// The display is blanked (matrix turned off): match mode follows to black.
    void submit_black();
    /// Latest edge colours; also what offer() stores. Public for tests.
    void submit_zones(const Zones& zones);

private:
    Service() = default;
    void sample(rgb_matrix::FrameCanvas* canvas, int width, int height);
    void run();
    void tick();
    void ensure_open(const std::string& device);
    void close_fd();
    void send(std::uint8_t brightness, int led_count, const Zones& zones);
    void note_error(const std::string& message, bool warn);

    std::mutex mutex_;
    std::condition_variable cv_;
    Settings settings_;
    bool stop_ = true;
    std::thread thread_;

    std::atomic<bool> sample_wanted_{false};
    std::mutex zones_mutex_;
    Zones latest_{};

    // Used by the sender thread (and start/stop around it).
    int fd_ = -1;
    std::string fd_device_;
    std::chrono::steady_clock::time_point next_open_try_{};
    int off_frames_left_ = 0;
    ZoneSmoother smoother_{0.35f};

    int preopened_fd_ = -1;
    std::string preopened_device_;

    std::atomic<std::uint64_t> frames_sent_{0};
    std::atomic<bool> running_open_{false};
    std::mutex status_mutex_;
    std::string last_error_;
};

}  // namespace Halo
