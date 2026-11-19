#pragma once
// Edge colour sampling and smoothing for the halo's "match" mode. Header-only
// and free of hardware types so it is unit-testable.
#include <algorithm>
#include <array>
#include <cstdint>

namespace Halo {

constexpr int kEdgeZones = 8;
constexpr int kDefaultSampleColumns = 8;
constexpr int kMaxSampleColumns = 16;
constexpr int kOuterSampleDepth = 2;
constexpr int kDefaultSampleDepth = 6;
constexpr int kMaxSampleDepth = 24;

struct Rgb {
    std::uint8_t r = 0, g = 0, b = 0;
    bool operator==(const Rgb&) const = default;
};
using Zones = std::array<Rgb, kEdgeZones>;

/// Samples a sparse set of pixels along the display edge and returns the mean
/// colour of each of the 8 zones, clockwise from the top-left, seen from the
/// front: top-left, top-right, right-top, right-bottom, bottom-right,
/// bottom-left, left-bottom, left-top. Per zone it reads `along` x 2 pixels
/// (two rows inside the edge, middle 75 % of the half edge), 16 by default, i.e. 128 reads per
/// call. `get(x, y)` returns an Rgb.
template <class GetPixel>
Zones sample_zones(int width, int height, GetPixel&& get, int along = kDefaultSampleColumns, int inner_depth = kDefaultSampleDepth)
{
    Zones zones{};
    if (width < 4 || height < 4 || along < 1)
        return zones;
    const int depth_a = std::min(kOuterSampleDepth, width / 2 - 1), depth_b = std::min(inner_depth, width / 2 - 1);
    const int depth_h_a = std::min(kOuterSampleDepth, height / 2 - 1), depth_h_b = std::min(inner_depth, height / 2 - 1);

    for (int zone = 0; zone < kEdgeZones; ++zone) {
        const int edge = zone / 2;  // 0 top, 1 right, 2 bottom, 3 left
        const int half = zone % 2;  // 0 = first half in clockwise direction
        const int span = (edge % 2 == 0) ? width : height;
        const int half_len = span / 2;
        long sr = 0, sg = 0, sb = 0;
        int count = 0;
        for (int i = 0; i < along; ++i) {
            // centre of the i-th of `along` equal slices of the middle 75 % of
            // this half edge (corners belong to two zones; keep them out)
            const int offset = half_len / 8 + (2 * i + 1) * (3 * half_len / 4) / (2 * along);
            const int along_pos = half * half_len + offset;  // clockwise distance from the edge start
            for (int d = 0; d < 2; ++d) {
                int x = 0, y = 0;
                switch (edge) {
                    case 0:
                        x = along_pos;
                        y = d ? depth_h_b : depth_h_a;
                        break;
                    case 1:
                        x = width - 1 - (d ? depth_b : depth_a);
                        y = along_pos;
                        break;
                    case 2:
                        x = width - 1 - along_pos;
                        y = height - 1 - (d ? depth_h_b : depth_h_a);
                        break;
                    default:
                        x = d ? depth_b : depth_a;
                        y = height - 1 - along_pos;
                        break;
                }
                const Rgb c = get(x, y);
                sr += c.r;
                sg += c.g;
                sb += c.b;
                ++count;
            }
        }
        zones[zone] = {static_cast<std::uint8_t>(sr / count), static_cast<std::uint8_t>(sg / count), static_cast<std::uint8_t>(sb / count)};
    }
    return zones;
}

/// Shift zones clockwise by `rotation` and optionally reverse their order, to
/// match where the strip starts and which way it runs.
inline Zones arrange_zones(const Zones& in, int rotation, bool reverse)
{
    Zones out{};
    const int rot = ((rotation % kEdgeZones) + kEdgeZones) % kEdgeZones;
    for (int i = 0; i < kEdgeZones; ++i) {
        const int src = reverse ? (kEdgeZones - 1 - i) : i;
        out[i] = in[(src + rot) % kEdgeZones];
    }
    return out;
}

/// Start halfway into the arranged loop while preserving every original anchor.
inline std::array<Rgb, 2 * kEdgeZones> start_between_zones(const Zones& zones)
{
    std::array<Rgb, 2 * kEdgeZones> out{};
    const auto average = [](std::uint8_t a, std::uint8_t b) { return static_cast<std::uint8_t>((a + b + 1) / 2); };
    for (int i = 0; i < kEdgeZones; ++i) {
        const auto a = zones[i], b = zones[(i + 1) % kEdgeZones];
        out[2 * i] = {average(a.r, b.r), average(a.g, b.g), average(a.b, b.b)};
        out[2 * i + 1] = b;
    }
    return out;
}

/// Resample the loop from a specific strip position. Slot zero is the top-left
/// corner, halfway before the first screen sample. Positions count clockwise
/// as seen from the front, regardless of the strip's wiring direction.
inline std::array<Rgb, 2 * kEdgeZones> align_to_led(const Zones& zones, int first_led, int led_count, bool reverse)
{
    std::array<Rgb, 2 * kEdgeZones> out{};
    const int units_per_zone = 2 * led_count;
    const int loop_units = kEdgeZones * units_per_zone;
    const int start = 2 * first_led * kEdgeZones - led_count;
    for (int i = 0; i < 2 * kEdgeZones; ++i) {
        const int pos = (start + (reverse ? -i : i) * led_count + 2 * loop_units) % loop_units;
        const int zone = pos / units_per_zone, fraction = pos % units_per_zone;
        const auto a = zones[zone], b = zones[(zone + 1) % kEdgeZones];
        const auto blend = [=](std::uint8_t left, std::uint8_t right) {
            return static_cast<std::uint8_t>((left * (units_per_zone - fraction) + right * fraction + led_count) / units_per_zone);
        };
        out[i] = {blend(a.r, b.r), blend(a.g, b.g), blend(a.b, b.b)};
    }
    return out;
}

/// Exponential smoothing towards the latest sample (called once per send tick).
class ZoneSmoother {
public:
    /// alpha in (0,1]: share of the way to the target per step.
    explicit ZoneSmoother(float alpha = 0.35f) : alpha_(alpha) {}

    /// The first call snaps to the target, so a restart does not fade in from black.
    const Zones& step(const Zones& target)
    {
        if (!primed_) {
            for (int i = 0; i < kEdgeZones; ++i) value_[i][0] = target[i].r, value_[i][1] = target[i].g, value_[i][2] = target[i].b;
            primed_ = true;
        }
        else {
            for (int i = 0; i < kEdgeZones; ++i) {
                const float t[3] = {float(target[i].r), float(target[i].g), float(target[i].b)};
                for (int c = 0; c < 3; ++c) value_[i][c] += (t[c] - value_[i][c]) * alpha_;
            }
        }
        for (int i = 0; i < kEdgeZones; ++i) out_[i] = {to_byte(value_[i][0]), to_byte(value_[i][1]), to_byte(value_[i][2])};
        return out_;
    }

    void reset() { primed_ = false; }

private:
    static std::uint8_t to_byte(float v) { return static_cast<std::uint8_t>(std::clamp(v + 0.5f, 0.0f, 255.0f)); }
    float alpha_;
    bool primed_ = false;
    float value_[kEdgeZones][3] = {};
    Zones out_{};
};

}  // namespace Halo
