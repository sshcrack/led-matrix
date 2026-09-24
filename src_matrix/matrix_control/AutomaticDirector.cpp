#include "AutomaticDirector.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <optional>

#include "shared/matrix/input_ids.h"
#include "shared/matrix/scene_descriptor.h"

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

using DirectorContext = AutomaticDirector::Context;

// Descriptor performance costs are priors on a 0..1 scale where anything above
// the budget is expected to strain the Pi.
constexpr float performance_budget = 0.90f;
// SceneRenderer starts desktop offload once a frame exceeds 72% of its budget,
// so a measured load of 0.72 maps onto the edge of the descriptor budget.
constexpr float measured_load_to_cost = performance_budget / 0.72f;
// Additive ranking weights. A candidate starts at 1.0; the fit terms dominate
// (up to ~2.3), context bonuses are 0.1-0.4 each, and Spotify lifecycle terms
// are deliberately larger because track timing is a hard presentation cue.
namespace weight {
constexpr float journey_motif_music = 0.12f;
constexpr float journey_motif_ambient = 0.32f;
constexpr float intensity_fit = 1.42f;
constexpr float motion_fit = 0.86f;
constexpr float showcase = 0.24f;
constexpr float music_affinity_live = 1.02f;
constexpr float audio_reactive_live = 0.22f;
constexpr float audio_reactive_shader_live = 0.30f;
constexpr float music_affinity_spotify_only = 0.42f;
constexpr float ambient_affinity_quiet = 0.42f;
constexpr float scenic_shader_quiet = 0.18f;
constexpr float album_art_base = 1.10f;
constexpr float album_art_repeat_penalty = 0.48f;
constexpr float album_art_track_intro = 0.78f;
constexpr float album_art_track_outro = 0.58f;
constexpr float spotify_video_base = 0.86f;
constexpr float spotify_video_mid_track = 1.02f;
constexpr float spotify_video_intro_penalty = 0.72f;
constexpr float spotify_video_late_penalty = 1.35f;
constexpr float spotify_video_ending_penalty = 1.20f;
constexpr float over_budget = 2.35f;
constexpr float under_budget = 0.20f;
constexpr float freshness = 0.28f;
constexpr float continuity = 0.18f;
constexpr float abrupt_energy_jump = 0.24f;
constexpr float follows_energy_trend = 0.16f;
constexpr float favorite = 0.40f;
}  // namespace weight

constexpr std::uint64_t freshness_delay_ms = 6 * 60000;
constexpr std::uint64_t freshness_ramp_ms = 24 * 60000;
constexpr std::uint64_t failure_cooldown_base_ms = 2 * 60000;
constexpr std::uint64_t failure_cooldown_max_ms = 30 * 60000;

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
}  // namespace

AutomaticDirector::AutomaticDirector(std::uint64_t seed) : journey_(seed), seed_(seed), rng_(seed) {}

AutomaticDirector::Context AutomaticDirector::observe(const RuntimeInputs::Snapshot& runtime_inputs)
{
    last_context_ = context_for(runtime_inputs, journey_.frame());
    return last_context_;
}

std::string AutomaticDirector::look_key(const Scenes::Scene& scene)
{
    const auto& variant = scene.get_variant_id();
    return variant.empty() ? scene.get_name() : scene.get_name() + "/" + variant;
}

void AutomaticDirector::set_preferences(std::map<std::string, std::string> preferences)
{
    preferences_ = std::move(preferences);
}

bool AutomaticDirector::cooling_down(const std::string& scene) const
{
    const auto it = memory_.find(scene);
    return it != memory_.end() && it->second.cooldown_until_ms > clock_ms_;
}

bool AutomaticDirector::sync_track_context(const std::string& track_id)
{
    if (track_id == current_track_id_)
        return false;
    current_track_id_ = track_id;
    current_track_cover_shown_ = false;
    current_track_mv_shown_ = false;
    track_switch_pending_ = !track_id.empty();
    switch_events_primed_ = false;
    return true;
}

