#pragma once

#include <cstdint>
#include <deque>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

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
        const std::vector<std::shared_ptr<Scenes::Scene>> &scenes,
        const RuntimeInputs::Snapshot &runtime_inputs,
        const std::string &exclude_name = "") const;

    Decision choose(
        const std::vector<std::shared_ptr<Scenes::Scene>> &scenes,
        const RuntimeInputs::Snapshot &runtime_inputs,
        const std::string &exclude_name = "");

    void record_played(const std::shared_ptr<Scenes::Scene> &scene);
    void report_render_quality(float quality_scale);

    [[nodiscard]] std::uint64_t seed() const { return seed_; }
    [[nodiscard]] nlohmann::json diagnostics() const;

private:
    struct HistoryEntry {
        std::string scene;
        std::string family;
        std::string variant;
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

    [[nodiscard]] float history_multiplier(
        const std::string &scene, const std::string &family) const;
};
