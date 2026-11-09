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

    const bool drop_event = drop_edge_.observe(context.drop_counter);
    const bool section_event = section_edge_.observe(context.section_counter);

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
    // Outside and Chill keep their long scenery dwell: a new track does not cut.
    if (music_cuts_allowed() && (track_changed || track_intro_.awaiting_cover()) && context.spotify && elapsed_ms >= 1500) {
        if (current_is_cover) {
            track_intro_.mark_cover_shown();
        } else if (auto cover = best_with_tag("album-art")) {
            request("Spotify track changed; introduce the new track visually", std::move(cover));
            return result;
        } else {
            // No cover look is available (hidden, cooling down or plugin
            // disabled): cutting without one would just reshuffle every tick.
            track_intro_.skip_cover();
        }
    }

    // Once Automatic Mode deliberately enters the prepared music video, let it
    // read as a feature segment instead of immediately reacting to the next
    // ordinary section/energy change. Track changes above may still cut it short.
    if (current_is_spotify_video && elapsed_ms < 26000 && elapsed_ms < planned_ms)
        return result;

    if (music_cuts_allowed() && context.spotify_mv_ready && !track_intro_.music_video_shown
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
        const bool wait_for_phrase = music_led(context) && presentation_role(current_profile) != "media";
        if (!wait_for_phrase || phrase_boundary || elapsed_ms >= planned_ms + phrase_grace_ms)
            request(phrase_boundary && wait_for_phrase ? "scene completed on a musical phrase boundary"
                                                       : "planned scene dwell completed");
        return result;
    }

    if (music_led(context)) {
        // ---- live-music session --------------------------------------------
        const bool current_music = is_music_look(current_profile);
        const bool showcase = has_tag(current_profile.tags, "music-showcase");
        const std::uint64_t min_dwell = !current_music ? 4000 : (showcase ? showcase_min_dwell_ms : music_min_dwell_ms);
        const auto best_music = [&](float min_intensity = -1.0f) -> const Candidate* {
            for (const auto& candidate : ranked_alternatives()) {
                const auto profile = profile_of(candidate.scene);
                if (is_music_look(profile) && profile.intensity > min_intensity)
                    return &candidate;
            }
            return nullptr;
        };
        const auto better_music_fit = [&](float margin) -> const Candidate* {
            const auto& ranked = ranked_alternatives();
            for (std::size_t i = 0; i < std::min<std::size_t>(5, ranked.size()); ++i) {
                const auto profile = profile_of(ranked[i].scene);
                if (is_music_look(profile) && fit_error(profile) + margin < current_error)
                    return &ranked[i];
            }
            return nullptr;
        };

        if (drop_event && elapsed_ms >= 3000) {
            // Covers and music videos follow the track, not its drops.
            if (!current_music && presentation_role(current_profile) != "media") {
                if (const auto* candidate = best_music()) {
                    request("music drop: bring a music-reactive look back on screen", candidate->scene);
                    return result;
                }
            }
            // Drop-reactive looks get to play the drop they were built for;
            // only late in their dwell does the drop become the handoff point.
            const bool plays_drop = has_tag(current_profile.tags, "drop-reactive")
                && elapsed_ms < static_cast<tmillis_t>(planned_ms * 0.6);
            if (current_music && !plays_drop && elapsed_ms >= std::max<tmillis_t>(8000, static_cast<tmillis_t>(min_dwell))) {
                if (const auto* candidate = best_music(current_profile.intensity + 0.10f)) {
                    request("music drop supports a higher-energy visual", candidate->scene);
                    return result;
                }
            }
        }

        // Ambient breathers end as soon as the music picks up again.
        if (!current_music && presentation_role(current_profile) != "media" && elapsed_ms >= 6000
            && context.target_intensity >= 0.40f) {
            if (const auto* candidate = best_music()) {
                request("music picked up; return to a music-reactive look", candidate->scene);
                return result;
            }
        }

        // A new track can change the style entirely (chill -> rock). Only
        // metadata-backed changes since this look started count; the audio
        // estimate is a soft nudge for casting, never a reason to cut.
        if (style_scene_ != look_key(*current_scene)) {
            style_scene_ = look_key(*current_scene);
            style_at_start_ = context.style.dominant;
            fit_error_at_start_ = current_error;
        }
        if (current_music && context.style.source.rfind("genre", 0) == 0 && context.style.dominant != style_at_start_
            && elapsed_ms >= 10000 && MusicStyle::style_fit(context.style, current_profile.tags) < 0.20f) {
            const auto& ranked = ranked_alternatives();
            for (std::size_t i = 0; i < std::min<std::size_t>(5, ranked.size()); ++i) {
                const auto profile = profile_of(ranked[i].scene);
                if (is_music_look(profile) && MusicStyle::style_fit(context.style, profile.tags) >= 0.50f) {
                    request("music style changed to " + std::string(MusicStyle::name(context.style.dominant)),
                            ranked[i].scene);
                    return result;
                }
            }
        }

        // Energy mismatch must persist (phrase-level, not one loud bar) and the
        // look must have had its minimum dwell.
        // "Moved away" is relative to how well the look fit when it was cast:
        // the ranking deliberately trades raw energy fit for showcase/style,
        // so an absolute threshold would cut its own picks.
        const std::string current_key = look_key(*current_scene);
        // A look that is far off the energy is always replaceable, even when it
        // was a showcase/style trade-off from the start.
        const bool drifted = current_error > std::max(0.30f, fit_error_at_start_ + 0.12f) || current_error > 0.38f;
        if (drifted) {
            if (mismatch_scene_ != current_key) {
                mismatch_scene_ = current_key;
                mismatch_since_ms_ = clock_ms_;
            }
        } else if (mismatch_scene_ == current_key) {
            mismatch_scene_.clear();
        }
        const bool sustained_mismatch = mismatch_scene_ == current_key && clock_ms_ - mismatch_since_ms_ >= mismatch_hold_ms;
        if (elapsed_ms >= min_dwell && (sustained_mismatch || (section_event && drifted))) {
            if (const auto* candidate = better_music_fit(current_error > 0.38f ? 0.08f : 0.18f)) {
                request(section_event ? "musical section change moved the energy away from this scene"
                                      : "sustained music energy no longer fits the current scene", candidate->scene);
                return result;
            }
        }
        return result;
    }

    // ---- no live music: ambient journey ------------------------------------
    // Sections change every 15-30 s in most music, so a section alone is not a
    // reason to cut mid-scene. It is only used when the current look clearly
    // stopped fitting; otherwise sections just align the planned handoff above.
    if (elapsed_ms >= 12000 && current_error > 0.25f && clock_ms_ >= next_fit_check_ms_) {
        next_fit_check_ms_ = clock_ms_ + 2000;
        if (const auto* candidate = better_fit(0.16f)) {
            request(context.spotify ? "Spotify playback energy no longer fits the current scene"
                                    : "visual journey has moved beyond the current scene", candidate->scene);
            return result;
        }
    }

    return result;
}
