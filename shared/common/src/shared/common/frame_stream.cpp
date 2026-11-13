#include "shared/common/frame_stream.h"

#include <chrono>
#include <limits>

#include "shared/common/remote_render_protocol.h"

namespace FrameStream {

namespace {
std::uint32_t new_session()
{
    // Seconds since the epoch: later desktop runs get larger sessions.
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    const auto session = static_cast<std::uint32_t>(seconds & 0xffffffffLL);
    return session == 0 ? 1 : session;
}
}  // namespace

Sender::Sender(std::uint8_t packet_id) : packet_id_(packet_id), session_(new_session()) {}

void Sender::reset()
{
    chunked_.store(false, std::memory_order_release);
}

bool Sender::on_message(std::string_view message)
{
    if (message != CapabilityMessage)
        return false;
    chunked_.store(true, std::memory_order_release);
    return true;
}

std::vector<std::unique_ptr<UdpPacket>> Sender::chunk_packets(std::span<const std::uint8_t> rgb, int width, int height)
{
    const bool fits = width > 0 && height > 0 && width <= std::numeric_limits<std::uint16_t>::max() &&
                      height <= std::numeric_limits<std::uint16_t>::max() &&
                      rgb.size() == static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3U;
    if (!fits)
        return {};
    if (++sequence_ == 0)
        sequence_ = 1;
    return RemoteRenderProtocol::make_frame_packets(packet_id_, HeaderVersion, session_, sequence_, static_cast<std::uint16_t>(width),
                                                    static_cast<std::uint16_t>(height), rgb);
}

std::vector<std::unique_ptr<UdpPacket>> Sender::packets(std::vector<std::uint8_t> rgb, int width, int height, const LegacyPacket& legacy)
{
    if (chunked()) {
        if (auto chunks = chunk_packets(rgb, width, height); !chunks.empty())
            return chunks;
    }
    std::vector<std::unique_ptr<UdpPacket>> result;
    if (auto packet = legacy(std::move(rgb)))
        result.push_back(std::move(packet));
    return result;
}

}  // namespace FrameStream