float AutomaticDirector::history_multiplier(
    const std::string& scene,
    const std::string& family,
    const std::string& role) const
{
    float multiplier = 1.0f;
    for (std::size_t index = 0; index < history_.size(); ++index) {
        const auto& entry = history_[history_.size() - 1 - index];
        const float scene_recency = index == 0 ? 0.16f : (index == 1 ? 0.34f : (index == 2 ? 0.58f : 0.76f));
        if (entry.scene == scene) {
            multiplier = std::min(multiplier, scene_recency);
            continue;
        }
        if (!family.empty() && entry.family == family) {
            const float family_penalty = index == 0 ? 0.62f : (index == 1 ? 0.73f : 0.84f);
            multiplier = std::min(multiplier, family_penalty);
        }
        if (!role.empty() && entry.role == role) {
            float role_penalty = 1.0f;
            if (role == "media")
                role_penalty = index == 0 ? 0.46f : (index == 1 ? 0.68f : 0.86f);
            else if (role == "director")
                role_penalty = index == 0 ? 0.52f : (index == 1 ? 0.72f : 0.88f);
            else if (role == "reactive")
                role_penalty = index == 0 ? 0.80f : (index == 1 ? 0.90f : 0.96f);
            else if (role == "ambient")
                role_penalty = index == 0 ? 0.88f : 0.96f;
            multiplier = std::min(multiplier, role_penalty);
        }
    }
    return multiplier;
}

