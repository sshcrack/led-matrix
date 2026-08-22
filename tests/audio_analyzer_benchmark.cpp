#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <numeric>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <vector>

#include "../plugins/AudioVisualizer/desktop/MusicAnalyzer.h"

namespace {
constexpr double Pi = 3.14159265358979323846;

struct Track {
    std::vector<float> mono;
    double sampleRate = 44100.0;
};

struct Result {
    float bpm = 0.0f;
    float confidence = 0.0f;
    float stability = 0.0f;
    float onsetRate = 0.0f;
    float beatRate = 0.0f;
    float dropRate = 0.0f;
    float meanKick = 0.0f;
    float meanSnare = 0.0f;
    float meanHihat = 0.0f;
};

float median(std::vector<float> values)
{
    if (values.empty())
        return 0.0f;
    const size_t middle = values.size() / 2;
    std::nth_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(middle), values.end());
    return values[middle];
}

Track syntheticDrums(float bpm, float seconds, double sampleRate = 44100.0)
{
    const size_t count = static_cast<size_t>(seconds * sampleRate);
    Track track{std::vector<float>(count, 0.0f), sampleRate};
    const double beatPeriod = 60.0 / bpm;
    std::mt19937 rng(0x51A17U + static_cast<unsigned>(bpm * 10.0f));
    std::uniform_real_distribution<float> noise(-1.0f, 1.0f);

    // Low-level musical bed gives the adaptive loudness/band normalizers a
    // realistic floor without creating transients of its own.
    for (size_t i = 0; i < count; ++i) {
        const double t = static_cast<double>(i) / sampleRate;
        track.mono[i] = 0.018f * std::sin(2.0 * Pi * 196.0 * t) + 0.012f * std::sin(2.0 * Pi * 293.66 * t);
    }

    const int beats = static_cast<int>(seconds / beatPeriod) + 1;
    for (int beat = 0; beat < beats; ++beat) {
        const double beatTime = 0.35 + beat * beatPeriod;
        const size_t start = static_cast<size_t>(beatTime * sampleRate);
        if (start >= count)
            break;

        // Kick on every quarter note: a decaying sine sweep plus a tiny click.
        const size_t kickLength = static_cast<size_t>(0.13 * sampleRate);
        for (size_t j = 0; j < kickLength && start + j < count; ++j) {
            const double t = static_cast<double>(j) / sampleRate;
            const double phase = 2.0 * Pi * (62.0 * t + 34.0 * t * t);
            const float env = static_cast<float>(std::exp(-24.0 * t));
            track.mono[start + j] += 0.72f * env * static_cast<float>(std::sin(phase));
        }
        for (size_t j = 0; j < 48 && start + j < count; ++j) track.mono[start + j] += 0.22f * (1.0f - static_cast<float>(j) / 48.0f);

        // Snare on 2/4. Keeping this on the quarter grid exercises extra
        // spectral content without changing the target tempo.
        if ((beat & 3) == 1 || (beat & 3) == 3) {
            const size_t snareLength = static_cast<size_t>(0.085 * sampleRate);
            for (size_t j = 0; j < snareLength && start + j < count; ++j) {
                const double t = static_cast<double>(j) / sampleRate;
                const float env = static_cast<float>(std::exp(-33.0 * t));
                track.mono[start + j] += 0.28f * env * noise(rng);
            }
        }
    }

    for (float& sample : track.mono) sample = std::clamp(sample, -1.0f, 1.0f);
    return track;
}

std::optional<Track> loadPcm16Wav(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return std::nullopt;
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), {});
    if (bytes.size() < 44 || std::memcmp(bytes.data(), "RIFF", 4) != 0 || std::memcmp(bytes.data() + 8, "WAVE", 4) != 0)
        return std::nullopt;

    auto u16 = [&](size_t p) -> uint16_t { return static_cast<uint16_t>(bytes[p]) | static_cast<uint16_t>(bytes[p + 1] << 8U); };
    auto u32 = [&](size_t p) -> uint32_t {
        return static_cast<uint32_t>(bytes[p]) | (static_cast<uint32_t>(bytes[p + 1]) << 8U) |
               (static_cast<uint32_t>(bytes[p + 2]) << 16U) | (static_cast<uint32_t>(bytes[p + 3]) << 24U);
    };

    uint16_t format = 0, channels = 0, bits = 0;
    uint32_t sampleRate = 0;
    size_t dataOffset = 0, dataSize = 0;
    for (size_t p = 12; p + 8 <= bytes.size();) {
        const uint32_t size = u32(p + 4);
        const size_t payload = p + 8;
        if (payload + size > bytes.size())
            break;
        if (std::memcmp(bytes.data() + p, "fmt ", 4) == 0 && size >= 16) {
            format = u16(payload);
            channels = u16(payload + 2);
            sampleRate = u32(payload + 4);
            bits = u16(payload + 14);
        }
        else if (std::memcmp(bytes.data() + p, "data", 4) == 0) {
            dataOffset = payload;
            dataSize = size;
        }
        p = payload + size + (size & 1U);
    }
    if (format != 1 || bits != 16 || channels == 0 || sampleRate == 0 || dataSize == 0)
        return std::nullopt;

    const size_t frameBytes = static_cast<size_t>(channels) * 2;
    const size_t frames = dataSize / frameBytes;
    Track result{std::vector<float>(frames), static_cast<double>(sampleRate)};
    for (size_t i = 0; i < frames; ++i) {
        float sum = 0.0f;
        for (uint16_t channel = 0; channel < channels; ++channel) {
            const size_t p = dataOffset + i * frameBytes + static_cast<size_t>(channel) * 2;
            const int16_t sample = static_cast<int16_t>(u16(p));
            sum += static_cast<float>(sample) / 32768.0f;
        }
        result.mono[i] = sum / static_cast<float>(channels);
    }
    return result;
}

