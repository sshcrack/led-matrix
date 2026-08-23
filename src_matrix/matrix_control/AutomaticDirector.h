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

    explicit AutomaticDirector(std::uint64_t seed = std::random_device{}());

    [[nodiscard]] std::vector<Candidate> rank(
        const std::vector<std::shared_ptr<Scenes::Scene>>& scenes,
        const RuntimeInputs::Snapshot& runtime_inputs,
        const std::string& exclude_name = "") const;

    Decision choose(
        const std::vector<std::shared_ptr<Scenes::Scene>>& scenes,
        const RuntimeInputs::Snapshot& runtime_inputs,
        const std::string& exclude_name = "");

    /// Automatic Mode owns presentation pacing as well as scene choice. Manual
    /// presets keep their configured durations; this is only consumed by the
    /// automatic coordinator path.
    [[nodiscard]] tmillis_t presentation_duration(
        const std::shared_ptr<Scenes::Scene>& scene,
        const RuntimeInputs::Snapshot& runtime_inputs) const;

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
    float last_loudness_ = 0.0f;
    float last_bass_ = 0.0f;
    float last_treble_ = 0.0f;
    float last_onset_ = 0.0f;
    float last_rhythmicity_ = 0.0f;
    float last_brightness_ = 0.0f;
    float last_tempo_trust_ = 0.0f;
    float last_spotify_progress_ = 0.0f;
    float last_spotify_remaining_seconds_ = 0.0f;
    float last_target_intensity_ = 0.42f;
    float last_target_motion_ = 0.46f;
    float last_performance_budget_ = 0.88f;
    std::string last_mode_ = "ambient";
    std::string last_exclude_name_;

    [[nodiscard]] float history_multiplier(
        const std::string& scene,
        const std::string& family,
        const std::string& role) const;
};
