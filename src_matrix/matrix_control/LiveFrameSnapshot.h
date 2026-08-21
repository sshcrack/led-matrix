#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
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

};

/// Captures the composed matrix strictly on demand. A connected live-preview
/// WebSocket is inert until its client requests a frame. With no outstanding
/// request render loops perform only relaxed atomic reads: no pixel reads,
/// allocation, mutex acquisition, timer, or background capture.
class SnapshotStore {
public:
    using PublishCallback = std::function<void(const Snapshot &, std::uint64_t capture_generation)>;

    static SnapshotStore &instance();

    /// Request one future composed frame and return the generation that must be
    /// reached before this request can be satisfied. Requests observed before
    /// the same render are coalesced into one physical matrix copy.
    [[nodiscard]] std::uint64_t request_capture();

    /// Capture exactly once when there is outstanding demand.
    void capture_if_requested(rgb_matrix::FrameCanvas *canvas, int width, int height);

    /// Publish a solid frame only when demanded. Used while the matrix is off.
    void publish_solid_if_requested(int width, int height,
                                    std::uint8_t r, std::uint8_t g, std::uint8_t b);

    /// Install the transport sink that receives only frames captured because
    /// demand was outstanding. capture_generation identifies the newest request
    /// already present when the pixel copy began, so requests racing with that
    /// copy can wait for the following frame instead of receiving stale data.
    void set_publish_callback(PublishCallback callback);

private:
    [[nodiscard]] std::uint64_t requested_generation() const;
    [[nodiscard]] bool capture_requested(std::uint64_t generation) const;
    void mark_demand_served(std::uint64_t generation);
    void publish(int width, int height, std::vector<std::uint8_t> rgb,
                 std::uint64_t capture_generation);

    std::atomic<std::uint64_t> demand_generation_{0};
    std::atomic<std::uint64_t> served_generation_{0};
    std::atomic<std::uint32_t> frame_sequence_{0};
    std::mutex callback_mutex_;
    PublishCallback publish_callback_;
};

} // namespace LiveFrame
