#include "AutomaticDirector.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>

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

struct DirectorContext {
    bool audio_available = false;
    bool audio_active = false;
    bool spotify = false;
    bool spotify_mv_ready = false;
    float loudness = 0.35f;
    float loudness_fast = 0.35f;
    float bass = 0.0f;
    float treble = 0.0f;
    float brightness = 0.0f;
    float onset = 0.0f;
    float kick = 0.0f;
    float snare = 0.0f;
    float hihat = 0.0f;
    float beat_strength = 0.0f;
    float rhythmicity = 0.0f;
    float energy_trend = 0.0f;
    float drop = 0.0f;
    float tempo_trust = 0.0f;
    float spotify_progress = 0.0f;
    float spotify_remaining_seconds = 0.0f;
    float target_intensity = 0.42f;
    float target_motion = 0.46f;
    float performance_budget = 0.88f;
    std::string mode = "ambient";
};

DirectorContext context_for(const RuntimeInputs::Snapshot& runtime_inputs, float render_quality)
{
    DirectorContext c;
    c.audio_available = runtime_inputs.available(RuntimeInputIds::Audio);
    c.spotify = runtime_inputs.available(RuntimeInputIds::SpotifyPlayback)
        && signal_bool(runtime_inputs, RuntimeInputIds::SpotifyPlayback, "playing", true);
    c.spotify_mv_ready = runtime_inputs.available(RuntimeInputIds::SpotifyMVReady);

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

    if (c.spotify) {
        const float progress_ms = std::max(
            0.0f, signal_number(runtime_inputs, RuntimeInputIds::SpotifyPlayback, "progress_ms", 0.0f));
        const float duration_ms = std::max(
            0.0f, signal_number(runtime_inputs, RuntimeInputIds::SpotifyPlayback, "duration_ms", 0.0f));
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

    c.performance_budget = render_quality < 0.78f ? 0.44f
        : (render_quality < 0.90f ? 0.61f : (render_quality < 0.97f ? 0.76f : 0.90f));
    return c;
}
}  // namespace