std::vector<AutomaticDirector::Candidate> AutomaticDirector::rank(
    const std::vector<std::shared_ptr<Scenes::Scene>>& scenes,
    const RuntimeInputs::Snapshot& runtime_inputs,
    const std::string& exclude_name) const
{
    std::vector<Candidate> ranked;
    const auto context = context_for(runtime_inputs, journey_.frame());
    const bool desktop_available = runtime_inputs.available(RuntimeInputIds::Desktop);

    // Scenes that recently crashed or bailed out immediately sit out a backoff
    // cooldown. If nothing else is eligible they are still better than the
    // fallback screen, so the second pass ignores cooldowns.
    bool skipped_cooling = false;
    for (const bool honor_cooldowns : {true, false}) {
        if (!honor_cooldowns && (!ranked.empty() || !skipped_cooling))
            break;
        for (const auto& scene : scenes) {
            if (!scene || scene->get_name() == exclude_name)
                continue;
            if (honor_cooldowns && cooling_down(scene->get_name())) {
                skipped_cooling = true;
                continue;
            }
            const auto descriptor = scene->get_descriptor();
            if (!descriptor.automatic_eligible)
                continue;
            if (!RuntimeInputs::satisfies(scene->get_effective_runtime_inputs(), runtime_inputs))
                continue;

            const auto* variant = Scenes::find_variant(descriptor, scene->get_variant_id());
            const auto preference = preferences_.find(look_key(*scene));
            static const std::string no_preference;
            const std::string& preferred = preference != preferences_.end() ? preference->second : no_preference;
            if (preferred == "hidden" || (preferred.empty() && !Scenes::automatic_default(descriptor, variant)))
                continue;
            const auto profile = Scenes::effective_profile(descriptor, variant);
            const auto role = presentation_role(profile);

            // Tool availability makes SpotifyMV manually usable, but Automatic Mode
            // waits for a decoded first frame for this exact track. This keeps the
            // loading animation out of unattended playback.
            if (has_tag(profile.tags, "spotify-video")) {
                if (!context.spotify_mv_ready)
                    continue;
                if (current_track_mv_shown_ && !current_track_id_.empty()
                    && current_track_id_ == context.spotify_track_id)
                    continue;
            }

            Candidate candidate;
            candidate.scene = scene;
            candidate.score = 1.0f;
            const auto journey = journey_.frame();
            candidate.reasons.push_back("journey phase: " + std::string(journey.phase));
            if (has_tag(profile.tags, journey.motif) || descriptor.family == journey.motif) {
                const float motif_strength = 4.0f * journey.progress * (1.0f - journey.progress);
                candidate.score += (context.audio_active ? weight::journey_motif_music : weight::journey_motif_ambient) * motif_strength;
                candidate.reasons.push_back("continues the " + std::string(journey.motif) + " journey motif");
            }

            const float intensity_fit = 1.0f - std::abs(profile.intensity - context.target_intensity);
            const float motion_fit = 1.0f - std::abs(profile.motion - context.target_motion);
            candidate.score += intensity_fit * weight::intensity_fit + motion_fit * weight::motion_fit;
            candidate.reasons.push_back(intensity_fit > 0.82f ? "energy closely fits context" : "energy fits context");
            if (motion_fit > 0.82f)
                candidate.reasons.push_back("motion fits current pace");

            // Curated repo shaders are authored and visually reviewed specifically
            // for the physical matrix. When the desktop renderer is available they
            // should not merely tie older CPU-rendered approximations with the same
            // semantic profile. `showcase` is metadata-driven so future agent-made
            // shaders can opt into the same quality tier without hard-coded names.
            if (has_tag(profile.tags, "showcase")) {
                candidate.score += weight::showcase;
                candidate.reasons.push_back("visually curated showcase scene");
            }

            if (context.audio_active) {
                candidate.score += profile.music_affinity * weight::music_affinity_live;
                if (profile.music_affinity > 0.72f)
                    candidate.reasons.push_back("strong live-music affinity");
                if (has_tag(profile.tags, "audio-reactive"))
                    candidate.score += weight::audio_reactive_live;
                if (has_tag(profile.tags, "shader") && has_tag(profile.tags, "audio-reactive")) {
                    candidate.score += weight::audio_reactive_shader_live;
                    candidate.reasons.push_back("GPU shader uses live music analysis");
                }
                if (has_any_tag(profile.tags, {"depth", "tunnel"}) && context.bass > 0.28f) {
                    candidate.score += context.bass * 0.30f + context.beat_strength * 0.12f;
                    candidate.reasons.push_back("bass supports depth motion");
                }
                if (has_tag(profile.tags, "particles") && context.rhythmicity > 0.24f) {
                    candidate.score += context.rhythmicity * 0.34f;
                    candidate.reasons.push_back("percussion suits particle motion");
                }
                if (has_any_tag(profile.tags, {"geometric", "symmetry"}) && context.tempo_trust > 0.34f) {
                    candidate.score += context.tempo_trust * 0.24f + context.brightness * 0.08f;
                    candidate.reasons.push_back("stable tempo suits geometry");
                }
                if (has_any_tag(profile.tags, {"flow", "ribbons", "organic"}) && context.rhythmicity < 0.48f)
                    candidate.score += (0.48f - context.rhythmicity) * 0.34f;
                if (context.target_intensity < 0.38f
                    && has_any_tag(profile.tags, {"calm", "soft", "minimal", "airy"}))
                    candidate.score += 0.32f;
                if (context.target_intensity > 0.72f
                    && has_any_tag(profile.tags, {"energetic", "vivid", "dense"}))
                    candidate.score += 0.30f + context.drop * 0.14f;
            }
            else if (context.spotify) {
                candidate.score += profile.music_affinity * weight::music_affinity_spotify_only;
                if (profile.music_affinity > 0.70f)
                    candidate.reasons.push_back("fits Spotify playback without audio capture");
            }
            else {
                candidate.score += (1.0f - profile.music_affinity) * weight::ambient_affinity_quiet;
                if (has_tag(profile.tags, "shader") && has_any_tag(profile.tags, {"ambient", "scenic", "calm"})) {
                    candidate.score += weight::scenic_shader_quiet;
                    candidate.reasons.push_back("curated shader fits ambient playback");
                }
                if (context.audio_available && context.target_intensity < 0.30f && profile.intensity > 0.70f)
                    candidate.score -= 0.55f;
            }

            if (context.spotify) {
                const bool album_art = has_tag(profile.tags, "album-art");
                const bool spotify_video = has_tag(profile.tags, "spotify-video");
                if (album_art) {
                    candidate.score += weight::album_art_base;
                    if (current_track_cover_shown_ && context.spotify_progress >= 0.14f
                        && context.spotify_progress <= 0.82f)
                        candidate.score -= weight::album_art_repeat_penalty;
                    if (context.spotify_progress < 0.12f) {
                        candidate.score += weight::album_art_track_intro;
                        candidate.reasons.push_back("album art suits the start of this track");
                    }
                    else if (context.spotify_progress > 0.84f) {
                        candidate.score += weight::album_art_track_outro;
                        candidate.reasons.push_back("album art is a clean end-of-track choice");
                    }
                    else {
                        candidate.score += 0.10f;
                        candidate.reasons.push_back("Spotify playback is available");
                    }
                }
                if (spotify_video) {
                    candidate.score += weight::spotify_video_base;
                    if (context.spotify_progress >= 0.10f && context.spotify_progress <= 0.78f) {
                        candidate.score += weight::spotify_video_mid_track;
                        candidate.reasons.push_back("music video fits the middle of the track");
                    }
                    else if (context.spotify_progress < 0.10f) {
                        candidate.score -= weight::spotify_video_intro_penalty;
                        candidate.reasons.push_back("video startup deferred behind track intro");
                    }
                    else {
                        candidate.score -= weight::spotify_video_late_penalty;
                        candidate.reasons.push_back("too late in track to start a music video");
                    }
                    if (context.spotify_remaining_seconds > 0.0f && context.spotify_remaining_seconds < 35.0f)
                        candidate.score -= weight::spotify_video_ending_penalty;
                    if (context.spotify_mv_ready)
                        candidate.reasons.push_back("prepared SpotifyMV frame matches this track");
                }
            }

            const auto memory = memory_.find(scene->get_name());
            const std::optional<float> measured_load = memory != memory_.end()
                ? memory->second.render_load : std::nullopt;
            const float cost = effective_cost(scene, profile, measured_load, desktop_available);
            if (cost > performance_budget) {
                candidate.score -= (cost - performance_budget) * weight::over_budget;
                candidate.reasons.push_back(measured_load.has_value() && cost != profile.performance_cost
                    ? "measured Pi render load is too high" : "deprioritized for Pi render cost");
            }
            else {
                candidate.score += weight::under_budget * (performance_budget - cost);
            }

            // Long-unseen looks slowly gain priority so a large catalog is actually
            // explored instead of the same well-fitting handful winning forever.
            const bool shown_before = memory != memory_.end() && memory->second.last_shown_ms.has_value();
            const std::uint64_t unseen_ms = shown_before ? clock_ms_ - *memory->second.last_shown_ms : std::numeric_limits<std::uint64_t>::max();
            const float freshness = unseen_ms <= freshness_delay_ms ? 0.0f
                : std::min(1.0f, static_cast<float>(unseen_ms - freshness_delay_ms) / static_cast<float>(freshness_ramp_ms));
            candidate.score += weight::freshness * freshness;
            if (shown_before && freshness >= 0.5f)
                candidate.reasons.push_back("not shown for a while");
            if (preferred == "favorite") {
                candidate.score += weight::favorite;
                candidate.reasons.push_back("marked as a favorite");
            }

            if (!history_.empty()) {
                const auto& previous = history_.back();
                const float intensity_delta = std::abs(profile.intensity - previous.intensity);
                const float motion_delta = std::abs(profile.motion - previous.motion);
                const float continuity = 1.0f - std::clamp(0.62f * intensity_delta + 0.38f * motion_delta, 0.0f, 1.0f);
                candidate.score += continuity * weight::continuity;
                if (continuity > 0.78f)
                    candidate.reasons.push_back("continues the visual trajectory");

                // Ordinary handoffs should form an arc instead of channel surfing.
                // A detected drop is the deliberate exception: it may justify a
                // large jump in visual intensity.
                if (intensity_delta > 0.48f && context.drop < 0.35f)
                    candidate.score -= weight::abrupt_energy_jump;
                if (context.energy_trend > 0.14f && profile.intensity > previous.intensity + 0.06f)
                    candidate.score += weight::follows_energy_trend;
                else if (context.energy_trend < -0.14f && profile.intensity + 0.06f < previous.intensity)
                    candidate.score += weight::follows_energy_trend;
            }

            const float history = history_multiplier(scene->get_name(), descriptor.family, role);
            candidate.score *= history;
            if (history < 0.9f)
                candidate.reasons.push_back("recent style/scene repetition penalty");

            ranked.push_back(std::move(candidate));
        }
    }

    std::stable_sort(ranked.begin(), ranked.end(), [](const Candidate& a, const Candidate& b) {
        if (std::abs(a.score - b.score) > 0.0001f)
            return a.score > b.score;
        const auto a_key = a.scene->get_name() + ":" + a.scene->get_variant_id();
        const auto b_key = b.scene->get_name() + ":" + b.scene->get_variant_id();
        return a_key < b_key;
    });
    return ranked;
}

