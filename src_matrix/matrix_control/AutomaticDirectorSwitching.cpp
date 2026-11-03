// AutomaticDirector: mid-scene switch opportunity evaluation.
#include "AutomaticDirectorInternal.h"

#include <algorithm>
#include <cmath>
#include <optional>

using namespace AutomaticDirectorDetail;

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
