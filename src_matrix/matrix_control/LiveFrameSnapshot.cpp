#include "LiveFrameSnapshot.h"

#include <chrono>
#include <limits>
#include <utility>

#include "led-matrix.h"

namespace LiveFrame {
namespace {
constexpr std::int64_t capture_lease_ms = 1500;
constexpr std::int64_t capture_interval_ms = 67; // ~15 FPS max.
}

SnapshotStore &SnapshotStore::instance()
{
    static SnapshotStore store;
    return store;
}

std::int64_t SnapshotStore::steady_now_ms()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

void SnapshotStore::request_capture()
{
    capture_until_ms_.store(steady_now_ms() + capture_lease_ms,
                            std::memory_order_relaxed);
}

bool SnapshotStore::claim_capture_slot(const std::int64_t now_ms)
{
    if (now_ms > capture_until_ms_.load(std::memory_order_relaxed))
        return false;

    auto next = next_capture_ms_.load(std::memory_order_relaxed);
    if (now_ms < next)
        return false;

    return next_capture_ms_.compare_exchange_strong(
        next, now_ms + capture_interval_ms,
        std::memory_order_relaxed, std::memory_order_relaxed);
}

void SnapshotStore::capture_if_requested(rgb_matrix::FrameCanvas *canvas,
                                         const int width,
                                         const int height)
{
    if (canvas == nullptr || width <= 0 || height <= 0 ||
        width > std::numeric_limits<std::uint16_t>::max() ||
        height > std::numeric_limits<std::uint16_t>::max())
        return;

    const auto now_ms = steady_now_ms();
    if (!claim_capture_slot(now_ms))
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
}

void SnapshotStore::publish_solid_if_requested(const int width,
                                               const int height,
                                               const std::uint8_t r,
                                               const std::uint8_t g,
                                               const std::uint8_t b)
{
    if (width <= 0 || height <= 0 ||
        width > std::numeric_limits<std::uint16_t>::max() ||
        height > std::numeric_limits<std::uint16_t>::max())
        return;

    const auto now_ms = steady_now_ms();
    if (!claim_capture_slot(now_ms))
        return;

    std::vector<std::uint8_t> rgb(static_cast<std::size_t>(width) * height * 3);
    for (std::size_t i = 0; i < rgb.size(); i += 3) {
        rgb[i] = r;
        rgb[i + 1] = g;
        rgb[i + 2] = b;
    }
    publish(width, height, std::move(rgb));
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
