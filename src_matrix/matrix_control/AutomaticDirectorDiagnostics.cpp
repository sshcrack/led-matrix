// AutomaticDirector: JSON diagnostics serialization.
#include "AutomaticDirectorInternal.h"

#include <algorithm>

using namespace AutomaticDirectorDetail;

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
              {"music_style",
               [&] {
                   nlohmann::json weights = nlohmann::json::object();
                   for (int i = 0; i < MusicStyle::StyleCount; ++i)
                       weights[std::string(MusicStyle::name(static_cast<MusicStyle::Style>(i)))] =
                           c.style.weights[static_cast<std::size_t>(i)];
                   return nlohmann::json{{"dominant", c.style.confidence > 0.0f ? std::string(MusicStyle::name(c.style.dominant)) : ""},
                                         {"confidence", c.style.confidence},
                                         {"source", c.style.source},
                                         {"genres", c.style.genres},
                                         {"weights", weights}};
               }()},
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
