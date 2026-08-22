#include "RenderFramePacket.h"

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

RenderFramePacket::RenderFramePacket(
    std::uint32_t session, std::uint32_t sequence,
    std::uint16_t width, std::uint16_t height,
    std::uint16_t chunk_index, std::uint16_t chunk_count,
    std::uint32_t byte_offset, std::vector<std::uint8_t> chunk)
    : UdpPacket(0x05), session_(session), sequence_(sequence),
      width_(width), height_(height), chunk_index_(chunk_index),
      chunk_count_(chunk_count), byte_offset_(byte_offset), chunk_(std::move(chunk)) {}

std::vector<std::uint8_t> RenderFramePacket::toData() const
{
    std::vector<std::uint8_t> out;
    out.reserve(21 + chunk_.size());
    out.push_back(1); // protocol version
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
