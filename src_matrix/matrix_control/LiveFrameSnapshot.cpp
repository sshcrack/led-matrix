#include "LiveFrameSnapshot.h"

#include <limits>
#include <utility>

#include "led-matrix.h"

namespace LiveFrame {

SnapshotStore &SnapshotStore::instance()
{
    static SnapshotStore store;
    return store;
}

void SnapshotStore::request_capture()
{
    demand_generation_.fetch_add(1, std::memory_order_relaxed);
}

std::uint64_t SnapshotStore::requested_generation() const
{
    return demand_generation_.load(std::memory_order_relaxed);
}

bool SnapshotStore::capture_requested(const std::uint64_t generation) const
{
    return generation != served_generation_.load(std::memory_order_relaxed);
}

void SnapshotStore::mark_demand_served(const std::uint64_t generation)
{
    // Only satisfy demand observed before the copy started. Requests that race
    // in while pixels are being copied retain a newer generation and therefore
    // trigger another capture on the next rendered frame.
    served_generation_.store(generation, std::memory_order_relaxed);
}

void SnapshotStore::capture_if_requested(rgb_matrix::FrameCanvas *canvas,
                                         const int width,
                                         const int height)
{
    const auto generation = requested_generation();
    if (!capture_requested(generation))
        return;
    if (canvas == nullptr || width <= 0 || height <= 0 ||
        width > std::numeric_limits<std::uint16_t>::max() ||
        height > std::numeric_limits<std::uint16_t>::max())
        return;

    std::vector<std::uint8_t> rgb(static_cast<std::size_t>(width) * height * 3);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            std::uint8_t r = 0, g = 0, b = 0;
            canvas->GetPixel(x, y, &r, &g, &b);
            const auto offset = static_cast<std::size_t>((y * width + x) * 3);
            rgb[offset] = r;
            rgb[offset + 1] = g;
            rgb[offset + 2] = b;
        }
    }
    publish(width, height, std::move(rgb));
    mark_demand_served(generation);
}

void SnapshotStore::publish_solid_if_requested(const int width,
                                               const int height,
                                               const std::uint8_t r,
                                               const std::uint8_t g,
                                               const std::uint8_t b)
{
    const auto generation = requested_generation();
    if (!capture_requested(generation))
        return;
    if (width <= 0 || height <= 0 ||
        width > std::numeric_limits<std::uint16_t>::max() ||
        height > std::numeric_limits<std::uint16_t>::max())
        return;

    std::vector<std::uint8_t> rgb(static_cast<std::size_t>(width) * height * 3);
    for (std::size_t i = 0; i < rgb.size(); i += 3) {
        rgb[i] = r;
        rgb[i + 1] = g;
        rgb[i + 2] = b;
    }
    publish(width, height, std::move(rgb));
    mark_demand_served(generation);
}

void SnapshotStore::publish(const int width,
                            const int height,
                            std::vector<std::uint8_t> rgb)
{
    std::lock_guard lock(snapshot_mutex_);
    snapshot_.width = static_cast<std::uint16_t>(width);
    snapshot_.height = static_cast<std::uint16_t>(height);
    ++snapshot_.sequence;
    snapshot_.rgb = std::move(rgb);
}

Snapshot SnapshotStore::snapshot() const
{
    std::lock_guard lock(snapshot_mutex_);
    return snapshot_;
}

} // namespace LiveFrame