AutomaticDirector::AutomaticDirector(std::uint64_t seed) : seed_(seed), rng_(seed) {}

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
    const auto context = context_for(runtime_inputs, render_quality_);

    for (const auto& scene : scenes) {
        if (!scene || scene->get_name() == exclude_name)
            continue;
        const auto descriptor = scene->get_descriptor();
        if (!descriptor.automatic_eligible)
            continue;
        if (!RuntimeInputs::satisfies(scene->get_effective_runtime_inputs(), runtime_inputs))
            continue;

        const auto* variant = Scenes::find_variant(descriptor, scene->get_variant_id());
        const auto profile = Scenes::effective_profile(descriptor, variant);
        const auto role = presentation_role(profile);
        Candidate candidate;
        candidate.scene = scene;
        candidate.score = 1.0f;

        const float intensity_fit = 1.0f - std::abs(profile.intensity - context.target_intensity);
        const float motion_fit = 1.0f - std::abs(profile.motion - context.target_motion);
        candidate.score += intensity_fit * 1.42f + motion_fit * 0.86f;
        candidate.reasons.push_back(intensity_fit > 0.82f ? "energy closely fits context" : "energy fits context");
        if (motion_fit > 0.82f)
            candidate.reasons.push_back("motion fits current pace");

        if (context.audio_active) {
            candidate.score += profile.music_affinity * 1.02f;
            if (profile.music_affinity > 0.72f)
                candidate.reasons.push_back("strong live-music affinity");
            if (has_tag(profile.tags, "audio-reactive"))
                candidate.score += 0.22f;
            if (has_tag(profile.tags, "director")) {
                // MusicDirector remains a strong long-form option, but it is no
                // longer allowed to crowd every purpose-built reactive/media
                // scene out of Automatic Mode.
                candidate.score += 0.48f;
                candidate.reasons.push_back("adapts continuously to music");
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
            candidate.score += profile.music_affinity * 0.42f;
            if (profile.music_affinity > 0.70f)
                candidate.reasons.push_back("fits Spotify playback without audio capture");
        }
        else {
            candidate.score += (1.0f - profile.music_affinity) * 0.42f;
            if (context.audio_available && context.target_intensity < 0.30f && profile.intensity > 0.70f)
                candidate.score -= 0.55f;
        }

        if (context.spotify) {
            const bool album_art = has_tag(profile.tags, "album-art");
            const bool spotify_video = has_tag(profile.tags, "spotify-video");
            if (album_art) {
                candidate.score += 1.10f;
                if (context.spotify_progress < 0.12f) {
                    candidate.score += 0.78f;
                    candidate.reasons.push_back("album art suits the start of this track");
                }
                else if (context.spotify_progress > 0.84f) {
                    candidate.score += 0.58f;
                    candidate.reasons.push_back("album art is a clean end-of-track choice");
                }
                else {
                    candidate.score += 0.10f;
                    candidate.reasons.push_back("Spotify playback is available");
                }
            }
            if (spotify_video) {
                candidate.score += 0.86f;
                if (context.spotify_progress >= 0.10f && context.spotify_progress <= 0.78f) {
                    candidate.score += 1.02f;
                    candidate.reasons.push_back("music video fits the middle of the track");
                }
                else if (context.spotify_progress < 0.10f) {
                    candidate.score -= 0.72f;
                    candidate.reasons.push_back("video startup deferred behind track intro");
                }
                else {
                    candidate.score -= 1.35f;
                    candidate.reasons.push_back("too late in track to start a music video");
                }
                if (context.spotify_remaining_seconds > 0.0f && context.spotify_remaining_seconds < 35.0f)
                    candidate.score -= 1.20f;
                if (context.spotify_mv_ready)
                    candidate.reasons.push_back("SpotifyMV desktop pipeline is ready");
            }
        }

        if (profile.performance_cost > context.performance_budget) {
            const float excess = profile.performance_cost - context.performance_budget;
            candidate.score -= excess * 2.35f;
            candidate.reasons.push_back("deprioritized for current render headroom");
        }
        else {
            candidate.score += 0.20f * (context.performance_budget - profile.performance_cost);
        }

        const float history = history_multiplier(scene->get_name(), descriptor.family, role);
        candidate.score *= history;
        if (history < 0.9f)
            candidate.reasons.push_back("recent style/scene repetition penalty");

        ranked.push_back(std::move(candidate));
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
    const std::string& exclude_name)
{
    Decision decision;
    const auto context = context_for(runtime_inputs, render_quality_);
    last_audio_available_ = context.audio_available;
    last_audio_active_ = context.audio_active;
    last_spotify_available_ = context.spotify;
    last_spotify_mv_ready_ = context.spotify_mv_ready;
    last_loudness_ = context.loudness;
    last_bass_ = context.bass;
    last_treble_ = context.treble;
    last_onset_ = context.onset;
    last_rhythmicity_ = context.rhythmicity;
    last_brightness_ = context.brightness;
    last_tempo_trust_ = context.tempo_trust;
    last_spotify_progress_ = context.spotify_progress;
    last_spotify_remaining_seconds_ = context.spotify_remaining_seconds;
    last_target_intensity_ = context.target_intensity;
    last_target_motion_ = context.target_motion;
    last_performance_budget_ = context.performance_budget;
    last_mode_ = context.mode;
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

    const float best_score = decision.ranked.front().score;
    std::vector<double> weights(pool_size);
    constexpr double selection_temperature = 0.30;
    for (std::size_t i = 0; i < pool_size; ++i)
        weights[i] = std::exp(static_cast<double>(decision.ranked[i].score - best_score) / selection_temperature);
    std::discrete_distribution<std::size_t> distribution(weights.begin(), weights.end());
    const auto selected = distribution(rng_);

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

    const auto context = context_for(runtime_inputs, render_quality_);
    const auto descriptor = scene->get_descriptor();
    const auto* variant = Scenes::find_variant(descriptor, scene->get_variant_id());
    const auto profile = Scenes::effective_profile(descriptor, variant);

    float seconds = 27.0f;
    if (has_tag(profile.tags, "spotify-video")) {
        seconds = 42.0f;
        if (context.spotify_remaining_seconds > 0.0f)
            seconds = std::min(seconds, std::max(12.0f, context.spotify_remaining_seconds - 7.0f));
    }
    else if (has_tag(profile.tags, "album-art")) {
        seconds = context.spotify_progress < 0.12f ? 24.0f : 20.0f;
    }
    else if (has_tag(profile.tags, "director")) {
        seconds = 32.0f;
    }
    else if (context.audio_active && profile.music_affinity > 0.72f) {
        seconds = profile.intensity > 0.82f ? 18.0f : (profile.intensity < 0.48f ? 27.0f : 22.0f);
    }
    else if (has_any_tag(profile.tags, {"calm", "soft", "minimal"})) {
        seconds = 34.0f;
    }
    else if (has_any_tag(profile.tags, {"energetic", "dense", "vivid"})) {
        seconds = 21.0f;
    }
    else if (profile.motion < 0.42f) {
        seconds = 31.0f;
    }

    if (profile.performance_cost > context.performance_budget + 0.12f)
        seconds = std::min(seconds, 18.0f);

    return static_cast<tmillis_t>(std::clamp(seconds, 12.0f, 45.0f) * 1000.0f);
}

void AutomaticDirector::record_played(const std::shared_ptr<Scenes::Scene>& scene)
{
    if (!scene)
        return;
    const auto descriptor = scene->get_descriptor();
    const auto* variant = Scenes::find_variant(descriptor, scene->get_variant_id());
    const auto profile = Scenes::effective_profile(descriptor, variant);
    history_.push_back({scene->get_name(), descriptor.family, scene->get_variant_id(), presentation_role(profile)});
    while (history_.size() > 8)
        history_.pop_front();
}

void AutomaticDirector::report_render_quality(float quality_scale)
{
    render_quality_ = std::clamp(quality_scale, 0.0f, 1.0f);
}

void AutomaticDirector::reseed(std::uint64_t seed)
{
    if (seed == 0)
        seed = 1;
    seed_ = seed;
    rng_.seed(seed_);
    history_.clear();
    render_quality_ = 1.0f;
    last_scene_.clear();
    last_variant_.clear();
    last_score_ = 0.0f;
    last_reasons_.clear();
    last_ranked_.clear();
    decision_count_ = 0;
    last_audio_available_ = false;
    last_audio_active_ = false;
    last_spotify_available_ = false;
    last_spotify_mv_ready_ = false;
    last_loudness_ = 0.0f;
    last_bass_ = 0.0f;
    last_treble_ = 0.0f;
    last_onset_ = 0.0f;
    last_rhythmicity_ = 0.0f;
    last_brightness_ = 0.0f;
    last_tempo_trust_ = 0.0f;
    last_spotify_progress_ = 0.0f;
    last_spotify_remaining_seconds_ = 0.0f;
    last_target_intensity_ = 0.42f;
    last_target_motion_ = 0.46f;
    last_performance_budget_ = 0.88f;
    last_mode_ = "ambient";
    last_exclude_name_.clear();
}

nlohmann::json AutomaticDirector::diagnostics() const
{
    nlohmann::json history = nlohmann::json::array();
    for (const auto& entry : history_) {
        history.push_back({{"scene", entry.scene},
                           {"family", entry.family},
                           {"variant", entry.variant},
                           {"role", entry.role}});
    }
    nlohmann::json candidates = nlohmann::json::array();
    for (std::size_t i = 0; i < std::min<std::size_t>(10, last_ranked_.size()); ++i) {
        const auto& candidate = last_ranked_[i];
        candidates.push_back({{"scene", candidate.scene->get_name()},
                              {"variant", candidate.scene->get_variant_id()},
                              {"score", candidate.score},
                              {"reasons", candidate.reasons}});
    }
    return {{"seed", std::to_string(seed_)},
            {"decision_count", decision_count_},
            {"render_quality", render_quality_},
            {"context",
             {{"mode", last_mode_},
              {"audio_available", last_audio_available_},
              {"audio_active", last_audio_active_},
              {"spotify_available", last_spotify_available_},
              {"spotify_mv_ready", last_spotify_mv_ready_},
              {"spotify_progress", last_spotify_progress_},
              {"spotify_remaining_seconds", last_spotify_remaining_seconds_},
              {"loudness", last_loudness_},
              {"bass", last_bass_},
              {"treble", last_treble_},
              {"onset", last_onset_},
              {"rhythmicity", last_rhythmicity_},
              {"brightness", last_brightness_},
              {"tempo_trust", last_tempo_trust_},
              {"target_intensity", last_target_intensity_},
              {"target_motion", last_target_motion_},
              {"performance_budget", last_performance_budget_},
              {"excluded_scene", last_exclude_name_}}},
            {"last_scene", last_scene_},
            {"last_variant", last_variant_},
            {"last_score", last_score_},
            {"last_reasons", last_reasons_},
            {"history", std::move(history)},
            {"candidates", std::move(candidates)}};
}
