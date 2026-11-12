#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

#include "shared/desktop/macro.h"

/// Watches the OS media sessions (MPRIS over D-Bus on Linux, the Global System
/// Media Transport Controls on Windows) on its own thread and reports whether
/// any player on this PC is playing. Polls every couple of seconds; queries
/// never block the caller.
class SHARED_DESKTOP_API MediaSessionMonitor {
public:
    struct Snapshot {
        /// False when the platform has no media-session API or it is unreachable.
        bool available = false;
        bool playing = false;
        /// The player that is playing (e.g. "spotify"), empty when none is.
        std::string player;
    };

    explicit MediaSessionMonitor(std::chrono::milliseconds interval = std::chrono::seconds(2));
    ~MediaSessionMonitor();
    MediaSessionMonitor(const MediaSessionMonitor&) = delete;
    MediaSessionMonitor& operator=(const MediaSessionMonitor&) = delete;

    void start();
    void stop();
    [[nodiscard]] Snapshot snapshot() const;

private:
    void run();

    std::chrono::milliseconds interval_;
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    bool stopping_ = false;
    Snapshot snapshot_;
    std::thread thread_;
};
