// AutomaticDirector: candidate scoring, selection and dwell planning.
#include "AutomaticDirectorInternal.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <random>

#include "shared/matrix/input_ids.h"

using namespace AutomaticDirectorDetail;

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
    const auto context = music_context(runtime_inputs);
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
                // Music-first casting: while music plays, looks that visibly
                // react to it lead; ambient scenery only appears as a short
                // breather during calm passages after a run of music looks.
                if (has_tag(profile.tags, "music-showcase")) {
                    candidate.score += weight::music_showcase;
                    candidate.reasons.push_back("featured music visualizer");
                }
                if (context.style.confidence > 0.0f) {
                    const float fit = MusicStyle::style_fit(context.style, profile.tags);
                    candidate.score += weight::style_fit * context.style.confidence * (fit - 0.35f);
                    if (fit >= 0.45f)
                        candidate.reasons.push_back("suits " + std::string(MusicStyle::name(context.style.dominant))
                            + " music (" + context.style.source + ")");
                }
                if (!is_music_look(profile) && role != "media" && role != "director") {
                    const bool breather = context.target_intensity < 0.36f && music_streak() >= 3;
                    candidate.score -= breather ? weight::breather_penalty : weight::non_music_in_session;
                    candidate.reasons.push_back(breather ? "calm passage allows an ambient breather"
                                                         : "not music-reactive while music plays");
                }
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

    const auto context = music_context(runtime_inputs);
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
    else if (context.audio_active && has_tag(profile.tags, "music-showcase")) {
        // Featured visualizers get room to develop across several phrases.
        seconds = context.target_intensity < 0.40f ? showcase_dwell_calm_s
            : (context.target_intensity > 0.75f ? showcase_dwell_peak_s : showcase_dwell_s);
    }
    else if (context.audio_active && !is_music_look(profile) && presentation_role(profile) == "ambient") {
        seconds = breather_dwell_s;
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