AutomaticDirector::Decision AutomaticDirector::choose(
    const std::vector<std::shared_ptr<Scenes::Scene>>& scenes,
    const RuntimeInputs::Snapshot& runtime_inputs,
    const std::string& exclude_name,
    const std::shared_ptr<Scenes::Scene>& preferred_scene)
{
    Decision decision;
    const auto context = observe(runtime_inputs);
    sync_track_context(context.spotify ? context.spotify_track_id : std::string{});
    last_exclude_name_ = exclude_name;
    ++decision_count_;
    decision.ranked = rank(scenes, runtime_inputs, exclude_name);
    if (decision.ranked.empty()) {
        last_scene_.clear();
        last_variant_.clear();
        last_score_ = 0.0f;
        last_reasons_ = {"no eligible automatic scene"};
        last_ranked_.clear();
        return decision;
    }

    // Randomness is limited to candidates that are genuinely competitive with
    // the best score. A fixed top-N pool used to occasionally admit a clearly
    // worse fourth-place scene just because the catalog was small.
    constexpr float score_window = 0.92f;
    constexpr std::size_t max_pool_size = 5;
    std::size_t pool_size = 1;
    while (pool_size < decision.ranked.size() && pool_size < max_pool_size
        && decision.ranked.front().score - decision.ranked[pool_size].score <= score_window) {
        ++pool_size;
    }

    std::size_t selected = decision.ranked.size();
    if (preferred_scene) {
        const auto preferred_name = preferred_scene->get_name();
        const auto preferred_variant = preferred_scene->get_variant_id();
        for (std::size_t i = 0; i < decision.ranked.size(); ++i) {
            const auto& candidate_scene = decision.ranked[i].scene;
            if (candidate_scene == preferred_scene
                || (candidate_scene->get_name() == preferred_name
                    && candidate_scene->get_variant_id() == preferred_variant)) {
                selected = i;
                break;
            }
        }
    }

    if (selected == decision.ranked.size()) {
        const float best_score = decision.ranked.front().score;
        std::vector<double> weights(pool_size);
        constexpr double selection_temperature = 0.30;
        for (std::size_t i = 0; i < pool_size; ++i)
            weights[i] = std::exp(static_cast<double>(decision.ranked[i].score - best_score) / selection_temperature);
        std::discrete_distribution<std::size_t> distribution(weights.begin(), weights.end());
        selected = distribution(rng_);
    }

    decision.scene = decision.ranked[selected].scene;
    decision.score = decision.ranked[selected].score;
    decision.reasons = decision.ranked[selected].reasons;
    last_scene_ = decision.scene->get_name();
    last_variant_ = decision.scene->get_variant_id();
    last_score_ = decision.score;
    last_reasons_ = decision.reasons;
    last_ranked_ = decision.ranked;
    return decision;
}

