#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace AudioProtocol {

constexpr uint32_t Magic = 0x3153554dU; // "MUS1" in little endian.
constexpr uint16_t Version = 1;
constexpr size_t MaxSpectrumBins = 256;
constexpr size_t MaxWaveformPoints = 256;

enum class Feature : uint16_t {
    Rms = 0,
    Peak,
    Loudness,
    LoudnessFast,
    LoudnessSlow,
    SubBass,
    Bass,
    LowMid,
    Mid,
    HighMid,
    Treble,
    Air,
    SpectralCentroid,
    SpectralRolloff,
    SpectralFlatness,
    SpectralFlux,
    OnsetStrength,
    Kick,
    Snare,
    Hihat,
    StereoWidth,
    StereoBalance,
    StereoCorrelation,
    EnergyTrend,
    SectionChange,
    Drop,
    Bpm,
    BeatPhase,
    BeatConfidence,
    BeatStrength,
    TempoStability,
    Silence,
    Count
};

constexpr size_t FeatureCount = static_cast<size_t>(Feature::Count);

enum EventFlag : uint16_t {
    BeatEvent = 1U << 0,
    OnsetEvent = 1U << 1,
    KickEvent = 1U << 2,
    SnareEvent = 1U << 3,
    HihatEvent = 1U << 4,
    DropEvent = 1U << 5,
    SectionEvent = 1U << 6,
    Silent = 1U << 7,
};

struct Frame {
    uint32_t sequence = 0;
    uint32_t timestamp_ms = 0;
    uint16_t flags = 0;

    uint64_t beat_counter = 0;
    uint64_t onset_counter = 0;
    uint64_t drop_counter = 0;
    uint64_t section_counter = 0;

    std::array<float, FeatureCount> features{};
    std::vector<float> spectrum;
    std::vector<float> waveform;

    [[nodiscard]] float feature(Feature id) const {
        return features[static_cast<size_t>(id)];
    }

    void set(Feature id, float value) {
        features[static_cast<size_t>(id)] = value;
    }

    [[nodiscard]] bool event(EventFlag flag) const {
        return (flags & static_cast<uint16_t>(flag)) != 0;
    }
};

std::vector<uint8_t> encode(const Frame &frame);
bool decode(std::span<const uint8_t> bytes, Frame &frame, std::string *error = nullptr);

} // namespace AudioProtocol
