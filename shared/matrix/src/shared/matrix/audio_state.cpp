#include <shared/matrix/audio_state.h>
#include <shared/matrix/input_ids.h>
#include <shared/matrix/music_style.h>
#include <shared/matrix/runtime_inputs.h>

#include <algorithm>
#include <chrono>
#include <mutex>

namespace {
std::mutex stateMutex;
AudioProtocol::Frame currentFrame;
std::chrono::steady_clock::time_point receivedAt{};
std::chrono::steady_clock::time_point runtimeInputPublishedAt{};
bool hasFrame = false;
MusicStyle::AudioEstimator styleEstimator;

constexpr auto RuntimeInputPublishInterval = std::chrono::milliseconds(200);
constexpr auto RuntimeInputTtl = std::chrono::milliseconds(750);
}  // namespace

namespace AudioState {
void update(const AudioProtocol::Frame& frame) { update(frame, std::chrono::steady_clock::now()); }

void update(const AudioProtocol::Frame& frame, std::chrono::steady_clock::time_point now)
{
    bool publish_runtime_input = false;
    MusicStyle::Weights style{};
    float style_confidence = 0.0f;
    {
        std::lock_guard lock(stateMutex);
        const float dt = hasFrame ? std::chrono::duration<float>(now - receivedAt).count() : 0.0f;
        styleEstimator.update(frame, dt);
        currentFrame = frame;
        receivedAt = now;
        hasFrame = true;
        if (runtimeInputPublishedAt == std::chrono::steady_clock::time_point{} ||
            now - runtimeInputPublishedAt >= RuntimeInputPublishInterval) {
            runtimeInputPublishedAt = now;
            publish_runtime_input = true;
            style = styleEstimator.weights();
            style_confidence = styleEstimator.confidence();
        }
    }

    // Runtime Inputs are intentionally coarse context for directors, eligibility,
    // and diagnostics. High-frequency visual consumers must read AudioState
    // directly instead of paying for string-keyed generic signal publication at
    // the desktop stream's 60 Hz packet rate.
    if (publish_runtime_input) {
        RuntimeInputs::Signals signals{{"loudness", static_cast<double>(frame.feature(AudioProtocol::Feature::Loudness))},
                                {"loudness_fast", static_cast<double>(frame.feature(AudioProtocol::Feature::LoudnessFast))},
                                {"loudness_slow", static_cast<double>(frame.feature(AudioProtocol::Feature::LoudnessSlow))},
                                {"sub_bass", static_cast<double>(frame.feature(AudioProtocol::Feature::SubBass))},
                                {"bass", static_cast<double>(frame.feature(AudioProtocol::Feature::Bass))},
                                {"mid", static_cast<double>(frame.feature(AudioProtocol::Feature::Mid))},
                                {"treble", static_cast<double>(frame.feature(AudioProtocol::Feature::Treble))},
                                {"spectral_centroid", static_cast<double>(frame.feature(AudioProtocol::Feature::SpectralCentroid))},
                                {"onset_strength", static_cast<double>(frame.feature(AudioProtocol::Feature::OnsetStrength))},
                                {"kick", static_cast<double>(frame.feature(AudioProtocol::Feature::Kick))},
                                {"snare", static_cast<double>(frame.feature(AudioProtocol::Feature::Snare))},
                                {"hihat", static_cast<double>(frame.feature(AudioProtocol::Feature::Hihat))},
                                {"energy_trend", static_cast<double>(frame.feature(AudioProtocol::Feature::EnergyTrend))},
                                {"drop", static_cast<double>(frame.feature(AudioProtocol::Feature::Drop))},
                                {"section_change", static_cast<double>(frame.feature(AudioProtocol::Feature::SectionChange))},
                                {"bpm", static_cast<double>(frame.feature(AudioProtocol::Feature::Bpm))},
                                {"beat_confidence", static_cast<double>(frame.feature(AudioProtocol::Feature::BeatConfidence))},
                                {"beat_strength", static_cast<double>(frame.feature(AudioProtocol::Feature::BeatStrength))},
                                {"tempo_stability", static_cast<double>(frame.feature(AudioProtocol::Feature::TempoStability))},
                                {"silence", frame.event(AudioProtocol::Silent)},
                                {"beat_counter", static_cast<std::int64_t>(frame.beat_counter)},
                                {"drop_counter", static_cast<std::int64_t>(frame.drop_counter)},
                                {"section_counter", static_cast<std::int64_t>(frame.section_counter)}};
        signals.emplace(std::string(MusicStyle::AudioConfidenceField), static_cast<double>(style_confidence));
        for (int i = 0; i < MusicStyle::StyleCount; ++i)
            signals.emplace(MusicStyle::audio_field(static_cast<MusicStyle::Style>(i)), static_cast<double>(style[static_cast<size_t>(i)]));
        RuntimeInputs::publish(RuntimeInputIds::Audio, std::move(signals), RuntimeInputTtl);
    }
}

void clear()
{
    {
        std::lock_guard lock(stateMutex);
        currentFrame = {};
        receivedAt = {};
        runtimeInputPublishedAt = {};
        hasFrame = false;
        styleEstimator.reset();
    }
    RuntimeInputs::set_available(RuntimeInputIds::Audio, false);
}

Snapshot snapshot()
{
    std::lock_guard lock(stateMutex);
    Snapshot result;
    static_cast<AudioProtocol::Frame&>(result) = currentFrame;
    result.available = hasFrame;
    if (hasFrame)
        result.age_seconds = std::chrono::duration<float>(std::chrono::steady_clock::now() - receivedAt).count();
    return result;
}

float average_spectrum(const Snapshot& state, float start_fraction, float end_fraction)
{
    if (state.spectrum.empty())
        return 0.0f;
    start_fraction = std::clamp(start_fraction, 0.0f, 1.0f);
    end_fraction = std::clamp(end_fraction, start_fraction, 1.0f);
    const size_t begin =
        std::min(state.spectrum.size() - 1, static_cast<size_t>(start_fraction * static_cast<float>(state.spectrum.size())));
    const size_t end =
        std::max(begin + 1, std::min(state.spectrum.size(), static_cast<size_t>(end_fraction * static_cast<float>(state.spectrum.size()))));
    float sum = 0.0f;
    for (size_t i = begin; i < end; ++i) sum += state.spectrum[i];
    return sum / static_cast<float>(end - begin);
}
}  // namespace AudioState
