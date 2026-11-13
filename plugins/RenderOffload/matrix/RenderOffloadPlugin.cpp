#include "RenderOffloadPlugin.h"

#include <shared/common/remote_render_protocol.h>
#include <shared/matrix/frame_stream.h>
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
    const auto header = FrameStream::parse_chunk(data, size, ProtocolVersion);
    if (!header)
        return false;
    if (!RemoteRender::session_requested(header->session))
        return true; // Valid packet for a session that has already been superseded.

    const auto done = assembler_.add(*header, data + HeaderSize);
    if (!done)
        return true;
    if (!RemoteRender::submit_frame(done->session, done->sequence, done->width, done->height, done->rgb))
        return false;
    assembler_.published(*done);
    return true;
}
