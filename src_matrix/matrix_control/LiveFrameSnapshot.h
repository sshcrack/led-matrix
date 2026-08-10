#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

namespace rgb_matrix {
class FrameCanvas;
}

namespace LiveFrame {

struct Snapshot {
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::uint32_t sequence = 0;
    std::vector<std::uint8_t> rgb;

    [[nodiscard]] bool available() const {
        return width > 0 && height > 0 &&
            rgb.size() == static_cast<std::size_t>(width) * height * 3;
    }
};

/// Stores a throttled copy of the composed matrix frame while a web client is
/// actively polling for it. With no client, render loops only pay two atomic
/// reads and do not copy any pixels.
class SnapshotStore {
public:
    static SnapshotStore &instance();

    /// Keep frame capture enabled briefly. Every /live_frame request renews
    /// this lease so capture automatically shuts off when the page is closed.
    void request_capture();

    /// Copy the current composed frame when a client lease is active and the
    /// capture-rate throttle allows it.
    void capture_if_requested(rgb_matrix::FrameCanvas *canvas, int width, int height);

    /// Publish a solid frame without touching the hardware canvas. Used while
    /// the matrix is disabled so browser previews do not freeze on stale data.
    void publish_solid_if_requested(int width, int height,
                                    std::uint8_t r, std::uint8_t g, std::uint8_t b);

    [[nodiscard]] Snapshot snapshot() const;

private:
    static std::int64_t steady_now_ms();
    bool claim_capture_slot(std::int64_t now_ms);
    void publish(int width, int height, std::vector<std::uint8_t> rgb);

    std::atomic<std::int64_t> capture_until_ms_{0};
    std::atomic<std::int64_t> next_capture_ms_{0};
    mutable std::mutex snapshot_mutex_;
    Snapshot snapshot_;
};

} // namespace LiveFrame
