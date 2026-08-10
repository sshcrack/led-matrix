#include "AudioPreviewProvider.h"

#include <algorithm>
#include <cmath>

#include "shared/common/audio_protocol.h"
#include "shared/matrix/audio_state.h"

namespace {

AudioProtocol::Frame make_preview_audio(const Scenes::SceneFrameContext &frame,
                                        const int fps,
                                        const float requested_bpm,
                                        const std::string &profile)
{
    AudioProtocol::Frame audio;
    const float t = static_cast<float>(frame.elapsed_seconds);
    const float bpm = std::clamp(requested_bpm, 40.0f, 240.0f);
    const float beat_period = 60.0f / bpm;
    const float beat_position = t / beat_period;
    const auto beat_index = static_cast<std::uint64_t>(std::floor(beat_position));
    const float beat_phase = beat_position - std::floor(beat_position);
    float kick = std::exp(-beat_phase * 9.0f);
    float groove = 0.5f + 0.5f * std::sin(t * 1.7f);
    float shimmer = 0.5f + 0.5f * std::sin(t * 4.1f + 0.7f);
    float low_gain = 1.0f;
    float mid_gain = 1.0f;
    float high_gain = 1.0f;
    float transient_gain = 1.0f;

    if (profile == "bass") {
        low_gain = 1.28f;
        mid_gain = 0.82f;
        high_gain = 0.58f;
        transient_gain = 0.9f;
    } else if (profile == "percussion") {
        low_gain = 1.08f;
        mid_gain = 0.92f;
        high_gain = 1.18f;
        transient_gain = 1.30f;
    } else if (profile == "ambient") {
        kick *= 0.35f;
        groove = 0.55f + 0.28f * std::sin(t * 0.75f);
        shimmer = 0.55f + 0.32f * std::sin(t * 1.25f + 0.7f);
        low_gain = 0.78f;
        mid_gain = 1.05f;
        high_gain = 0.82f;
        transient_gain = 0.45f;
    }

    const auto frame_index = frame.frame_index;
    audio.sequence = static_cast<std::uint32_t>(frame_index + 1);
    audio.timestamp_ms = static_cast<std::uint32_t>(std::max(0.0, frame.elapsed_seconds) * 1000.0);
    audio.beat_counter = beat_index + 1;
    audio.onset_counter = static_cast<std::uint64_t>(std::floor(t * 4.0f)) + 1;
    audio.drop_counter = beat_index / 8;
    audio.section_counter = static_cast<std::uint64_t>(t / 4.0f);

    const float frame_phase = 1.0f / static_cast<float>(std::max(1, fps));
    if (frame_index == 0 || beat_phase < frame_phase)
        audio.flags |= AudioProtocol::BeatEvent | AudioProtocol::KickEvent;
    if (frame_index % static_cast<std::uint64_t>(std::max(1, fps / 4)) == 0)
        audio.flags |= AudioProtocol::OnsetEvent | AudioProtocol::HihatEvent;
    if (beat_index > 0 && beat_index % 8 == 0 && beat_phase < frame_phase)
        audio.flags |= AudioProtocol::DropEvent;

    audio.set(AudioProtocol::Feature::Rms, 0.35f + groove * 0.35f);
    audio.set(AudioProtocol::Feature::Peak, 0.55f + kick * 0.40f);
    audio.set(AudioProtocol::Feature::Loudness, 0.35f + groove * 0.40f);
    audio.set(AudioProtocol::Feature::LoudnessFast, 0.35f + groove * 0.42f);
    audio.set(AudioProtocol::Feature::LoudnessSlow, 0.42f + groove * 0.25f);
    audio.set(AudioProtocol::Feature::SubBass, std::clamp((0.25f + kick * 0.70f) * low_gain, 0.0f, 1.0f));
    audio.set(AudioProtocol::Feature::Bass, std::clamp((0.32f + kick * 0.62f) * low_gain, 0.0f, 1.0f));
    audio.set(AudioProtocol::Feature::LowMid, std::clamp((0.30f + groove * 0.45f) * mid_gain, 0.0f, 1.0f));
    audio.set(AudioProtocol::Feature::Mid, std::clamp((0.28f + groove * 0.40f) * mid_gain, 0.0f, 1.0f));
    audio.set(AudioProtocol::Feature::HighMid, std::clamp((0.22f + shimmer * 0.45f) * high_gain, 0.0f, 1.0f));
    audio.set(AudioProtocol::Feature::Treble, std::clamp((0.20f + shimmer * 0.60f) * high_gain, 0.0f, 1.0f));
    audio.set(AudioProtocol::Feature::Air, std::clamp((0.18f + shimmer * 0.50f) * high_gain, 0.0f, 1.0f));
    audio.set(AudioProtocol::Feature::SpectralCentroid, 0.42f + shimmer * 0.28f);
    audio.set(AudioProtocol::Feature::SpectralFlux, std::clamp((0.18f + kick * 0.65f) * transient_gain, 0.0f, 1.0f));
    audio.set(AudioProtocol::Feature::OnsetStrength, std::clamp((0.18f + kick * 0.72f) * transient_gain, 0.0f, 1.0f));
    audio.set(AudioProtocol::Feature::Kick, std::clamp(kick * transient_gain, 0.0f, 1.0f));
    audio.set(AudioProtocol::Feature::Snare, 0.25f + (1.0f - kick) * groove * 0.45f);
    audio.set(AudioProtocol::Feature::Hihat, std::clamp((0.25f + shimmer * 0.65f) * high_gain * transient_gain, 0.0f, 1.0f));
    audio.set(AudioProtocol::Feature::StereoWidth, 0.58f);
    audio.set(AudioProtocol::Feature::StereoBalance, std::sin(t * 0.73f) * 0.55f);
    audio.set(AudioProtocol::Feature::StereoCorrelation, 0.45f);
    audio.set(AudioProtocol::Feature::EnergyTrend, std::sin(t * 0.55f) * 0.22f);
    audio.set(AudioProtocol::Feature::Drop, (audio.flags & AudioProtocol::DropEvent) ? 1.0f : 0.0f);
    audio.set(AudioProtocol::Feature::Bpm, bpm);
    audio.set(AudioProtocol::Feature::BeatPhase, beat_phase);
    audio.set(AudioProtocol::Feature::BeatConfidence, 0.92f);
    audio.set(AudioProtocol::Feature::BeatStrength, 0.55f + kick * 0.45f);
    audio.set(AudioProtocol::Feature::TempoStability, 0.95f);

    constexpr int spectrum_bins = 96;
    audio.spectrum.resize(spectrum_bins);
    for (int i = 0; i < spectrum_bins; ++i) {
        const float x = static_cast<float>(i) / static_cast<float>(spectrum_bins - 1);
        const float bass = std::exp(-std::pow((x - 0.08f) / 0.10f, 2.0f)) * (0.25f + kick * 0.75f) * low_gain;
        const float mids = std::exp(-std::pow((x - 0.38f) / 0.22f, 2.0f)) * (0.20f + groove * 0.55f) * mid_gain;
        const float highs = std::exp(-std::pow((x - 0.76f) / 0.18f, 2.0f)) * (0.12f + shimmer * 0.48f) * high_gain;
        audio.spectrum[static_cast<std::size_t>(i)] = std::clamp(0.025f + bass + mids + highs, 0.0f, 1.0f);
    }

    constexpr int waveform_points = 128;
    audio.waveform.resize(waveform_points);
    for (int i = 0; i < waveform_points; ++i) {
        const float phase = static_cast<float>(i) / waveform_points;
        audio.waveform[static_cast<std::size_t>(i)] =
            std::sin((phase * 5.0f + t * 1.8f) * 6.2831853f) * (0.28f + groove * 0.22f) +
            std::sin((phase * 11.0f + t * 3.1f) * 6.2831853f) * 0.12f;
    }
    return audio;
}

} // namespace

void AudioPreviewProvider::begin(const Previews::RunContext &context)
{
    fps_ = std::max(1, context.fps);
    const auto options = context.options_for(id());
    bpm_ = std::clamp(options.value("bpm", 120.0f), 40.0f, 240.0f);
    profile_ = options.value("profile", std::string("balanced"));
    if (profile_ != "balanced" && profile_ != "bass" &&
        profile_ != "percussion" && profile_ != "ambient")
        profile_ = "balanced";
}

void AudioPreviewProvider::update(const Scenes::SceneFrameContext &frame)
{
    AudioState::update(make_preview_audio(frame, fps_, bpm_, profile_));
}

void AudioPreviewProvider::end() noexcept
{
    AudioState::clear();
}
