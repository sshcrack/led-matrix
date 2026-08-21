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

std::uint64_t SnapshotStore::request_capture()
{
    return demand_generation_.fetch_add(1, std::memory_order_relaxed) + 1;
}

std::uint64_t SnapshotStore::requested_generation() const
{
    return demand_generation_.load(std::memory_order_relaxed);
}

bool SnapshotStore::capture_requested(const std::uint64_t generation) const
{
    return generation > served_generation_.load(std::memory_order_relaxed);
}

void SnapshotStore::mark_demand_served(const std::uint64_t generation)
{
    // Never move the served watermark backwards if capture paths ever overlap.
    // Requests that race in while pixels are being copied retain a newer
    // generation and therefore trigger another capture on the next frame.
    auto served = served_generation_.load(std::memory_order_relaxed);
    while (served < generation &&
           !served_generation_.compare_exchange_weak(
               served, generation, std::memory_order_relaxed, std::memory_order_relaxed)) {
    }
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
    publish(width, height, std::move(rgb), generation);
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
    publish(width, height, std::move(rgb), generation);
    mark_demand_served(generation);
}

void SnapshotStore::publish(const int width,
                            const int height,
                            std::vector<std::uint8_t> rgb,
                            const std::uint64_t capture_generation)
{
    Snapshot published;
    published.width = static_cast<std::uint16_t>(width);
    published.height = static_cast<std::uint16_t>(height);
    published.sequence = frame_sequence_.fetch_add(1, std::memory_order_relaxed) + 1;
    published.rgb = std::move(rgb);

    PublishCallback callback;
    {
        std::lock_guard lock(callback_mutex_);
        callback = publish_callback_;
    }

    if (callback)
        callback(published, capture_generation);
}

void SnapshotStore::set_publish_callback(PublishCallback callback)
{
    std::lock_guard lock(callback_mutex_);
    publish_callback_ = std::move(callback);
}

} // namespace LiveFrame
