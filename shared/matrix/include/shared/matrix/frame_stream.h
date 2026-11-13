#pragma once

#include <shared/common/frame_stream.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace rgb_matrix {
class FrameCanvas;
}

/// Matrix side of desktop frame streams (see shared/common/frame_stream.h).
namespace FrameStream {

/// The fixed chunk header shared with RemoteRender (21 bytes).
struct ChunkHeader {
    std::uint32_t session = 0;
    std::uint32_t sequence = 0;
    int width = 0;
    int height = 0;
    std::uint16_t chunk_index = 0;
    std::uint16_t chunk_count = 0;
    std::size_t byte_offset = 0;
    std::size_t chunk_bytes = 0;
    std::size_t frame_bytes = 0;
};

/// Parse and validate one chunk whose first byte must be `header_version`.
/// Empty when the datagram is not a well-formed chunk of a sane frame.
[[nodiscard]] std::optional<ChunkHeader> parse_chunk(const std::uint8_t* data, std::size_t size, std::uint8_t header_version);

/// Bounded reassembly of chunked frames. Several frames are kept, so chunks of
/// frame N that arrive after the first chunks of N+1 (Wi-Fi reordering) still
/// complete N. Counts outcomes in `/diagnostics` under `udp.frame_streams.<name>`.
/// Single-threaded (the UDP receive thread).
class Assembler {
public:
    explicit Assembler(std::string name);

    struct Completed {
        std::uint32_t session = 0;
        std::uint32_t sequence = 0;
        int width = 0;
        int height = 0;
        std::span<const std::uint8_t> rgb;  ///< valid until the next add()
        bool newer_pending = false;         ///< a newer frame had already started arriving
    };

    /// Add one chunk (`payload` holds `header.chunk_bytes`). Returns the frame
    /// it completed. The caller decides whether to show it and then calls
    /// published(); until then older frames still count as pending.
    std::optional<Completed> add(const ChunkHeader& header, const std::uint8_t* payload);
    /// The frame `add()` returned was shown: older frames can never be shown any more.
    void published(const Completed& frame);
    /// Forget everything (session restart).
    void clear();

    [[nodiscard]] const std::string& name() const { return name_; }

private:
    struct Slot {
        bool in_use = false;
        std::uint32_t session = 0;
        std::uint32_t sequence = 0;
        int width = 0;
        int height = 0;
        std::uint16_t chunk_count = 0;
        std::size_t received_count = 0;
        std::vector<std::uint8_t> frame;
        std::vector<std::uint8_t> received;
    };
    static constexpr std::size_t Slots = 3;

    Slot* slot_for(std::uint32_t session, std::uint32_t sequence);

    std::string name_;
    std::array<Slot, Slots> slots_;
    std::uint32_t published_session_ = 0;
    std::uint32_t published_sequence_ = 0;
};

/// The newest complete frame of a stream, top-down RGB888.
struct Frame {
    std::uint32_t sequence = 0;
    int width = 0;
    int height = 0;
    std::chrono::steady_clock::time_point received_at{};
    std::vector<std::uint8_t> rgb;
};

/// One desktop stream (shader, video, music video): chunked frames from current
/// desktops and whole-frame datagrams from older ones, latest frame wins.
/// on_packet() runs on the UDP thread; the readers are lock-free.
class Receiver {
public:
    /// `legacy_bottom_up`: whole-frame datagrams from older desktops are
    /// bottom-up (OpenGL readback) and get flipped on receipt.
    Receiver(std::string name, bool legacy_bottom_up = false);

    /// A chunk or a legacy whole frame (Constants::width x height). False when
    /// the payload is neither.
    bool on_packet(const std::uint8_t* data, std::size_t size);

    [[nodiscard]] std::shared_ptr<const Frame> latest() const;
    /// Milliseconds since the newest frame arrived, empty without one.
    [[nodiscard]] std::optional<double> age_ms() const;
    /// Drop the current frame (the stream changed content).
    void clear();
    /// Copy the newest frame to the canvas. False (canvas untouched) without
    /// one; a frame of another size is copied into the overlapping area.
    bool copy_latest(rgb_matrix::FrameCanvas* canvas, int width, int height) const;

private:
    void publish(std::uint32_t sequence, int width, int height, std::span<const std::uint8_t> rgb, bool flip);

    Assembler assembler_;
    bool legacy_bottom_up_;
    std::uint32_t session_ = 0;          // UDP thread
    std::uint32_t legacy_sequence_ = 0;  // UDP thread
    std::atomic<std::shared_ptr<const Frame>> latest_{};
};

}  // namespace FrameStream