Result analyzeTrack(const Track& track, float collectAfterSeconds = 8.0f)
{
    AudioVisualizerConfig config;
    config.musicAnalysisGain = 1.0;
    config.transientSensitivity = 1.0;
    config.beatSensitivity = 1.0;
    MusicAnalyzer analyzer(config);

    std::vector<float> bpms, confidences, stabilities, kicks, snares, hihats;
    uint64_t firstOnset = 0, lastOnset = 0, firstBeat = 0, lastBeat = 0, firstDrop = 0, lastDrop = 0;
    double firstCollected = 0.0, lastCollected = 0.0;
    std::vector<float> displayBands(64, 0.0f);

    for (size_t end = MUSIC_ANALYSIS_WINDOW_SIZE; end <= track.mono.size(); end += FFT_HOP_SIZE) {
        AudioRecorder::CapturedAudioFrame frame;
        frame.sampleRate = track.sampleRate;
        frame.sequence = end;
        frame.mono.assign(track.mono.begin() + static_cast<std::ptrdiff_t>(end - MUSIC_ANALYSIS_WINDOW_SIZE),
                          track.mono.begin() + static_cast<std::ptrdiff_t>(end));
        frame.left = frame.mono;
        frame.right = frame.mono;
        const auto result = analyzer.analyze(frame, displayBands);
        const double seconds = static_cast<double>(end) / track.sampleRate;
        if (std::getenv("AUDIO_BENCH_TRACE") && result.event(AudioProtocol::OnsetEvent)) {
            std::cout << "TRACE t=" << seconds << " onset=" << result.feature(AudioProtocol::Feature::OnsetStrength)
                      << " kick=" << result.feature(AudioProtocol::Feature::Kick)
                      << " snare=" << result.feature(AudioProtocol::Feature::Snare)
                      << " hihat=" << result.feature(AudioProtocol::Feature::Hihat)
                      << " bpm=" << result.feature(AudioProtocol::Feature::Bpm)
                      << " conf=" << result.feature(AudioProtocol::Feature::BeatConfidence)
                      << " beat=" << result.event(AudioProtocol::BeatEvent) << " beat_counter=" << result.beat_counter << '\n';
        }
        if (seconds < collectAfterSeconds)
            continue;
        if (bpms.empty()) {
            firstOnset = result.onset_counter;
            firstBeat = result.beat_counter;
            firstDrop = result.drop_counter;
            firstCollected = seconds;
        }
        lastOnset = result.onset_counter;
        lastBeat = result.beat_counter;
        lastDrop = result.drop_counter;
        lastCollected = seconds;
        if (result.feature(AudioProtocol::Feature::Bpm) > 1.0f)
            bpms.push_back(result.feature(AudioProtocol::Feature::Bpm));
        confidences.push_back(result.feature(AudioProtocol::Feature::BeatConfidence));
        stabilities.push_back(result.feature(AudioProtocol::Feature::TempoStability));
        kicks.push_back(result.feature(AudioProtocol::Feature::Kick));
        snares.push_back(result.feature(AudioProtocol::Feature::Snare));
        hihats.push_back(result.feature(AudioProtocol::Feature::Hihat));
    }

    const float duration = static_cast<float>(std::max(0.001, lastCollected - firstCollected));
    auto mean = [](const std::vector<float>& values) {
        return values.empty() ? 0.0f : std::accumulate(values.begin(), values.end(), 0.0f) / static_cast<float>(values.size());
    };
    return {median(bpms),
            median(confidences),
            median(stabilities),
            static_cast<float>(lastOnset - firstOnset) / duration,
            static_cast<float>(lastBeat - firstBeat) / duration,
            static_cast<float>(lastDrop - firstDrop) / duration,
            mean(kicks),
            mean(snares),
            mean(hihats)};
}

void printResult(const std::string& name, const Result& r)
{
    std::cout << name << ": bpm=" << r.bpm << " confidence=" << r.confidence << " stability=" << r.stability << " onsets/s=" << r.onsetRate
              << " beats/s=" << r.beatRate << " drops/s=" << r.dropRate << " kick=" << r.meanKick << " snare=" << r.meanSnare
              << " hihat=" << r.meanHihat << '\n';
}
}  // namespace