tmillis_t AutomaticDirector::presentation_duration(
    const std::shared_ptr<Scenes::Scene>& scene,
    const RuntimeInputs::Snapshot& runtime_inputs) const
{
    if (!scene)
        return 20000;

    const auto context = context_for(runtime_inputs, journey_.frame());
    const auto descriptor = scene->get_descriptor();
    const auto* variant = Scenes::find_variant(descriptor, scene->get_variant_id());
    const auto profile = Scenes::effective_profile(descriptor, variant);

    float seconds = 30.0f;
    if (has_tag(profile.tags, "spotify-video")) {
        seconds = 42.0f;
        if (context.spotify_remaining_seconds > 0.0f)
            seconds = std::min(seconds, std::max(12.0f, context.spotify_remaining_seconds - 7.0f));
    }
    else if (has_tag(profile.tags, "album-art")) {
        seconds = context.spotify_progress < 0.12f ? 24.0f : 20.0f;
    }
    else if (context.audio_active && profile.music_affinity > 0.72f) {
        seconds = profile.intensity > 0.82f ? 18.0f : (profile.intensity < 0.48f ? 27.0f : 22.0f);
    }
    else if (has_any_tag(profile.tags, {"calm", "soft", "minimal"})) {
        seconds = context.audio_active ? 34.0f : 42.0f;
    }
    else if (has_any_tag(profile.tags, {"energetic", "dense", "vivid"})) {
        seconds = 21.0f;
    }
    else if (profile.motion < 0.42f) {
        seconds = context.audio_active ? 31.0f : 38.0f;
    }

    if (presentation_role(profile) != "media") {
        const float influence = context.audio_active ? 0.25f : 1.0f;
        seconds *= 1.0f + (journey_.frame().dwell_scale - 1.0f) * influence;
    }

    const auto memory = memory_.find(scene->get_name());
    const auto measured_load = memory != memory_.end() ? memory->second.render_load : std::nullopt;
    if (effective_cost(scene, profile, measured_load, runtime_inputs.available(RuntimeInputIds::Desktop))
        > performance_budget + 0.12f)
        seconds = std::min(seconds, 18.0f);

    return static_cast<tmillis_t>(std::clamp(seconds, 12.0f, 75.0f) * 1000.0f);
}

