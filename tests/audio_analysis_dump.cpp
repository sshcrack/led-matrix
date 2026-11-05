// Offline music-analysis dumper.
//
// Runs the exact desktop AudioProcessor + MusicAnalyzer pipeline over a PCM16
// WAV file and writes one encoded AudioProtocol frame per video frame. The
// AudioVisualizer preview provider can replay that file
// (--preview-option audio:replay="path") so preview_gen renders scenes against
// real music instead of the synthetic fixture.
//
//   audio_analysis_dump <in.wav> <out.audr> [fps=30] [start_s=0] [duration_s=30] [preroll_s=8]
//
// File format: "AUDR" | u32 fps | u32 frame_count | { u32 len | bytes }*
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "../plugins/AudioVisualizer/desktop/AudioProcessor.h"
#include "../plugins/AudioVisualizer/desktop/MusicAnalyzer.h"

namespace {
struct Track {
    std::vector<float> left, right;
    double sampleRate = 44100.0;
};

bool loadWav(const std::string& path, Track& out)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return false;
    std::vector<uint8_t> b((std::istreambuf_iterator<char>(in)), {});
    if (b.size() < 44 || std::memcmp(b.data(), "RIFF", 4) != 0)
        return false;
    auto u16 = [&](size_t p) { return static_cast<uint16_t>(b[p] | (b[p + 1] << 8)); };
    auto u32 = [&](size_t p) { return static_cast<uint32_t>(b[p] | (b[p + 1] << 8) | (b[p + 2] << 16) | (b[p + 3] << 24)); };
    uint16_t channels = 0, bits = 0;
    size_t off = 0, size = 0;
    for (size_t p = 12; p + 8 <= b.size();) {
        const uint32_t sz = u32(p + 4);
        if (std::memcmp(b.data() + p, "fmt ", 4) == 0) {
            channels = u16(p + 10);
            out.sampleRate = u32(p + 12);
            bits = u16(p + 22);
        }
        else if (std::memcmp(b.data() + p, "data", 4) == 0) {
            off = p + 8;
            size = std::min<size_t>(sz, b.size() - off);
            break;
        }
        p += 8 + sz + (sz & 1U);
    }
    if (bits != 16 || channels == 0 || size == 0)
        return false;
    const size_t frames = size / (channels * 2);
    out.left.resize(frames);
    out.right.resize(frames);
    for (size_t i = 0; i < frames; ++i) {
        const size_t p = off + i * channels * 2;
        out.left[i] = static_cast<int16_t>(u16(p)) / 32768.0f;
        out.right[i] = channels > 1 ? static_cast<int16_t>(u16(p + 2)) / 32768.0f : out.left[i];
    }
    return true;
}
}  // namespace

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::cerr << "usage: audio_analysis_dump in.wav out.audr [fps] [start_s] [duration_s] [preroll_s]\n";
        return 2;
    }
    const int fps = argc > 3 ? std::stoi(argv[3]) : 30;
    const double start = argc > 4 ? std::stod(argv[4]) : 0.0;
    const double duration = argc > 5 ? std::stod(argv[5]) : 30.0;
    const double preroll = argc > 6 ? std::stod(argv[6]) : 8.0;

    Track track;
    if (!loadWav(argv[1], track)) {
        std::cerr << "Expected PCM16 WAV: " << argv[1] << '\n';
        return 1;
    }

    AudioVisualizerConfig config;
    AudioProcessor processor(config);
    MusicAnalyzer analyzer(config);

    std::ofstream out(argv[2], std::ios::binary);
    const uint32_t total = static_cast<uint32_t>(duration * fps);
    const uint32_t ufps = static_cast<uint32_t>(fps);
    out.write("AUDR", 4);
    out.write(reinterpret_cast<const char*>(&ufps), 4);
    out.write(reinterpret_cast<const char*>(&total), 4);

    // The desktop app analyses on its ~60 Hz render loop; emulate a 2x
    // oversampled update cadence and emit every second analysis.
    const int analysisRate = fps * 2;
    const double analysisStart = std::max(0.0, start - preroll);
    const auto window = MUSIC_ANALYSIS_WINDOW_SIZE;
    uint32_t written = 0;
    for (int64_t k = 0;; ++k) {
        const double t = analysisStart + static_cast<double>(k) / analysisRate;
        const auto end = static_cast<size_t>(t * track.sampleRate);
        if (end >= track.left.size() || written >= total)
            break;
        if (end < window)
            continue;
        AudioRecorder::CapturedAudioFrame frame;
        frame.sampleRate = track.sampleRate;
        frame.sequence = end;
        frame.left.assign(track.left.begin() + static_cast<std::ptrdiff_t>(end - window), track.left.begin() + static_cast<std::ptrdiff_t>(end));
        frame.right.assign(track.right.begin() + static_cast<std::ptrdiff_t>(end - window), track.right.begin() + static_cast<std::ptrdiff_t>(end));
        frame.mono.resize(window);
        for (size_t i = 0; i < window; ++i) frame.mono[i] = 0.5f * (frame.left[i] + frame.right[i]);

        auto bands = processor.computeBands(frame.mono, frame.sampleRate);
        auto analysis = analyzer.analyze(frame, bands);
        if (t + 1e-9 < start || (k % 2) != 0)
            continue;
        analysis.timestamp_ms = static_cast<uint32_t>((t - start) * 1000.0);
        const auto bytes = AudioProtocol::encode(analysis);
        const auto len = static_cast<uint32_t>(bytes.size());
        out.write(reinterpret_cast<const char*>(&len), 4);
        out.write(reinterpret_cast<const char*>(bytes.data()), len);
        ++written;
    }
    out.seekp(8);
    out.write(reinterpret_cast<const char*>(&written), 4);
    std::cout << "wrote " << written << " frames\n";
    return 0;
}
