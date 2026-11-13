#include <shared/common/remote_render_protocol.h>

#include <limits>
#include <stdexcept>

namespace RemoteRenderProtocol {
namespace {
void append_u16(std::vector<std::uint8_t> &out, std::uint16_t value)
{
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
    out.push_back(static_cast<std::uint8_t>(value & 0xffU));
}

void append_u32(std::vector<std::uint8_t> &out, std::uint32_t value)
{
    out.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xffU));
    out.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xffU));
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
    out.push_back(static_cast<std::uint8_t>(value & 0xffU));
}
constexpr char Base64Alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int base64_value(char c)
{
    if (c >= 'A' && c <= 'Z')
        return c - 'A';
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 26;
    if (c >= '0' && c <= '9')
        return c - '0' + 52;
    if (c == '+')
        return 62;
    if (c == '/')
        return 63;
    return -1;
}
} // namespace

std::string encode_base64(std::span<const std::uint8_t> bytes)
{
    std::string out;
    out.reserve((bytes.size() + 2) / 3 * 4);
    std::size_t i = 0;
    for (; i + 3 <= bytes.size(); i += 3) {
        const std::uint32_t v =
            (static_cast<std::uint32_t>(bytes[i]) << 16U) | (static_cast<std::uint32_t>(bytes[i + 1]) << 8U) | bytes[i + 2];
        out.push_back(Base64Alphabet[(v >> 18U) & 63U]);
        out.push_back(Base64Alphabet[(v >> 12U) & 63U]);
        out.push_back(Base64Alphabet[(v >> 6U) & 63U]);
        out.push_back(Base64Alphabet[v & 63U]);
    }
    if (const auto rest = bytes.size() - i; rest > 0) {
        std::uint32_t v = static_cast<std::uint32_t>(bytes[i]) << 16U;
        if (rest == 2)
            v |= static_cast<std::uint32_t>(bytes[i + 1]) << 8U;
        out.push_back(Base64Alphabet[(v >> 18U) & 63U]);
        out.push_back(Base64Alphabet[(v >> 12U) & 63U]);
        out.push_back(rest == 2 ? Base64Alphabet[(v >> 6U) & 63U] : '=');
        out.push_back('=');
    }
    return out;
}

std::optional<std::vector<std::uint8_t>> decode_base64(std::string_view text)
{
    if (text.size() % 4 != 0)
        return std::nullopt;
    std::vector<std::uint8_t> out;
    out.reserve(text.size() / 4 * 3);
    for (std::size_t i = 0; i < text.size(); i += 4) {
        const bool last = i + 4 == text.size();
        const int padding = last ? (text[i + 3] == '=') + (text[i + 2] == '=') : 0;
        if (padding == 1 && text[i + 2] == '=')
            return std::nullopt;  // "x=y=" style padding
        std::uint32_t v = 0;
        for (int k = 0; k < 4; ++k) {
            const char c = text[i + static_cast<std::size_t>(k)];
            const int value = k >= 4 - padding ? 0 : base64_value(c);
            if (value < 0)
                return std::nullopt;
            v = (v << 6U) | static_cast<std::uint32_t>(value);
        }
        out.push_back(static_cast<std::uint8_t>((v >> 16U) & 0xffU));
        if (padding < 2)
            out.push_back(static_cast<std::uint8_t>((v >> 8U) & 0xffU));
        if (padding < 1)
            out.push_back(static_cast<std::uint8_t>(v & 0xffU));
    }
    return out;
}

FramePacket::FramePacket(std::uint32_t session, std::uint32_t sequence, std::uint16_t width, std::uint16_t height,
                         std::uint16_t chunk_index, std::uint16_t chunk_count, std::uint32_t byte_offset, std::vector<std::uint8_t> chunk,
                         std::uint8_t packet_id, std::uint8_t header_version)
    : UdpPacket(packet_id),
      header_version_(header_version),
      session_(session),
      sequence_(sequence),
      width_(width),
      height_(height),
      chunk_index_(chunk_index),
      chunk_count_(chunk_count),
      byte_offset_(byte_offset),
      chunk_(std::move(chunk))
{
}

std::vector<std::uint8_t> FramePacket::toData() const
{
    std::vector<std::uint8_t> out;
    out.reserve(HeaderBytes + chunk_.size());
    out.push_back(header_version_);
    append_u32(out, session_);
    append_u32(out, sequence_);
    append_u16(out, width_);
    append_u16(out, height_);
    append_u16(out, chunk_index_);
    append_u16(out, chunk_count_);
    append_u32(out, byte_offset_);
    out.insert(out.end(), chunk_.begin(), chunk_.end());
    return out;
}

std::vector<std::unique_ptr<UdpPacket>> make_frame_packets(
    std::uint32_t session, std::uint32_t sequence,
    std::uint16_t width, std::uint16_t height,
    std::span<const std::uint8_t> rgb)
{
    return make_frame_packets(PluginId, Version, session, sequence, width, height, rgb);
}

std::vector<std::unique_ptr<UdpPacket>> make_frame_packets(std::uint8_t packet_id, std::uint8_t header_version, std::uint32_t session,
                                                           std::uint32_t sequence, std::uint16_t width, std::uint16_t height,
                                                           std::span<const std::uint8_t> rgb)
{
    if (session == 0 || width == 0 || height == 0)
        return {};
    const auto expected = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3U;
    if (rgb.size() != expected)
        throw std::invalid_argument("remote RGB frame size does not match dimensions");

    const auto chunks = (rgb.size() + ChunkBytes - 1U) / ChunkBytes;
    if (chunks > std::numeric_limits<std::uint16_t>::max())
        throw std::length_error("remote RGB frame needs too many UDP chunks");

    std::vector<std::unique_ptr<UdpPacket>> packets;
    packets.reserve(chunks);
    const auto chunk_count = static_cast<std::uint16_t>(chunks);
    for (std::uint16_t chunk_index = 0; chunk_index < chunk_count; ++chunk_index) {
        const std::size_t offset = static_cast<std::size_t>(chunk_index) * ChunkBytes;
        const std::size_t count = std::min(ChunkBytes, rgb.size() - offset);
        std::vector<std::uint8_t> chunk(rgb.begin() + static_cast<std::ptrdiff_t>(offset),
                                        rgb.begin() + static_cast<std::ptrdiff_t>(offset + count));
        packets.push_back(std::make_unique<FramePacket>(session, sequence, width, height, chunk_index, chunk_count,
                                                        static_cast<std::uint32_t>(offset), std::move(chunk), packet_id, header_version));
    }
    return packets;
}

} // namespace RemoteRenderProtocol