AutomaticDirector::SwitchOpportunity AutomaticDirector::consider_switch(
    const std::vector<std::shared_ptr<Scenes::Scene>>& scenes,
    const std::shared_ptr<Scenes::Scene>& current_scene,
    const RuntimeInputs::Snapshot& runtime_inputs,
    tmillis_t elapsed_ms,
    tmillis_t planned_ms)
{
    SwitchOpportunity result;
    if (!current_scene)
        return result;

    const auto context = observe(runtime_inputs);
    const bool track_changed = sync_track_context(context.spotify ? context.spotify_track_id : std::string{});

    bool drop_event = false;
    bool section_event = false;
    if (!switch_events_primed_) {
        seen_drop_ = context.drop_counter;
        seen_section_ = context.section_counter;
        switch_events_primed_ = true;
    } else {
        if (context.drop_counter < seen_drop_) seen_drop_ = context.drop_counter;
        else drop_event = context.drop_counter > seen_drop_;
        if (context.section_counter < seen_section_) seen_section_ = context.section_counter;
        else section_event = context.section_counter > seen_section_;
        seen_drop_ = context.drop_counter;
        seen_section_ = context.section_counter;
    }

    const auto current_profile = profile_of(current_scene);
    const auto fit_error = [&](const Scenes::EffectiveSceneProfile& profile) {
        return std::abs(profile.intensity - context.target_intensity) * 0.62f
            + std::abs(profile.motion - context.target_motion) * 0.38f;
    };
    const float current_error = fit_error(current_profile);
    result.current_score = 1.0f - current_error;

    // This runs on the render thread several times per second, so the full
    // catalog is only ranked once a trigger actually needs an alternative.
    std::optional<std::vector<Candidate>> alternatives;
    const auto ranked_alternatives = [&]() -> const std::vector<Candidate>& {
        if (!alternatives.has_value()) {
            alternatives = rank(scenes, runtime_inputs, current_scene->get_name());
            if (!alternatives->empty())
                result.alternative_score = alternatives->front().score;
        }
        return *alternatives;
    };
    const auto best = [&]() -> const Candidate* {
        const auto& ranked = ranked_alternatives();
        return ranked.empty() ? nullptr : &ranked.front();
    };

    // Energy-mismatch switches look at the competitive head of the ranking, not
    // just the single top score, and take the best-placed look that clearly
    // fits the current energy better than what is showing.
    const auto better_fit = [&](float margin) -> const Candidate* {
        const auto& ranked = ranked_alternatives();
        for (std::size_t i = 0; i < std::min<std::size_t>(5, ranked.size()); ++i) {
            if (fit_error(profile_of(ranked[i].scene)) + margin < current_error)
                return &ranked[i];
        }
        return nullptr;
    };

    auto request = [&](std::string reason, std::shared_ptr<Scenes::Scene> preferred = nullptr) {
        result.should_switch = true;
        result.reason = std::move(reason);
        result.preferred_scene = std::move(preferred);
        last_switch_reason_ = result.reason;
    };

    const auto best_with_tag = [&](std::string_view tag) -> std::shared_ptr<Scenes::Scene> {
        for (const auto& candidate : ranked_alternatives()) {
            if (has_tag(profile_of(candidate.scene).tags, tag))
                return candidate.scene;
        }
        return nullptr;
    };

    if (const auto preference = preferences_.find(look_key(*current_scene));
        preference != preferences_.end() && preference->second == "hidden") {
        request("hidden from Automatic Mode by the user");
        return result;
    }

    const bool current_is_cover = has_tag(current_profile.tags, "album-art");
    const bool current_is_spotify_video = has_tag(current_profile.tags, "spotify-video");
    if ((track_changed || track_switch_pending_) && context.spotify && elapsed_ms >= 1500) {
        if (current_is_cover) {
            current_track_cover_shown_ = true;
            track_switch_pending_ = false;
        } else if (!ranked_alternatives().empty()) {
            request("Spotify track changed; introduce the new track visually", best_with_tag("album-art"));
            return result;
        }
    }

    // Once Automatic Mode deliberately enters the prepared music video, let it
    // read as a feature segment instead of immediately reacting to the next
    // ordinary section/energy change. Track changes above may still cut it short.
    if (current_is_spotify_video && elapsed_ms < 26000 && elapsed_ms < planned_ms)
        return result;

    if (context.spotify_mv_ready && !current_track_mv_shown_
        && context.spotify_progress >= 0.10f && context.spotify_progress <= 0.74f
        && elapsed_ms >= 8000) {
        if (const auto* candidate = best(); candidate && has_tag(profile_of(candidate->scene).tags, "spotify-video")) {
            request("prepared SpotifyMV reached a clean mid-track insertion point", candidate->scene);
            return result;
        }
    }

    // The planned dwell is a target, not a hard cut. While music plays, wait a
    // few seconds for the next section or drop so the handoff lands on a
    // musical phrase instead of mid-phrase. Media scenes follow the track.
    if (planned_ms > 0 && elapsed_ms >= planned_ms) {
        const bool phrase_boundary = section_event || drop_event;
        const bool wait_for_phrase = context.audio_active && presentation_role(current_profile) != "media";
        if (!wait_for_phrase || phrase_boundary || elapsed_ms >= planned_ms + phrase_grace_ms)
            request(phrase_boundary && wait_for_phrase ? "scene completed on a musical phrase boundary"
                                                       : "planned scene dwell completed");
        return result;
    }

    if (drop_event && context.audio_active && elapsed_ms >= 5500) {
        if (const auto* candidate = best();
            candidate && profile_of(candidate->scene).intensity > current_profile.intensity + 0.10f) {
            request("music drop supports a higher-energy visual", candidate->scene);
            return result;
        }
    }

    // Sections change every 15-30 s in most music, so a section alone is not a
    // reason to cut mid-scene. It is only used when the current look clearly
    // stopped fitting; otherwise sections just align the planned handoff above.
    if (section_event && context.audio_active && elapsed_ms >= 8500) {
        if (const auto* candidate = better_fit(0.10f)) {
            request("musical section change moved the energy away from this scene", candidate->scene);
            return result;
        }
    }

    if (elapsed_ms >= 12000 && current_error > 0.25f && clock_ms_ >= next_fit_check_ms_) {
        next_fit_check_ms_ = clock_ms_ + 2000;
        if (const auto* candidate = better_fit(0.16f)) {
            request(context.audio_active ? "sustained music energy no longer fits the current scene"
                                         : "visual journey has moved beyond the current scene", candidate->scene);
            return result;
        }
    }

    return result;
}

