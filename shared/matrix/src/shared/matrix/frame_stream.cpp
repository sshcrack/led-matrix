#include "shared/matrix/frame_stream.h"

#include <algorithm>
#include <cstring>

#include "led-matrix.h"
#include "shared/common/frame_stream.h"
#include "shared/common/remote_render_protocol.h"
#include "shared/matrix/canvas_consts.h"
#include "shared/matrix/diagnostics.h"

namespace FrameStream {

namespace {
constexpr std::size_t HeaderBytes = RemoteRenderProtocol::HeaderBytes;
constexpr std::size_t ChunkBytes = RemoteRenderProtocol::ChunkBytes;
constexpr std::size_t MaxFrameBytes = 4U * 1024U * 1024U;
constexpr std::uint16_t MaxChunks = 4096;

std::uint16_t read_u16(const std::uint8_t* p)
{
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(p[0]) << 8U) | p[1]);
}

std::uint32_t read_u32(const std::uint8_t* p)
{
    return (static_cast<std::uint32_t>(p[0]) << 24U) | (static_cast<std::uint32_t>(p[1]) << 16U) |
           (static_cast<std::uint32_t>(p[2]) << 8U) | static_cast<std::uint32_t>(p[3]);
}

void record(const std::string& stream, Diagnostics::FrameStreamEvent event)
{
    Diagnostics::RuntimeDiagnostics::instance().record_frame_stream_event(stream, event);
}
}  // namespace

std::optional<ChunkHeader> parse_chunk(const std::uint8_t* data, std::size_t size, std::uint8_t header_version)
{
    if (!data || size < HeaderBytes || data[0] != header_version)
        return std::nullopt;
    ChunkHeader h;
    h.session = read_u32(data + 1);
    h.sequence = read_u32(data + 5);
    h.width = static_cast<int>(read_u16(data + 9));
    h.height = static_cast<int>(read_u16(data + 11));
    h.chunk_index = read_u16(data + 13);
    h.chunk_count = read_u16(data + 15);
    h.byte_offset = static_cast<std::size_t>(read_u32(data + 17));
    h.chunk_bytes = size - HeaderBytes;
    if (h.session == 0 || h.width <= 0 || h.height <= 0 || h.chunk_count == 0 || h.chunk_count > MaxChunks ||
        h.chunk_index >= h.chunk_count)
        return std::nullopt;
    h.frame_bytes = static_cast<std::size_t>(h.width) * static_cast<std::size_t>(h.height) * 3U;
    const std::size_t expected_offset = static_cast<std::size_t>(h.chunk_index) * ChunkBytes;
    const std::size_t expected_chunk_bytes = expected_offset < h.frame_bytes ? std::min(ChunkBytes, h.frame_bytes - expected_offset) : 0;
    if (h.frame_bytes > MaxFrameBytes || h.byte_offset != expected_offset ||
        h.chunk_count != (h.frame_bytes + ChunkBytes - 1) / ChunkBytes || h.chunk_bytes != expected_chunk_bytes)
        return std::nullopt;
    return h;
}

// ---- Assembler ----

Assembler::Assembler(std::string name) : name_(std::move(name)) {}

void Assembler::clear()
{
    for (auto& slot : slots_) slot.in_use = false;
    published_session_ = published_sequence_ = 0;
}

Assembler::Slot* Assembler::slot_for(std::uint32_t session, std::uint32_t sequence)
{
    Slot* free_slot = nullptr;
    Slot* oldest = nullptr;
    for (auto& slot : slots_) {
        if (slot.in_use && slot.session != session)
            slot.in_use = false;  // A superseded session's frames never publish.
        if (!slot.in_use) {
            if (!free_slot)
                free_slot = &slot;
            continue;
        }
        if (slot.sequence == sequence)
            return &slot;
        if (!oldest || slot.sequence < oldest->sequence)
            oldest = &slot;
    }
    if (free_slot)
        return free_slot;
    if (!oldest || sequence < oldest->sequence)
        return nullptr;
    record(name_, Diagnostics::FrameStreamEvent::Incomplete);
    oldest->in_use = false;
    return oldest;
}

std::optional<Assembler::Completed> Assembler::add(const ChunkHeader& h, const std::uint8_t* payload)
{
    if (h.session == published_session_ && h.sequence <= published_sequence_) {
        // Late or duplicate chunk of a frame that is already shown or superseded.
        record(name_, Diagnostics::FrameStreamEvent::LateChunk);
        return std::nullopt;
    }
    auto* slot = slot_for(h.session, h.sequence);
    if (!slot) {
        record(name_, Diagnostics::FrameStreamEvent::LateChunk);
        return std::nullopt;  // Older than every frame still being assembled.
    }
    if (!slot->in_use) {
        slot->in_use = true;
        slot->session = h.session;
        slot->sequence = h.sequence;
        slot->width = h.width;
        slot->height = h.height;
        slot->chunk_count = h.chunk_count;
        slot->received_count = 0;
        // Exact chunk geometry and the received bitmap ensure every byte is
        // overwritten before publication; reuse the storage without clearing it.
        slot->frame.resize(h.frame_bytes);
        slot->received.assign(h.chunk_count, 0);
    }
    if (h.width != slot->width || h.height != slot->height || h.chunk_count != slot->chunk_count || slot->frame.size() != h.frame_bytes)
        return std::nullopt;
    if (slot->received[h.chunk_index]) {
        record(name_, Diagnostics::FrameStreamEvent::DuplicateChunk);
        return std::nullopt;
    }
    std::memcpy(slot->frame.data() + h.byte_offset, payload, h.chunk_bytes);
    slot->received[h.chunk_index] = 1;
    if (++slot->received_count != slot->chunk_count)
        return std::nullopt;

    slot->in_use = false;
    Completed done{.session = slot->session,
                   .sequence = slot->sequence,
                   .width = slot->width,
                   .height = slot->height,
                   .rgb = std::span<const std::uint8_t>(slot->frame.data(), slot->frame.size())};
    done.newer_pending = std::any_of(slots_.begin(), slots_.end(), [&](const Slot& other) {
        return other.in_use && other.session == done.session && other.sequence > done.sequence;
    });
    return done;
}

