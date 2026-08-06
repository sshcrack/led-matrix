#include <shared/common/audio_protocol.h>

#include <algorithm>
#include <bit>
#include <limits>

namespace AudioProtocol {
namespace {
void append_u16(std::vector<uint8_t> &out, uint16_t value) {
    out.push_back(static_cast<uint8_t>(value));
    out.push_back(static_cast<uint8_t>(value >> 8U));
}

void append_u32(std::vector<uint8_t> &out, uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8)
        out.push_back(static_cast<uint8_t>(value >> shift));
}

void append_u64(std::vector<uint8_t> &out, uint64_t value) {
    for (int shift = 0; shift < 64; shift += 8)
        out.push_back(static_cast<uint8_t>(value >> shift));
}

void append_float(std::vector<uint8_t> &out, float value) {
    append_u32(out, std::bit_cast<uint32_t>(value));
}

bool take_u16(std::span<const uint8_t> data, size_t &offset, uint16_t &value) {
    if (offset + 2 > data.size()) return false;
    value = static_cast<uint16_t>(data[offset]) |
            static_cast<uint16_t>(data[offset + 1] << 8U);
    offset += 2;
    return true;
}

bool take_u32(std::span<const uint8_t> data, size_t &offset, uint32_t &value) {
    if (offset + 4 > data.size()) return false;
    value = static_cast<uint32_t>(data[offset]) |
            (static_cast<uint32_t>(data[offset + 1]) << 8U) |
            (static_cast<uint32_t>(data[offset + 2]) << 16U) |
            (static_cast<uint32_t>(data[offset + 3]) << 24U);
    offset += 4;
    return true;
}

bool take_u64(std::span<const uint8_t> data, size_t &offset, uint64_t &value) {
    if (offset + 8 > data.size()) return false;
    value = 0;
    for (int shift = 0; shift < 64; shift += 8)
        value |= static_cast<uint64_t>(data[offset++]) << shift;
    return true;
}

bool take_float(std::span<const uint8_t> data, size_t &offset, float &value) {
    uint32_t bits = 0;
    if (!take_u32(data, offset, bits)) return false;
    value = std::bit_cast<float>(bits);
    return true;
}

void set_error(std::string *error, const char *message) {
    if (error) *error = message;
}
} // namespace

std::vector<uint8_t> encode(const Frame &frame) {
    const uint16_t spectrum_count = static_cast<uint16_t>(
        std::min(frame.spectrum.size(), MaxSpectrumBins));
    const uint16_t waveform_count = static_cast<uint16_t>(
        std::min(frame.waveform.size(), MaxWaveformPoints));

    std::vector<uint8_t> result;
    result.reserve(56 + FeatureCount * sizeof(float) +
                   (spectrum_count + waveform_count) * sizeof(float));

    append_u32(result, Magic);
    append_u16(result, Version);
    append_u16(result, frame.flags);
    append_u32(result, frame.sequence);
    append_u32(result, frame.timestamp_ms);
    append_u64(result, frame.beat_counter);
    append_u64(result, frame.onset_counter);
    append_u64(result, frame.drop_counter);
    append_u64(result, frame.section_counter);
    append_u16(result, static_cast<uint16_t>(FeatureCount));
    append_u16(result, spectrum_count);
    append_u16(result, waveform_count);
    append_u16(result, 0);

    for (float value : frame.features)
        append_float(result, value);
    for (size_t i = 0; i < spectrum_count; ++i)
        append_float(result, frame.spectrum[i]);
    for (size_t i = 0; i < waveform_count; ++i)
        append_float(result, frame.waveform[i]);

    return result;
}

bool decode(std::span<const uint8_t> bytes, Frame &frame, std::string *error) {
    size_t offset = 0;
    uint32_t magic = 0;
    uint16_t version = 0;
    uint16_t feature_count = 0;
    uint16_t spectrum_count = 0;
    uint16_t waveform_count = 0;
    uint16_t reserved = 0;

    if (!take_u32(bytes, offset, magic) || magic != Magic) {
        set_error(error, "invalid music packet magic");
        return false;
    }
    if (!take_u16(bytes, offset, version) || version != Version) {
        set_error(error, "unsupported music packet version");
        return false;
    }
    if (!take_u16(bytes, offset, frame.flags) ||
        !take_u32(bytes, offset, frame.sequence) ||
        !take_u32(bytes, offset, frame.timestamp_ms) ||
        !take_u64(bytes, offset, frame.beat_counter) ||
        !take_u64(bytes, offset, frame.onset_counter) ||
        !take_u64(bytes, offset, frame.drop_counter) ||
        !take_u64(bytes, offset, frame.section_counter) ||
        !take_u16(bytes, offset, feature_count) ||
        !take_u16(bytes, offset, spectrum_count) ||
        !take_u16(bytes, offset, waveform_count) ||
        !take_u16(bytes, offset, reserved)) {
        set_error(error, "truncated music packet header");
        return false;
    }

    if (feature_count != FeatureCount || spectrum_count > MaxSpectrumBins ||
        waveform_count > MaxWaveformPoints) {
        set_error(error, "invalid music packet dimensions");
        return false;
    }

    const size_t expected = offset +
        (static_cast<size_t>(feature_count) + spectrum_count + waveform_count) * sizeof(float);
    if (bytes.size() != expected) {
        set_error(error, "music packet size mismatch");
        return false;
    }

    for (float &value : frame.features) {
        if (!take_float(bytes, offset, value)) return false;
    }
    frame.spectrum.resize(spectrum_count);
    for (float &value : frame.spectrum) {
        if (!take_float(bytes, offset, value)) return false;
    }
    frame.waveform.resize(waveform_count);
    for (float &value : frame.waveform) {
        if (!take_float(bytes, offset, value)) return false;
    }
    return true;
}

} // namespace AudioProtocol
