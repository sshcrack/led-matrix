#pragma once

#include <cstdint>
#include <deque>
#include <memory>
#include <nlohmann/json.hpp>
#include <random>
#include <string>
#include <vector>

#include "shared/matrix/Scene.h"
#include "shared/matrix/runtime_inputs.h"

class AutomaticDirector {
public:
    struct Candidate {
        std::shared_ptr<Scenes::Scene> scene;
        float score = 0.0f;
        std::vector<std::string> reasons;
    };

    struct Decision {
        std::shared_ptr<Scenes::Scene> scene;
        float score = 0.0f;
        std::vector<std::string> reasons;
        std::vector<Candidate> ranked;
    };

    struct SwitchOpportunity {
        bool should_switch = false;
        std::string reason;
        std::shared_ptr<Scenes::Scene> preferred_scene;
        float current_score = 0.0f;
        float alternative_score = 0.0f;
    };

    explicit AutomaticDirector(std::uint64_t seed = std::random_device{}());

    [[nodiscard]] std::vector<Candidate> rank(
        const std::vector<std::shared_ptr<Scenes::Scene>>& scenes,
        const RuntimeInputs::Snapshot& runtime_inputs,
        const std::string& exclude_name = "") const;

    Decision choose(
        const std::vector<std::shared_ptr<Scenes::Scene>>& scenes,
        const RuntimeInputs::Snapshot& runtime_inputs,
        const std::string& exclude_name = "",
        const std::shared_ptr<Scenes::Scene>& preferred_scene = nullptr);

    /// Automatic Mode owns presentation pacing as well as scene choice. Manual
    /// presets keep their configured durations; this is only consumed by the
    /// automatic coordinator path.
    [[nodiscard]] tmillis_t presentation_duration(
        const std::shared_ptr<Scenes::Scene>& scene,
        const RuntimeInputs::Snapshot& runtime_inputs) const;

    /// Called periodically while a scene is visible. It turns durable runtime
    /// changes (track changes, prepared media, musical sections/energy shifts)
    /// into sparse switch opportunities while enforcing dwell/hysteresis.
    SwitchOpportunity consider_switch(
        const std::vector<std::shared_ptr<Scenes::Scene>>& scenes,
        const std::shared_ptr<Scenes::Scene>& current_scene,
        const RuntimeInputs::Snapshot& runtime_inputs,
        tmillis_t elapsed_ms);

    void record_played(const std::shared_ptr<Scenes::Scene>& scene);
    void report_render_quality(float quality_scale);
    void reseed(std::uint64_t seed);

    [[nodiscard]] std::uint64_t seed() const { return seed_; }
    [[nodiscard]] nlohmann::json diagnostics() const;

private:
    struct HistoryEntry {
        std::string scene;
        std::string family;
        std::string variant;
        std::string role;
        float intensity = 0.5f;
        float motion = 0.5f;
    };

    std::uint64_t seed_;
    mutable std::mt19937_64 rng_;
    std::deque<HistoryEntry> history_;
    float render_quality_ = 1.0f;
    std::string last_scene_;
    std::string last_variant_;
    float last_score_ = 0.0f;
    std::vector<std::string> last_reasons_;
    std::vector<Candidate> last_ranked_;
    std::uint64_t decision_count_ = 0;
    bool last_audio_available_ = false;
    bool last_audio_active_ = false;
    bool last_spotify_available_ = false;
    bool last_spotify_mv_ready_ = false;
    bool last_spotify_mv_tools_ready_ = false;
    bool last_spotify_mv_first_frame_ready_ = false;
    float last_loudness_ = 0.0f;
    float last_bass_ = 0.0f;
    float last_treble_ = 0.0f;
    float last_onset_ = 0.0f;
    float last_rhythmicity_ = 0.0f;
    float last_brightness_ = 0.0f;
    float last_tempo_trust_ = 0.0f;
    float last_spotify_progress_ = 0.0f;
    float last_spotify_remaining_seconds_ = 0.0f;
    std::string last_spotify_track_id_;
    std::string last_spotify_mv_track_id_;
    std::string last_spotify_mv_state_;
    float last_target_intensity_ = 0.42f;
    float last_target_motion_ = 0.46f;
    float last_performance_budget_ = 0.88f;
    std::string last_mode_ = "ambient";
    std::string last_exclude_name_;
    std::string current_track_id_;
    bool current_track_cover_shown_ = false;
    bool current_track_mv_shown_ = false;
    bool track_switch_pending_ = false;
    bool switch_events_primed_ = false;
    std::uint64_t seen_drop_ = 0;
    std::uint64_t seen_section_ = 0;
    std::string last_switch_reason_;

    [[nodiscard]] float history_multiplier(
        const std::string& scene,
        const std::string& family,
        const std::string& role) const;
    bool sync_track_context(const std::string& track_id);
};
