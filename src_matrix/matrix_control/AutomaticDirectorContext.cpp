// AutomaticDirector: runtime-input signals and context derivation.
#include "AutomaticDirectorInternal.h"

#include <algorithm>
#include <cmath>

#include "shared/matrix/input_ids.h"

namespace {
float signal_number(const RuntimeInputs::Snapshot& snapshot, std::string_view input, std::string_view signal, float fallback)
{
    const auto value = snapshot.number(input, signal);
    return value.has_value() ? static_cast<float>(*value) : fallback;
}

bool signal_bool(const RuntimeInputs::Snapshot& snapshot, std::string_view input, std::string_view signal, bool fallback)
{
    const auto value = snapshot.boolean(input, signal);
    return value.has_value() ? *value : fallback;
}

std::uint64_t signal_counter(const RuntimeInputs::Snapshot& snapshot, std::string_view input, std::string_view signal)
{
    const auto value = snapshot.number(input, signal);
    if (!value.has_value() || *value <= 0.0)
        return 0;
    return static_cast<std::uint64_t>(*value);
}

std::string signal_text(const RuntimeInputs::Snapshot& snapshot, std::string_view input, std::string_view signal)
{
    return snapshot.text(input, signal).value_or(std::string{});
}

}  // namespace

namespace AutomaticDirectorDetail {
bool has_tag(const std::vector<std::string>& tags, std::string_view wanted)
{
    return std::find(tags.begin(), tags.end(), wanted) != tags.end();
}

bool has_any_tag(const std::vector<std::string>& tags, std::initializer_list<std::string_view> wanted)
{
    return std::any_of(wanted.begin(), wanted.end(), [&](std::string_view tag) { return has_tag(tags, tag); });
}

std::string presentation_role(const Scenes::EffectiveSceneProfile& profile)
{
    if (has_any_tag(profile.tags, {"spotify-video", "album-art", "media"}))
        return "media";
    if (has_tag(profile.tags, "director"))
        return "director";
    if (has_any_tag(profile.tags, {"audio-reactive", "music", "beat-driven"}))
        return "reactive";
    return "ambient";
}

DirectorContext context_for(const RuntimeInputs::Snapshot& runtime_inputs, const VisualJourney::Frame& journey)
{
    DirectorContext c;
    c.audio_available = runtime_inputs.available(RuntimeInputIds::Audio);
    c.spotify = runtime_inputs.available(RuntimeInputIds::SpotifyPlayback)
        && signal_bool(runtime_inputs, RuntimeInputIds::SpotifyPlayback, "playing", true);
    c.spotify_track_id = signal_text(runtime_inputs, RuntimeInputIds::SpotifyPlayback, "track_id");
    c.spotify_mv_tools_ready = runtime_inputs.available(RuntimeInputIds::SpotifyMVReady)
        && signal_bool(runtime_inputs, RuntimeInputIds::SpotifyMVReady, "tools_ready", true);
    c.spotify_mv_first_frame_ready = signal_bool(
        runtime_inputs, RuntimeInputIds::SpotifyMVReady, "first_frame_ready", false);
    c.spotify_mv_track_id = signal_text(runtime_inputs, RuntimeInputIds::SpotifyMVReady, "track_id");
    c.spotify_mv_state = signal_text(runtime_inputs, RuntimeInputIds::SpotifyMVReady, "state");
    c.spotify_mv_ready = c.spotify && c.spotify_mv_tools_ready && c.spotify_mv_first_frame_ready
        && !c.spotify_track_id.empty() && c.spotify_mv_track_id == c.spotify_track_id;

    const bool silence = c.audio_available
        && signal_bool(runtime_inputs, RuntimeInputIds::Audio, "silence", false);
    c.audio_active = c.audio_available && !silence;

    const float loudness = std::clamp(
        signal_number(runtime_inputs, RuntimeInputIds::Audio, "loudness", 0.35f), 0.0f, 1.0f);
    const float slow = std::clamp(
        signal_number(runtime_inputs, RuntimeInputIds::Audio, "loudness_slow", loudness), 0.0f, 1.0f);
    c.loudness_fast = std::clamp(
        signal_number(runtime_inputs, RuntimeInputIds::Audio, "loudness_fast", loudness), 0.0f, 1.0f);
    // Director decisions happen on scene boundaries. Bias toward the stable
    // envelope so one kick/drop cannot select an over-aggressive scene for the
    // next 20-30 seconds.
    c.loudness = std::clamp(0.52f * slow + 0.34f * loudness + 0.14f * c.loudness_fast, 0.0f, 1.0f);
    c.bass = std::clamp(
        0.58f * signal_number(runtime_inputs, RuntimeInputIds::Audio, "bass", 0.0f)
            + 0.42f * signal_number(runtime_inputs, RuntimeInputIds::Audio, "sub_bass", 0.0f),
        0.0f, 1.0f);
    c.treble = std::clamp(signal_number(runtime_inputs, RuntimeInputIds::Audio, "treble", 0.0f), 0.0f, 1.0f);
    c.brightness = std::clamp(
        0.58f * signal_number(runtime_inputs, RuntimeInputIds::Audio, "spectral_centroid", c.treble)
            + 0.42f * c.treble,
        0.0f, 1.0f);
    c.onset = std::clamp(signal_number(runtime_inputs, RuntimeInputIds::Audio, "onset_strength", 0.0f), 0.0f, 1.0f);
    c.kick = std::clamp(signal_number(runtime_inputs, RuntimeInputIds::Audio, "kick", 0.0f), 0.0f, 1.0f);
    c.snare = std::clamp(signal_number(runtime_inputs, RuntimeInputIds::Audio, "snare", 0.0f), 0.0f, 1.0f);
    c.hihat = std::clamp(signal_number(runtime_inputs, RuntimeInputIds::Audio, "hihat", 0.0f), 0.0f, 1.0f);
    c.beat_strength = std::clamp(signal_number(runtime_inputs, RuntimeInputIds::Audio, "beat_strength", 0.0f), 0.0f, 1.0f);
    c.rhythmicity = std::clamp(
        0.28f * c.onset + 0.24f * c.beat_strength + 0.22f * c.kick + 0.14f * c.snare + 0.12f * c.hihat,
        0.0f, 1.0f);
    c.energy_trend = std::clamp(signal_number(runtime_inputs, RuntimeInputIds::Audio, "energy_trend", 0.0f), -1.0f, 1.0f);
    c.drop = std::clamp(signal_number(runtime_inputs, RuntimeInputIds::Audio, "drop", 0.0f), 0.0f, 1.0f);
    const float confidence = std::clamp(
        signal_number(runtime_inputs, RuntimeInputIds::Audio, "beat_confidence", 0.0f), 0.0f, 1.0f);
    const float stability = std::clamp(
        signal_number(runtime_inputs, RuntimeInputIds::Audio, "tempo_stability", 0.0f), 0.0f, 1.0f);
    c.tempo_trust = std::clamp((confidence - 0.30f) / 0.50f, 0.0f, 1.0f) * stability;
    c.drop_counter = signal_counter(runtime_inputs, RuntimeInputIds::Audio, "drop_counter");
    c.section_counter = signal_counter(runtime_inputs, RuntimeInputIds::Audio, "section_counter");

    if (c.spotify) {
        float progress_ms = std::max(
            0.0f, signal_number(runtime_inputs, RuntimeInputIds::SpotifyPlayback, "progress_ms", 0.0f));
        if (const auto* playback = runtime_inputs.find(RuntimeInputIds::SpotifyPlayback); playback != nullptr)
            progress_ms += static_cast<float>(std::max(0.0, playback->age_seconds) * 1000.0);
        const float duration_ms = std::max(
            0.0f, signal_number(runtime_inputs, RuntimeInputIds::SpotifyPlayback, "duration_ms", 0.0f));
        if (duration_ms > 0.0f)
            progress_ms = std::min(progress_ms, duration_ms);
        if (duration_ms > 1000.0f) {
            c.spotify_progress = std::clamp(progress_ms / duration_ms, 0.0f, 1.0f);
            c.spotify_remaining_seconds = std::max(0.0f, (duration_ms - progress_ms) / 1000.0f);
        }
        else {
            float progress = signal_number(runtime_inputs, RuntimeInputIds::SpotifyPlayback, "progress", 0.0f);
            if (progress > 1.0f && progress <= 100.0f)
                progress *= 0.01f;
            c.spotify_progress = std::clamp(progress, 0.0f, 1.0f);
        }
    }

    if (c.audio_active) {
        const float sustained_energy = std::clamp(
            0.54f * c.loudness + 0.22f * c.bass + 0.12f * c.rhythmicity
                + 0.07f * std::max(0.0f, c.energy_trend) + 0.05f * c.brightness,
            0.0f, 1.0f);
        c.target_intensity = std::clamp(0.18f + sustained_energy * 0.76f, 0.20f, 0.94f);
        c.target_motion = std::clamp(
            0.22f + c.loudness * 0.25f + c.rhythmicity * 0.27f
                + c.tempo_trust * 0.15f + c.brightness * 0.06f,
            0.22f, 0.94f);
        c.mode = c.spotify ? "spotify+music" : "music";
    }
    else if (c.spotify) {
        // Spotify playback is still meaningful context even when desktop audio
        // capture is disabled. Prefer music/media looks without pretending the
        // unavailable FFT features are zero-energy music.
        c.target_intensity = 0.56f;
        c.target_motion = 0.58f;
        c.mode = "spotify";
    }
    else {
        c.target_intensity = silence ? 0.23f : 0.40f;
        c.target_motion = silence ? 0.27f : 0.44f;
        c.mode = silence ? "quiet" : "ambient";
    }

    const float journey_weight = c.audio_active ? 0.18f : (c.spotify ? 0.35f : (silence ? 0.25f : 0.85f));
    c.target_intensity += (journey.intensity - c.target_intensity) * journey_weight;
    c.target_motion += (journey.motion - c.target_motion) * journey_weight;
    return c;
}

Scenes::EffectiveSceneProfile profile_of(const std::shared_ptr<Scenes::Scene>& scene)
{
    const auto descriptor = scene->get_descriptor();
    return Scenes::effective_profile(descriptor, Scenes::find_variant(descriptor, scene->get_variant_id()));
}

/// Measured Pi render load beats the authored prior. Offloadable scenes are
/// judged by their prior while a desktop is connected, because their local
/// cost is then irrelevant.
float effective_cost(const std::shared_ptr<Scenes::Scene>& scene, const Scenes::EffectiveSceneProfile& profile,
                     const std::optional<float>& measured_load, bool desktop_available)
{
    if (!measured_load.has_value())
        return profile.performance_cost;
    if (desktop_available && scene->get_capabilities().supports_remote_rendering)
        return profile.performance_cost;
    return std::clamp(*measured_load * measured_load_to_cost, 0.0f, 1.5f);
}
}  // namespace AutomaticDirectorDetail

using namespace AutomaticDirectorDetail;

AutomaticDirector::Context AutomaticDirector::observe(const RuntimeInputs::Snapshot& runtime_inputs)
{
    last_context_ = context_for(runtime_inputs, journey_.frame());
    return last_context_;
}