int main(int argc, char** argv)
{
    if (argc >= 2) {
        const auto track = loadPcm16Wav(argv[1]);
        if (!track) {
            std::cerr << "Expected a PCM 16-bit WAV file: " << argv[1] << '\n';
            return 2;
        }
        const Result result = analyzeTrack(*track, std::min(12.0f, static_cast<float>(track->mono.size() / track->sampleRate) * 0.35f));
        printResult(argv[1], result);
        if (argc >= 3) {
            const float expected = std::stof(argv[2]);
            const float error = std::abs(result.bpm - expected);
            if (error > 4.0f || result.confidence < 0.40f) {
                std::cerr << "tempo validation failed: expected " << expected << " BPM, error=" << error
                          << ", confidence=" << result.confidence << '\n';
                return 1;
            }
        }
        return 0;
    }

    bool ok = true;
    for (float expected : {72.0f, 90.0f, 120.0f, 150.0f, 168.0f, 170.0f, 174.0f, 176.0f, 180.0f}) {
        const Result result = analyzeTrack(syntheticDrums(expected, 22.0f));
        printResult("synthetic " + std::to_string(static_cast<int>(expected)) + " BPM", result);
        const float error = std::abs(result.bpm - expected);
        const float expectedBeatRate = expected / 60.0f;
        const float beatRateError = std::abs(result.beatRate - expectedBeatRate);
        if (error > 2.5f || result.confidence < 0.55f || result.stability < 0.50f || result.onsetRate < 0.7f || result.dropRate > 0.03f ||
            beatRateError > std::max(0.16f, expectedBeatRate * 0.10f)) {
            std::cerr << "FAILED " << expected << " BPM: error=" << error << ", confidence=" << result.confidence
                      << ", stability=" << result.stability << ", onsetRate=" << result.onsetRate << ", dropRate=" << result.dropRate
                      << ", beatRateError=" << beatRateError << '\n';
            ok = false;
        }
    }

    Track silence{std::vector<float>(static_cast<size_t>(12.0 * 44100.0), 0.0f), 44100.0};
    const Result silent = analyzeTrack(silence, 5.0f);
    printResult("silence", silent);
    if (silent.confidence > 0.05f || silent.onsetRate > 0.05f || silent.beatRate > 0.05f) {
        std::cerr << "FAILED silence rejection\n";
        ok = false;
    }

    Track stopped = syntheticDrums(120.0f, 16.0f);
    stopped.mono.resize(static_cast<size_t>(26.0 * stopped.sampleRate), 0.0f);
    const Result afterStop = analyzeTrack(stopped, 22.0f);
    printResult("120 BPM then silence", afterStop);
    if (afterStop.confidence > 0.12f || afterStop.stability > 0.18f || afterStop.beatRate > 0.08f) {
        std::cerr << "FAILED stale-tempo decay after audio stopped\n";
        ok = false;
    }

    Track noise{std::vector<float>(static_cast<size_t>(16.0 * 44100.0)), 44100.0};
    std::mt19937 noiseRng(0xA11D10U);
    std::uniform_real_distribution<float> randomSample(-0.08f, 0.08f);
    for (float& sample : noise.mono) sample = randomSample(noiseRng);
    const Result randomNoise = analyzeTrack(noise, 6.0f);
    printResult("aperiodic noise", randomNoise);
    if (randomNoise.confidence > 0.22f || randomNoise.beatRate > 0.40f) {
        std::cerr << "FAILED aperiodic-noise rejection\n";
        ok = false;
    }

    // A transport pause is not a musical breakdown/drop. The first transient
    // after sustained digital silence used to arm and fire DropEvent, causing
    // tunnel/particle visuals to explode exactly when playback resumed.
    Track pauseResume = syntheticDrums(120.0f, 8.0f);
    pauseResume.mono.resize(static_cast<size_t>(11.0 * pauseResume.sampleRate), 0.0f);
    const Track resumed = syntheticDrums(120.0f, 8.0f, pauseResume.sampleRate);
    pauseResume.mono.insert(pauseResume.mono.end(), resumed.mono.begin(), resumed.mono.end());
    const Result afterPauseResume = analyzeTrack(pauseResume, 7.0f);
    printResult("120 BPM pause/resume", afterPauseResume);
    if (afterPauseResume.dropRate > 0.02f || afterPauseResume.confidence < 0.35f || std::abs(afterPauseResume.bpm - 120.0f) > 4.0f) {
        std::cerr << "FAILED pause/resume stability: false drop or tempo did not recover\n";
        ok = false;
    }

    Track transition = syntheticDrums(90.0f, 12.0f);
    const Track faster = syntheticDrums(150.0f, 18.0f, transition.sampleRate);
    transition.mono.insert(transition.mono.end(), faster.mono.begin(), faster.mono.end());
    const Result afterChange = analyzeTrack(transition, 24.0f);
    printResult("90 to 150 BPM transition", afterChange);
    if (std::abs(afterChange.bpm - 150.0f) > 3.0f || afterChange.confidence < 0.45f || afterChange.stability < 0.45f) {
        std::cerr << "FAILED tempo-change recovery\n";
        ok = false;
    }
    return ok ? 0 : 1;
}