void AutomaticDirector::advance_journey(tmillis_t elapsed_ms)
{
    if (elapsed_ms <= 0)
        return;
    journey_.advance(static_cast<std::uint64_t>(elapsed_ms));
    clock_ms_ += static_cast<std::uint64_t>(elapsed_ms);
}

void AutomaticDirector::record_played(const std::shared_ptr<Scenes::Scene>& scene)
{
    if (!scene)
        return;
    const auto descriptor = scene->get_descriptor();
    const auto* variant = Scenes::find_variant(descriptor, scene->get_variant_id());
    const auto profile = Scenes::effective_profile(descriptor, variant);
    history_.push_back({scene->get_name(), descriptor.family, scene->get_variant_id(), presentation_role(profile),
                        profile.intensity, profile.motion});
    while (history_.size() > 8)
        history_.pop_front();
    auto& memory = memory_[scene->get_name()];
    memory.last_shown_ms = clock_ms_;
    ++memory.presentations;

    if (!current_track_id_.empty()) {
        if (has_tag(profile.tags, "album-art")) {
            current_track_cover_shown_ = true;
            track_switch_pending_ = false;
        }
        if (has_tag(profile.tags, "spotify-video"))
            current_track_mv_shown_ = true;
    }
}

void AutomaticDirector::report_outcome(const std::shared_ptr<Scenes::Scene>& scene, const PresentationOutcome& outcome)
{
    if (!scene)
        return;
    auto& memory = memory_[scene->get_name()];
    if (outcome.render_load.has_value()) {
        const float load = std::max(0.0f, *outcome.render_load);
        memory.render_load = memory.render_load.has_value() ? *memory.render_load * 0.65f + load * 0.35f : load;
    }
    if (!outcome.failed) {
        memory.failures = 0;
        return;
    }
    ++memory.failures;
    const auto shift = std::min<std::uint32_t>(memory.failures - 1, 4);
    memory.cooldown_until_ms = clock_ms_ + std::min(failure_cooldown_base_ms << shift, failure_cooldown_max_ms);
}

void AutomaticDirector::reseed(std::uint64_t seed)
{
    if (seed == 0)
        seed = 1;
    journey_ = VisualJourney(seed);
    seed_ = seed;
    rng_.seed(seed_);
    history_.clear();
    for (auto& [_, memory] : memory_) {
        memory.last_shown_ms.reset();
        memory.presentations = 0;
    }
    last_context_ = {};
    last_scene_.clear();
    last_variant_.clear();
    last_score_ = 0.0f;
    last_reasons_.clear();
    last_ranked_.clear();
    decision_count_ = 0;
    last_exclude_name_.clear();
    current_track_id_.clear();
    current_track_cover_shown_ = false;
    current_track_mv_shown_ = false;
    track_switch_pending_ = false;
    switch_events_primed_ = false;
    seen_drop_ = 0;
    seen_section_ = 0;
    next_fit_check_ms_ = 0;
    last_switch_reason_.clear();
}

