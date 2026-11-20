#pragma once

#include <refresh-timing.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace rgb_matrix {
class RGBMatrixBase;
class RGBMatrix;
}  // namespace rgb_matrix
namespace Config {
class MainConfig;
}

/// Flicker measurement for /diagnostics → `refresh`: how long the library's
/// refresh thread took per full-panel refresh (rpi-rgb-led-matrix
/// GetRefreshTiming) and which interrupts hit which CPU (/proc/interrupts), over
/// the last 10 s and 60 s. A refresh that takes much longer than the median
/// (the refresh thread on its CPU was held up) shows as flicker.
namespace RefreshMonitor {

struct InterruptRow {
    std::string id;    // "104", "IPI0", "Err"
    std::string name;  // "mmc1", "Rescheduling interrupts"
    std::vector<std::uint64_t> per_cpu;
};

struct InterruptTable {
    int cpus = 0;
    std::vector<InterruptRow> rows;
};

/// Parses the text of /proc/interrupts. Unknown lines are skipped.
InterruptTable parse_interrupts(std::string_view text);

struct Sample {
    std::chrono::steady_clock::time_point at;
    std::shared_ptr<const rgb_matrix::RefreshTiming> timing;  // null without a refresh thread
    InterruptTable interrupts;
};

/// Statistics between two samples (`after` taken later). Pure, for tests.
nlohmann::json window_json(const Sample& before, const Sample& after, int refresh_cpu);

/// "Steady brightness": the refresh limit for a panel whose uncontended refresh
/// takes `fastest_us`. With hardware-timed bit planes a refresh's lit time is
/// fixed, so brightness follows the refresh rate; contention from other cores
/// (network and SD DMA, memory-heavy work) stretches a refresh by 10-15 %.
/// Padding every refresh to 1.2x the fastest one keeps the rate, and so the
/// brightness, constant through that, at the cost of a ~17 % dimmer panel.
/// 0 when the result would be below 50 Hz (pacing would flicker visibly).
int steady_limit_hz(std::uint32_t fastest_us);

class Monitor {
public:
    /// `matrix` may be an emulator; then only interrupts are reported.
    /// `configured_limit_hz` is the --led-limit-refresh value (0: none); it is
    /// restored when steady brightness is switched off.
    Monitor(rgb_matrix::RGBMatrixBase* matrix, int refresh_cpu, int configured_limit_hz);
    ~Monitor();
    Monitor(const Monitor&) = delete;
    Monitor& operator=(const Monitor&) = delete;

    /// Follows config "steady_brightness" from now on (checked every second).
    /// `config` must outlive the monitor.
    void attach_config(Config::MainConfig* config);

private:
    enum class Pacing { Off, Calibrating, On, TooSlow, Unavailable };

    void run();
    Sample take_sample() const;
    void publish();
    /// Returns true when the pacing state changed.
    bool update_pacing(std::chrono::steady_clock::time_point now);

    rgb_matrix::RGBMatrixBase* matrix_;
    rgb_matrix::RGBMatrix* hardware_;  // null for the emulator
    int refresh_cpu_;
    int configured_limit_hz_;
    std::atomic<Config::MainConfig*> config_{nullptr};

    // Used by the monitor thread only.
    Pacing pacing_ = Pacing::Off;
    int pacing_limit_hz_ = 0;
    std::uint32_t pacing_fastest_us_ = 0;
    std::chrono::steady_clock::time_point calibration_started_;
    std::shared_ptr<rgb_matrix::RefreshTiming> calibration_before_;
    std::deque<Sample> samples_;  // oldest first, every kInterval, up to 60 s back

    std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_ = false;
    std::thread thread_;
};

}  // namespace RefreshMonitor
