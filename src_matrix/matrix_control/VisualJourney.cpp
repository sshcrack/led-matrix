#include "VisualJourney.h"

#include <array>

namespace {
std::uint64_t mix(std::uint64_t value)
{
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}
}

VisualJourney::VisualJourney(std::uint64_t seed)
    : seed_(seed), duration_ms_((24 + mix(seed) % 13) * 60000) {}

void VisualJourney::advance(std::uint64_t elapsed_ms)
{
    cycle_ += elapsed_ms / duration_ms_;
    elapsed_ms_ += elapsed_ms % duration_ms_;
    cycle_ += elapsed_ms_ / duration_ms_;
    elapsed_ms_ %= duration_ms_;
}

VisualJourney::Frame VisualJourney::frame() const
{
    constexpr std::array<float, 6> boundaries{0.0f, 0.18f, 0.46f, 0.68f, 0.84f, 1.0f};
    constexpr std::array<std::string_view, 5> phases{"settle", "explore", "rise", "crest", "release"};
    constexpr std::array<std::string_view, 4> motifs{"organic", "depth", "geometric", "particles"};
    constexpr std::array<float, 6> intensity{0.24f, 0.30f, 0.48f, 0.72f, 0.62f, 0.24f};
    constexpr std::array<float, 6> motion{0.26f, 0.32f, 0.52f, 0.74f, 0.56f, 0.26f};
    constexpr std::array<float, 6> dwell{1.45f, 1.35f, 1.05f, 0.85f, 1.0f, 1.45f};
    const float progress = static_cast<float>(elapsed_ms_) / static_cast<float>(duration_ms_);
    std::size_t phase = 0;
    while (phase + 1 < phases.size() && progress >= boundaries[phase + 1])
        ++phase;
    const float local = (progress - boundaries[phase]) / (boundaries[phase + 1] - boundaries[phase]);
    const float smooth = local * local * (3.0f - 2.0f * local);
    const auto interpolate = [&](const auto& values) {
        return values[phase] + (values[phase + 1] - values[phase]) * smooth;
    };
    const auto motif = (mix(seed_) + cycle_) % motifs.size();
    return {phases[phase], motifs[motif], progress, local,
            interpolate(intensity), interpolate(motion), interpolate(dwell)};
}