nlohmann::json AutomaticDirector::diagnostics() const
{
    nlohmann::json history = nlohmann::json::array();
    for (const auto& entry : history_) {
        history.push_back({{"scene", entry.scene},
                           {"family", entry.family},
                           {"variant", entry.variant},
                           {"role", entry.role},
                           {"intensity", entry.intensity},
                           {"motion", entry.motion}});
    }
    nlohmann::json candidates = nlohmann::json::array();
    for (std::size_t i = 0; i < std::min<std::size_t>(10, last_ranked_.size()); ++i) {
        const auto& candidate = last_ranked_[i];
        candidates.push_back({{"scene", candidate.scene->get_name()},
                              {"variant", candidate.scene->get_variant_id()},
                              {"score", candidate.score},
                              {"reasons", candidate.reasons}});
    }
    std::vector<std::string> names;
    names.reserve(memory_.size());
    for (const auto& [name, _] : memory_)
        names.push_back(name);
    std::sort(names.begin(), names.end());
    nlohmann::json scenes = nlohmann::json::array();
    for (const auto& name : names) {
        const auto& memory = memory_.at(name);
        scenes.push_back({{"scene", name},
                          {"presentations", memory.presentations},
                          {"last_shown_ago_ms", memory.last_shown_ms.has_value()
                               ? nlohmann::json(clock_ms_ - *memory.last_shown_ms) : nlohmann::json(nullptr)},
                          {"render_load", memory.render_load.has_value()
                               ? nlohmann::json(*memory.render_load) : nlohmann::json(nullptr)},
                          {"failures", memory.failures},
                          {"cooldown_remaining_ms", memory.cooldown_until_ms > clock_ms_
                               ? memory.cooldown_until_ms - clock_ms_ : 0}});
    }
    const auto& c = last_context_;
    const auto journey = journey_.frame();
    return {{"journey", {{"phase", journey.phase}, {"motif", journey.motif},
                          {"cycle", journey_.cycle()}, {"progress", journey.progress},
                          {"phase_progress", journey.phase_progress},
                          {"elapsed_ms", journey_.elapsed_ms()}, {"duration_ms", journey_.duration_ms()},
                          {"intensity", journey.intensity}, {"motion", journey.motion},
                          {"dwell_scale", journey.dwell_scale}}},
            {"seed", std::to_string(seed_)},
            {"decision_count", decision_count_},
            {"clock_ms", clock_ms_},
            {"performance_budget", performance_budget},
            {"context",
             {{"mode", c.mode},
              {"audio_available", c.audio_available},
              {"audio_active", c.audio_active},
              {"spotify_available", c.spotify},
              {"spotify_track_id", c.spotify_track_id},
              {"spotify_mv_ready", c.spotify_mv_ready},
              {"spotify_mv_tools_ready", c.spotify_mv_tools_ready},
              {"spotify_mv_first_frame_ready", c.spotify_mv_first_frame_ready},
              {"spotify_mv_track_id", c.spotify_mv_track_id},
              {"spotify_mv_state", c.spotify_mv_state},
              {"spotify_progress", c.spotify_progress},
              {"spotify_remaining_seconds", c.spotify_remaining_seconds},
              {"loudness", c.loudness},
              {"bass", c.bass},
              {"treble", c.treble},
              {"onset", c.onset},
              {"rhythmicity", c.rhythmicity},
              {"brightness", c.brightness},
              {"tempo_trust", c.tempo_trust},
              {"target_intensity", c.target_intensity},
              {"target_motion", c.target_motion},
              {"excluded_scene", last_exclude_name_}}},
            {"scenes", std::move(scenes)},
            {"last_scene", last_scene_},
            {"last_variant", last_variant_},
            {"last_score", last_score_},
            {"last_reasons", last_reasons_},
            {"last_switch_reason", last_switch_reason_},
            {"track_session",
             {{"track_id", current_track_id_},
              {"cover_shown", current_track_cover_shown_},
              {"spotify_mv_shown", current_track_mv_shown_},
              {"track_switch_pending", track_switch_pending_}}},
            {"history", std::move(history)},
            {"candidates", std::move(candidates)}};
}
