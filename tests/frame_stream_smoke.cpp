// Desktop frame streams (shader, video, music video): the desktop Sender and
// the matrix Receiver, including older peers on either side.
#include <emulator.h>
#include <led-matrix.h>
#include <shared/common/frame_stream.h>
#include <shared/matrix/canvas_consts.h>
#include <shared/matrix/diagnostics.h>
#include <shared/matrix/frame_stream.h>

#include <iostream>
#include <memory>
#include <vector>

namespace {
int failures = 0;
void expect(bool ok, const char* what)
{
    if (!ok) {
        std::cerr << "FAIL: " << what << '\n';
        ++failures;
    }
}

struct WholeFrame final : UdpPacket {
    std::vector<std::uint8_t> rgb;
    explicit WholeFrame(std::vector<std::uint8_t> data) : UdpPacket(0x02), rgb(std::move(data)) {}
    [[nodiscard]] std::vector<std::uint8_t> toData() const override { return rgb; }
};

std::vector<std::uint8_t> frame_of(std::uint8_t value, int width = 32, int height = 32)
{
    std::vector<std::uint8_t> rgb(static_cast<std::size_t>(width * height * 3), value);
    rgb[0] = static_cast<std::uint8_t>(value + 1);  // top-left pixel differs: catches flips
    return rgb;
}

std::vector<std::vector<std::uint8_t>> payloads(FrameStream::Sender& sender, std::vector<std::uint8_t> rgb, int width = 32, int height = 32)
{
    std::vector<std::vector<std::uint8_t>> result;
    for (const auto& packet :
         sender.packets(std::move(rgb), width, height, [](auto data) { return std::make_unique<WholeFrame>(std::move(data)); }))
        result.push_back(packet->toData());
    return result;
}

void feed(FrameStream::Receiver& receiver, const std::vector<std::vector<std::uint8_t>>& packets)
{
    for (const auto& p : packets) receiver.on_packet(p.data(), p.size());
}
}  // namespace

