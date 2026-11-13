#pragma once

#include <shared/common/macro.h>
#include <shared/common/udp/packet.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace RemoteRenderProtocol {

inline constexpr std::uint8_t PluginId = 0x05;
// v4 replaces source fingerprints with explicit per-scene data contracts.
// Bump for incompatible command, runtime-input, audio or frame-wire changes.
inline constexpr std::uint8_t Version = 4;
inline constexpr std::size_t HeaderBytes = 21;
inline constexpr std::size_t ChunkBytes = 1200;

/// Additive capabilities a worker lists in its heartbeat's `features` array.
/// The Pi only uses a capability the connected worker advertised, so older
/// workers keep receiving the original encoding.
namespace Features {
/// The transition start command may carry the outgoing frame as `from_base64`
/// (base64 of the RGB888 bytes) instead of `from` (a JSON array of numbers).
/// The array costs the Pi tens of milliseconds to build and is ~2.7x larger.
inline constexpr std::string_view TransitionSnapshotBase64 = "transition_snapshot_base64";
}  // namespace Features

SHARED_COMMON_API std::string encode_base64(std::span<const std::uint8_t> bytes);
/// Empty on malformed input (wrong length, invalid characters or padding).
SHARED_COMMON_API std::optional<std::vector<std::uint8_t>> decode_base64(std::string_view text);

class SHARED_COMMON_API FramePacket final : public UdpPacket {
public:
    FramePacket(std::uint32_t session, std::uint32_t sequence,
                std::uint16_t width, std::uint16_t height,
                std::uint16_t chunk_index, std::uint16_t chunk_count,
                std::uint32_t byte_offset,
                std::vector<std::uint8_t> chunk);

    [[nodiscard]] std::vector<std::uint8_t> toData() const override;

private:
    std::uint32_t session_;
    std::uint32_t sequence_;
    std::uint16_t width_;
    std::uint16_t height_;
    std::uint16_t chunk_index_;
    std::uint16_t chunk_count_;
    std::uint32_t byte_offset_;
    std::vector<std::uint8_t> chunk_;
};

/// Split one RGB888 frame into MTU-safe datagrams. The matrix receiver keeps
/// only the newest complete frame, so packet loss costs freshness rather than
/// building a latency queue.
SHARED_COMMON_API std::vector<std::unique_ptr<UdpPacket>> make_frame_packets(
    std::uint32_t session, std::uint32_t sequence,
    std::uint16_t width, std::uint16_t height,
    std::span<const std::uint8_t> rgb);

} // namespace RemoteRenderProtocol