void Assembler::published(const Completed& frame)
{
    record(name_, Diagnostics::FrameStreamEvent::Completed);
    if (frame.newer_pending)
        record(name_, Diagnostics::FrameStreamEvent::ReorderRecovered);
    if (frame.session == published_session_ && frame.sequence > published_sequence_ + 1)
        Diagnostics::RuntimeDiagnostics::instance().record_frame_stream_event(name_, Diagnostics::FrameStreamEvent::MissedFrame,
                                                                              frame.sequence - published_sequence_ - 1);
    published_session_ = frame.session;
    published_sequence_ = frame.sequence;
    for (auto& slot : slots_) {
        if (slot.in_use && slot.session == frame.session && slot.sequence < frame.sequence) {
            slot.in_use = false;  // It can never be shown any more.
            record(name_, Diagnostics::FrameStreamEvent::Incomplete);
        }
    }
}

// ---- Receiver ----

Receiver::Receiver(std::string name, bool legacy_bottom_up) : assembler_(std::move(name)), legacy_bottom_up_(legacy_bottom_up) {}

void Receiver::publish(std::uint32_t sequence, int width, int height, std::span<const std::uint8_t> rgb, bool flip)
{
    auto frame = std::make_shared<Frame>();
    frame->sequence = sequence;
    frame->width = width;
    frame->height = height;
    frame->received_at = std::chrono::steady_clock::now();
    if (flip) {
        frame->rgb.resize(rgb.size());
        const auto row = static_cast<std::size_t>(width) * 3U;
        for (int y = 0; y < height; ++y)
            std::memcpy(frame->rgb.data() + static_cast<std::size_t>(y) * row, rgb.data() + static_cast<std::size_t>(height - 1 - y) * row,
                        row);
    }
    else {
        frame->rgb.assign(rgb.begin(), rgb.end());
    }
    latest_.store(std::move(frame), std::memory_order_release);
}

bool Receiver::on_packet(const std::uint8_t* data, std::size_t size)
{
    if (!data)
        return false;
    // Chunks are at most 1221 bytes; a legacy datagram is a whole frame.
    if (size <= HeaderBytes + ChunkBytes) {
        const auto header = parse_chunk(data, size, HeaderVersion);
        if (!header)
            return false;
        // Sessions grow across desktop runs: chunks of an older run are late.
        if (header->session < session_) {
            Diagnostics::RuntimeDiagnostics::instance().record_frame_stream_event(assembler_.name(),
                                                                                  Diagnostics::FrameStreamEvent::LateChunk);
            return true;
        }
        session_ = header->session;
        if (const auto done = assembler_.add(*header, data + HeaderBytes)) {
            publish(done->sequence, done->width, done->height, done->rgb, false);
            assembler_.published(*done);
        }
        return true;
    }

    const int width = Constants::width;
    const int height = Constants::height;
    const auto frame_bytes = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3U;
    if (width <= 0 || height <= 0 || size < frame_bytes)
        return false;
    Diagnostics::RuntimeDiagnostics::instance().record_frame_stream_event(assembler_.name(), Diagnostics::FrameStreamEvent::LegacyFrame);
    publish(++legacy_sequence_, width, height, std::span<const std::uint8_t>(data, frame_bytes), legacy_bottom_up_);
    return true;
}

std::shared_ptr<const Frame> Receiver::latest() const
{
    return latest_.load(std::memory_order_acquire);
}

std::optional<double> Receiver::age_ms() const
{
    const auto frame = latest();
    if (!frame)
        return std::nullopt;
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - frame->received_at).count();
}

void Receiver::clear()
{
    latest_.store({}, std::memory_order_release);
}

bool Receiver::copy_latest(rgb_matrix::FrameCanvas* canvas, int width, int height) const
{
    const auto frame = latest();
    if (!canvas || !frame || frame->rgb.empty())
        return false;
    const int w = std::min(width, frame->width);
    const int h = std::min(height, frame->height);
    if (w <= 0 || h <= 0)
        return false;
    if (w == frame->width) {
        canvas->SetRGB(0, 0, w, h, frame->rgb.data());
    }
    else {
        for (int y = 0; y < h; ++y) canvas->SetRGB(0, y, w, 1, frame->rgb.data() + static_cast<std::size_t>(y) * frame->width * 3U);
    }
    return true;
}

}  // namespace FrameStream