int main()
{
    Constants::width = Constants::height = 32;
    auto& diagnostics = Diagnostics::RuntimeDiagnostics::instance();

    {  // Until the matrix announces chunks, the desktop sends the legacy whole frame.
        FrameStream::Sender sender(0x02);
        FrameStream::Receiver receiver("smoke-legacy");
        const auto legacy = payloads(sender, frame_of(10));
        expect(!sender.chunked() && legacy.size() == 1 && legacy[0].size() == 32 * 32 * 3, "legacy datagram before the capability");
        feed(receiver, legacy);
        const auto frame = receiver.latest();
        expect(frame && frame->rgb == frame_of(10), "legacy frame received as is");
        expect(diagnostics.frame_stream_json("smoke-legacy")["legacy_frames"] == 1, "legacy frame counted");

        expect(!sender.on_message("size:32x32") && sender.on_message(FrameStream::CapabilityMessage) && sender.chunked(),
               "capability enables chunks");
        const auto chunks = payloads(sender, frame_of(20));
        expect(chunks.size() == 3 && chunks[0].size() <= 1221, "a 32x32 frame is three MTU-safe chunks");
        expect(payloads(sender, frame_of(20), 16, 16).size() == 1, "a size mismatch falls back to legacy");
        sender.reset();
        expect(!sender.chunked(), "a new connection waits for the capability again");
    }

    {  // Chunks: reordering, duplicates, late chunks and older desktop runs.
        FrameStream::Sender sender(0x02);
        sender.on_message(FrameStream::CapabilityMessage);
        FrameStream::Receiver receiver("smoke-chunks");
        const auto a = payloads(sender, frame_of(30));
        const auto b = payloads(sender, frame_of(40));
        receiver.on_packet(a[0].data(), a[0].size());
        receiver.on_packet(b[0].data(), b[0].size());
        receiver.on_packet(b[1].data(), b[1].size());
        receiver.on_packet(a[1].data(), a[1].size());
        receiver.on_packet(a[1].data(), a[1].size());  // duplicate
        receiver.on_packet(a[2].data(), a[2].size());
        expect(receiver.latest() && receiver.latest()->rgb == frame_of(30), "reordered older frame still completes");
        receiver.on_packet(b[2].data(), b[2].size());
        expect(receiver.latest()->rgb == frame_of(40) && receiver.latest()->sequence == 2, "newer frame wins");
        receiver.on_packet(a[0].data(), a[0].size());  // late
        expect(receiver.latest()->sequence == 2, "a late chunk changes nothing");
        const auto counts = diagnostics.frame_stream_json("smoke-chunks");
        expect(counts["completed"] == 2 && counts["reorder_recovered"] == 1 && counts["duplicate_chunks"] == 1 &&
                   counts["late_chunks"] == 1 && counts["incomplete"] == 0,
               "chunk outcomes counted");

        FrameStream::Receiver lossy("smoke-loss");
        const auto c = payloads(sender, frame_of(50));
        const auto d = payloads(sender, frame_of(60));
        lossy.on_packet(c[0].data(), c[0].size());  // c loses two chunks
        feed(lossy, d);
        expect(lossy.latest()->rgb == frame_of(60) && diagnostics.frame_stream_json("smoke-loss")["incomplete"] == 1,
               "a frame with lost chunks is dropped and counted once the next completes");
        (void)payloads(sender, frame_of(70));  // never arrives
        feed(lossy, payloads(sender, frame_of(80)));
        expect(diagnostics.frame_stream_json("smoke-loss")["missed_frames"] == 1, "a frame that never arrived is counted as missed");

        const auto age = receiver.age_ms();
        expect(age && *age >= 0.0 && *age < 1000.0, "frame age");
        receiver.clear();
        expect(!receiver.latest() && !receiver.age_ms(), "clear drops the frame");
    }

    {  // Older desktops send bottom-up shader readbacks; the receiver flips them once.
        FrameStream::Receiver receiver("smoke-flip", true);
        std::vector<std::uint8_t> bottom_up(32 * 32 * 3, 0);
        bottom_up[31 * 32 * 3] = 200;  // bottom-up row 31 = top row
        receiver.on_packet(bottom_up.data(), bottom_up.size());
        const auto frame = receiver.latest();
        expect(frame && frame->rgb[0] == 200, "legacy bottom-up frame is stored top-down");

        rgb_matrix::RGBMatrix::Options led;
        led.rows = led.cols = 32;
        rgb_matrix::EmulatorOptions emulator;
        emulator.headless = true;
        std::unique_ptr<rgb_matrix::EmulatorMatrix> matrix(rgb_matrix::EmulatorMatrix::Create(led, emulator));
        if (!matrix) {
            std::cerr << "failed to create emulator matrix\n";
            return 1;
        }
        auto* canvas = matrix->CreateFrameCanvas();
        std::uint8_t r = 0, g = 0, b = 0;
        expect(receiver.copy_latest(canvas, 32, 32), "copy to canvas");
        canvas->GetPixel(0, 0, &r, &g, &b);
        expect(r == 200 && g == 0 && b == 0, "canvas shows the top row on top");
        FrameStream::Receiver empty("smoke-empty");
        expect(!empty.copy_latest(canvas, 32, 32), "no frame, no copy");
    }

    {  // A restarted desktop has a newer session; chunks of the old run are late.
        FrameStream::Receiver receiver("smoke-session");
        std::vector<std::uint8_t> chunk(21 + 1200, 0);
        const auto header = [&](std::uint32_t session, std::uint32_t sequence) {
            chunk[0] = FrameStream::HeaderVersion;
            for (int i = 0; i < 4; ++i) {
                chunk[1 + i] = static_cast<std::uint8_t>(session >> (24 - 8 * i));
                chunk[5 + i] = static_cast<std::uint8_t>(sequence >> (24 - 8 * i));
            }
            chunk[9] = 0;
            chunk[10] = 20;  // width 20
            chunk[11] = 0;
            chunk[12] = 20;             // height 20 -> 1200 bytes, one chunk
            chunk[13] = chunk[14] = 0;  // index 0
            chunk[15] = 0;
            chunk[16] = 1;  // count 1
            chunk[17] = chunk[18] = chunk[19] = chunk[20] = 0;
        };
        header(100, 5);
        receiver.on_packet(chunk.data(), chunk.size());
        header(200, 1);
        receiver.on_packet(chunk.data(), chunk.size());
        expect(receiver.latest() && receiver.latest()->sequence == 1 && receiver.latest()->width == 20,
               "a newer desktop session restarts the sequence");
        header(100, 6);
        expect(receiver.on_packet(chunk.data(), chunk.size()) && receiver.latest()->sequence == 1,
               "chunks of an older session are ignored");
        chunk[0] = 99;
        expect(!receiver.on_packet(chunk.data(), chunk.size()), "a malformed chunk is rejected");
    }

    if (failures == 0)
        std::cout << "frame_stream_smoke: ok\n";
    return failures == 0 ? 0 : 1;
}
