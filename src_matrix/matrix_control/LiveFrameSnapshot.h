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

/// Stores the composed matrix frame strictly on demand. Every /live_frame HTTP
/// request asks for at most one future capture. With no outstanding request,
/// render loops perform only two relaxed atomic reads and never touch pixels,
/// allocate frame memory, lock the snapshot mutex, or run a timer.
class SnapshotStore {
public:
    static SnapshotStore &instance();

    /// Request one future composed frame. Multiple requests that arrive before
    /// the next render are coalesced into that single capture.
    void request_capture();

    /// Capture exactly once when there is outstanding demand.
    void capture_if_requested(rgb_matrix::FrameCanvas *canvas, int width, int height);

    /// Publish a solid frame only when demanded. Used while the matrix is off.
    void publish_solid_if_requested(int width, int height,
                                    std::uint8_t r, std::uint8_t g, std::uint8_t b);

    [[nodiscard]] Snapshot snapshot() const;

private:
    [[nodiscard]] std::uint64_t requested_generation() const;
    [[nodiscard]] bool capture_requested(std::uint64_t generation) const;
    void mark_demand_served(std::uint64_t generation);
    void publish(int width, int height, std::vector<std::uint8_t> rgb);

    std::atomic<std::uint64_t> demand_generation_{0};
    std::atomic<std::uint64_t> served_generation_{0};
    mutable std::mutex snapshot_mutex_;
    Snapshot snapshot_;
};

} // namespace LiveFrame
