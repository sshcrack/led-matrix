#include "RenderOffloadPlugin.h"

#include <shared/common/remote_render_protocol.h>
#include <shared/matrix/diagnostics.h>
#include <shared/matrix/remote_render.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstring>
#include <limits>
#include <span>

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
        RemoteRender::SceneContracts contracts;
        if (const auto advertised = command.find("scene_contracts"); advertised != command.end()
            && advertised->is_object()) {
            for (const auto &[name, version] : advertised->items())
                if (version.is_number_integer() && version > 0
                    && version <= std::numeric_limits<int>::max())
                    contracts.emplace(name, version.get<int>());
        }
        const auto protocol = command.find("protocol");
        const int protocol_version = protocol != command.end() && protocol->is_number_integer()
            && *protocol >= 0 && *protocol <= std::numeric_limits<std::uint8_t>::max()
            ? protocol->get<int>() : 0;
        std::vector<std::string> features;
        if (const auto advertised = command.find("features"); advertised != command.end() && advertised->is_array()) {
            for (const auto& feature : *advertised)
                if (feature.is_string() && features.size() < 64)
                    features.push_back(feature.get<std::string>());
        }
        RemoteRender::report_worker_heartbeat(protocol_version, std::move(contracts), std::move(features));
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

    if (!RemoteRender::session_requested(session))
        return true; // Valid packet for a session that has already been superseded.

    const std::size_t expected = static_cast<std::size_t>(width)
        * static_cast<std::size_t>(height) * 3U;
    const std::size_t expected_offset = static_cast<std::size_t>(chunk_index) * ChunkBytes;
    const std::size_t expected_chunk_bytes = expected_offset < expected
        ? std::min(ChunkBytes, expected - expected_offset)
        : 0;
    if (expected == 0 || expected > MaxFrameBytes || byte_offset != expected_offset
        || chunk_count != (expected + ChunkBytes - 1) / ChunkBytes
        || chunk_bytes != expected_chunk_bytes)
        return false;

    auto& diagnostics = Diagnostics::RuntimeDiagnostics::instance();
    if (session == published_session_ && sequence <= published_sequence_) {
        // Late or duplicate chunk of a frame that is already shown or superseded.
        diagnostics.record_remote_frame_event(Diagnostics::RemoteFrameEvent::LateChunk);
        return true;
    }

    auto* assembly = assembly_for(session, sequence);
    if (!assembly) {
        diagnostics.record_remote_frame_event(Diagnostics::RemoteFrameEvent::LateChunk);
        return true;  // Older than every frame still being assembled.
    }
    if (!assembly->in_use) {
        assembly->in_use = true;
        assembly->session = session;
        assembly->sequence = sequence;
        assembly->width = width;
        assembly->height = height;
        assembly->chunk_count = chunk_count;
        assembly->received_count = 0;
        // Exact chunk geometry and the received bitmap ensure every byte is
        // overwritten before publication. Reuse storage without clearing the
        // whole RGB frame for each sequence (including abandoned frames).
        assembly->frame.resize(expected);
        assembly->received.assign(chunk_count, 0);
    }

    if (width != assembly->width || height != assembly->height || chunk_count != assembly->chunk_count ||
        assembly->frame.size() != expected)
        return false;

    if (assembly->received[chunk_index]) {
        diagnostics.record_remote_frame_event(Diagnostics::RemoteFrameEvent::DuplicateChunk);
        return true;
    }

    std::memcpy(assembly->frame.data() + byte_offset, data + HeaderSize, chunk_bytes);
    assembly->received[chunk_index] = 1;
    if (++assembly->received_count != assembly->chunk_count)
        return true;

    const bool newer_pending = std::any_of(assemblies_.begin(), assemblies_.end(), [&](const Assembly& other) {
        return other.in_use && other.session == session && other.sequence > sequence;
    });
    const bool submitted = RemoteRender::submit_frame(session, sequence, width, height,
                                                      std::span<const std::uint8_t>(assembly->frame.data(), assembly->frame.size()));
    assembly->in_use = false;
    if (!submitted)
        return false;
    diagnostics.record_remote_frame_event(Diagnostics::RemoteFrameEvent::Completed);
    if (newer_pending)
        diagnostics.record_remote_frame_event(Diagnostics::RemoteFrameEvent::ReorderRecovered);
    published_session_ = session;
    published_sequence_ = sequence;
    drop_older(session, sequence);  // They can never be shown any more.
    return true;
}

RenderOffloadPlugin::Assembly* RenderOffloadPlugin::assembly_for(std::uint32_t session, std::uint32_t sequence)
{
    Assembly* free_slot = nullptr;
    Assembly* oldest = nullptr;
    for (auto& assembly : assemblies_) {
        if (assembly.in_use && assembly.session != session) {
            assembly.in_use = false;  // A superseded session's frames never publish.
        }
        if (!assembly.in_use) {
            if (!free_slot)
                free_slot = &assembly;
            continue;
        }
        if (assembly.sequence == sequence)
            return &assembly;
        if (!oldest || assembly.sequence < oldest->sequence)
            oldest = &assembly;
    }
    if (free_slot)
        return free_slot;
    if (!oldest || sequence < oldest->sequence)
        return nullptr;
    Diagnostics::RuntimeDiagnostics::instance().record_remote_frame_event(Diagnostics::RemoteFrameEvent::Incomplete);
    oldest->in_use = false;
    return oldest;
}

void RenderOffloadPlugin::drop_older(std::uint32_t session, std::uint32_t sequence)
{
    for (auto& assembly : assemblies_) {
        if (assembly.in_use && assembly.session == session && assembly.sequence < sequence) {
            assembly.in_use = false;
            Diagnostics::RuntimeDiagnostics::instance().record_remote_frame_event(Diagnostics::RemoteFrameEvent::Incomplete);
        }
    }
}
