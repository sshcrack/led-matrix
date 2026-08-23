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
} // namespace

FramePacket::FramePacket(
    std::uint32_t session, std::uint32_t sequence,
    std::uint16_t width, std::uint16_t height,
    std::uint16_t chunk_index, std::uint16_t chunk_count,
    std::uint32_t byte_offset, std::vector<std::uint8_t> chunk)
    : UdpPacket(PluginId), session_(session), sequence_(sequence),
      width_(width), height_(height), chunk_index_(chunk_index),
      chunk_count_(chunk_count), byte_offset_(byte_offset), chunk_(std::move(chunk)) {}

std::vector<std::uint8_t> FramePacket::toData() const
{
    std::vector<std::uint8_t> out;
    out.reserve(HeaderBytes + chunk_.size());
    out.push_back(Version);
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
        packets.push_back(std::make_unique<FramePacket>(
            session, sequence, width, height, chunk_index, chunk_count,
            static_cast<std::uint32_t>(offset), std::move(chunk)));
    }
    return packets;
}

} // namespace RemoteRenderProtocol
