#include "RenderOffloadPlugin.h"

#include <algorithm>
#include <cstring>
#include <span>

#include <shared/common/remote_render_protocol.h>
#include <shared/matrix/remote_render.h>
#include <spdlog/spdlog.h>

namespace {
constexpr std::uint8_t PacketId = RemoteRenderProtocol::PluginId;
constexpr std::uint8_t ProtocolVersion = RemoteRenderProtocol::Version;
constexpr std::size_t HeaderSize = RemoteRenderProtocol::HeaderBytes;
constexpr std::size_t MaxFrameBytes = 4U * 1024U * 1024U;
constexpr std::uint16_t MaxChunks = 4096;
constexpr std::size_t ChunkBytes = RemoteRenderProtocol::ChunkBytes;

std::uint16_t read_u16(const std::uint8_t *p)
{
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(p[0]) << 8U) | p[1]);
}

std::uint32_t read_u32(const std::uint8_t *p)
{
    return (static_cast<std::uint32_t>(p[0]) << 24U)
         | (static_cast<std::uint32_t>(p[1]) << 16U)
         | (static_cast<std::uint32_t>(p[2]) << 8U)
         | static_cast<std::uint32_t>(p[3]);
}
} // namespace

REGISTER_PLUGIN(RenderOffload, RenderOffloadPlugin)

std::optional<std::string> RenderOffloadPlugin::before_server_init()
{
    RemoteRender::set_command_sender([this](const nlohmann::json &command) {
        send_msg_to_desktop(command.dump());
    });
    return std::nullopt;
}

std::optional<std::string> RenderOffloadPlugin::pre_exit()
{
    RemoteRender::stop();
    RemoteRender::clear_command_sender();
    return std::nullopt;
}

std::optional<std::vector<std::string>> RenderOffloadPlugin::on_websocket_open()
{
    if (const auto command = RemoteRender::reconnect_command(); command.has_value())
        return std::vector<std::string>{command->dump()};
    return std::nullopt;
}

void RenderOffloadPlugin::on_websocket_message(const std::string &message)
{
    try {
        const auto command = nlohmann::json::parse(message);
        if (command.value("op", std::string{}) != "worker_heartbeat")
            return;
        RemoteRender::report_worker_heartbeat(
            command.value("protocol", 0),
            command.value("scenes", std::vector<std::string>{}));
    } catch (const std::exception &e) {
        spdlog::debug("Ignoring invalid RenderOffload worker message: {}", e.what());
    }
}

bool RenderOffloadPlugin::on_udp_packet(uint8_t plugin_id, const uint8_t *data, size_t size)
{
    if (plugin_id != PacketId)
        return false;
    if (!data || size < HeaderSize || data[0] != ProtocolVersion)
        return false;

    const auto session = read_u32(data + 1);
    const auto sequence = read_u32(data + 5);
    const auto width = static_cast<int>(read_u16(data + 9));
    const auto height = static_cast<int>(read_u16(data + 11));
    const auto chunk_index = read_u16(data + 13);
    const auto chunk_count = read_u16(data + 15);
    const auto byte_offset = static_cast<std::size_t>(read_u32(data + 17));
    const std::size_t chunk_bytes = size - HeaderSize;

    if (session == 0 || width <= 0 || height <= 0 || chunk_count == 0
        || chunk_count > MaxChunks || chunk_index >= chunk_count)
        return false;

    const auto remote_status = RemoteRender::status();
    if (!remote_status.requested || remote_status.session != session)
        return true; // Valid packet for a session that has already been superseded.

    const std::size_t expected = static_cast<std::size_t>(width)
        * static_cast<std::size_t>(height) * 3U;
    const std::size_t expected_offset = static_cast<std::size_t>(chunk_index) * ChunkBytes;
    const std::size_t expected_chunk_bytes = expected_offset < expected
        ? std::min(ChunkBytes, expected - expected_offset)
        : 0;
    if (expected == 0 || expected > MaxFrameBytes || byte_offset != expected_offset
        || chunk_bytes != expected_chunk_bytes)
        return false;

    if (assembly_session_ == session && sequence < assembly_sequence_)
        return true;

    if (assembly_session_ != session || sequence > assembly_sequence_) {
        assembly_session_ = session;
        assembly_sequence_ = sequence;
        assembly_width_ = width;
        assembly_height_ = height;
        assembly_chunk_count_ = chunk_count;
        assembly_received_count_ = 0;
        assembly_submitted_ = false;
        assembly_frame_.assign(expected, 0);
        assembly_received_.assign(chunk_count, 0);
    }

    if (sequence != assembly_sequence_ || width != assembly_width_ || height != assembly_height_
        || chunk_count != assembly_chunk_count_ || assembly_frame_.size() != expected)
        return false;

    if (assembly_submitted_ || assembly_received_[chunk_index])
        return true;

    std::memcpy(assembly_frame_.data() + byte_offset, data + HeaderSize, chunk_bytes);
    assembly_received_[chunk_index] = 1;
    ++assembly_received_count_;

    if (assembly_received_count_ != assembly_chunk_count_)
        return true;

    assembly_submitted_ = RemoteRender::submit_frame(
        session, sequence, width, height,
        std::span<const std::uint8_t>(assembly_frame_.data(), assembly_frame_.size()));
    // Keep the sequence number so late duplicate chunks for this completed
    // frame are harmless; the next newer sequence will replace the buffer.
    return assembly_submitted_;
}
