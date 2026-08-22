#include "AutomaticDirector.h"

#include <algorithm>
#include <cmath>
#include <numeric>

#include "shared/matrix/input_ids.h"
#include "shared/matrix/scene_descriptor.h"

namespace {
float signal_number(const RuntimeInputs::Snapshot &snapshot, std::string_view input,
                    std::string_view signal, float fallback)
{
    const auto value = snapshot.number(input, signal);
    return value.has_value() ? static_cast<float>(*value) : fallback;
}

bool has_tag(const std::vector<std::string> &tags, std::string_view wanted)
{
    return std::find(tags.begin(), tags.end(), wanted) != tags.end();
}
}

AutomaticDirector::AutomaticDirector(std::uint64_t seed)
    : seed_(seed), rng_(seed)
{}

float AutomaticDirector::history_multiplier(
    const std::string &scene, const std::string &family) const
{
    float multiplier = 1.0f;
    for (std::size_t index = 0; index < history_.size(); ++index) {
        const auto &entry = history_[history_.size() - 1 - index];
        const float recency = index == 0 ? 0.18f : (index == 1 ? 0.38f : 0.62f);
        if (entry.scene == scene)
            multiplier = std::min(multiplier, recency);
        else if (!family.empty() && entry.family == family)
            multiplier = std::min(multiplier, 0.72f + 0.08f * static_cast<float>(std::min<std::size_t>(index, 2)));
    }
    return multiplier;
}

std::vector<AutomaticDirector::Candidate> AutomaticDirector::rank(
    const std::vector<std::shared_ptr<Scenes::Scene>> &scenes,
    const RuntimeInputs::Snapshot &runtime_inputs,
    const std::string &exclude_name) const
{
    std::vector<Candidate> ranked;
    const bool audio = runtime_inputs.available(RuntimeInputIds::Audio);
    const bool spotify = runtime_inputs.available(RuntimeInputIds::SpotifyPlayback);
    const float loudness = std::clamp(signal_number(runtime_inputs, RuntimeInputIds::Audio, "loudness", 0.35f), 0.0f, 1.0f);
    const float target_intensity = audio ? std::clamp(0.28f + loudness * 0.72f, 0.2f, 1.0f) : 0.42f;
    const float performance_budget = render_quality_ < 0.82f ? 0.48f : (render_quality_ < 0.94f ? 0.68f : 0.88f);

    for (const auto &scene : scenes) {
        if (!scene || scene->get_name() == exclude_name)
            continue;
        const auto descriptor = scene->get_descriptor();
        if (!descriptor.automatic_eligible)
            continue;
        if (!RuntimeInputs::satisfies(scene->get_effective_runtime_inputs(), runtime_inputs))
            continue;

        const auto *variant = Scenes::find_variant(descriptor, scene->get_variant_id());
        const auto profile = Scenes::effective_profile(descriptor, variant);
        Candidate candidate;
        candidate.scene = scene;
        candidate.score = 1.0f;

        const float intensity_fit = 1.0f - std::abs(profile.intensity - target_intensity);
        candidate.score += intensity_fit * 1.35f;
        candidate.reasons.push_back(intensity_fit > 0.72f ? "intensity fits current context" : "intensity is usable");

        if (audio) {
            candidate.score += profile.music_affinity * 1.25f;
            if (profile.music_affinity > 0.7f)
                candidate.reasons.push_back("matches live audio");
        } else {
            candidate.score += (1.0f - profile.music_affinity) * 0.35f;
        }

        if (spotify && (has_tag(profile.tags, "album-art") || has_tag(profile.tags, "spotify"))) {
            candidate.score += 0.9f;
            candidate.reasons.push_back("Spotify playback is available");
        }

        if (profile.performance_cost > performance_budget) {
            const float excess = profile.performance_cost - performance_budget;
            candidate.score -= excess * 2.2f;
            candidate.reasons.push_back("deprioritized for Pi render headroom");
        } else {
            candidate.score += 0.18f * (performance_budget - profile.performance_cost);
        }

        const float history = history_multiplier(scene->get_name(), descriptor.family);
        candidate.score *= history;
        if (history < 0.9f)
            candidate.reasons.push_back("recent-history repetition penalty");

        ranked.push_back(std::move(candidate));
    }

    std::stable_sort(ranked.begin(), ranked.end(), [](const Candidate &a, const Candidate &b) {
        if (std::abs(a.score - b.score) > 0.0001f) return a.score > b.score;
        const auto a_key = a.scene->get_name() + ":" + a.scene->get_variant_id();
        const auto b_key = b.scene->get_name() + ":" + b.scene->get_variant_id();
        return a_key < b_key;
    });
    return ranked;
}

AutomaticDirector::Decision AutomaticDirector::choose(
    const std::vector<std::shared_ptr<Scenes::Scene>> &scenes,
    const RuntimeInputs::Snapshot &runtime_inputs,
    const std::string &exclude_name)
{
    Decision decision;
    decision.ranked = rank(scenes, runtime_inputs, exclude_name);
    if (decision.ranked.empty()) {
        last_scene_.clear(); last_variant_.clear(); last_score_ = 0.0f;
        last_reasons_ = {"no eligible automatic scene"}; last_ranked_.clear();
        return decision;
    }

    const std::size_t pool_size = std::min<std::size_t>(5, decision.ranked.size());
    const float floor = decision.ranked[pool_size - 1].score;
    std::vector<double> weights(pool_size);
    for (std::size_t i = 0; i < pool_size; ++i)
        weights[i] = std::max(0.05, static_cast<double>(decision.ranked[i].score - floor + 0.18f));
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

void AutomaticDirector::record_played(const std::shared_ptr<Scenes::Scene> &scene)
{
    if (!scene) return;
    history_.push_back({scene->get_name(), scene->get_descriptor().family, scene->get_variant_id()});
    while (history_.size() > 6) history_.pop_front();
}

void AutomaticDirector::report_render_quality(float quality_scale)
{
    render_quality_ = std::clamp(quality_scale, 0.0f, 1.0f);
}

nlohmann::json AutomaticDirector::diagnostics() const
{
    nlohmann::json history = nlohmann::json::array();
    for (const auto &entry : history_)
        history.push_back({{"scene", entry.scene}, {"family", entry.family}, {"variant", entry.variant}});
    nlohmann::json candidates = nlohmann::json::array();
    for (std::size_t i = 0; i < std::min<std::size_t>(8, last_ranked_.size()); ++i) {
        const auto &candidate = last_ranked_[i];
        candidates.push_back({
            {"scene", candidate.scene->get_name()},
            {"variant", candidate.scene->get_variant_id()},
            {"score", candidate.score},
            {"reasons", candidate.reasons}
        });
    }
    return {
        {"seed", seed_},
        {"render_quality", render_quality_},
        {"last_scene", last_scene_},
        {"last_variant", last_variant_},
        {"last_score", last_score_},
        {"last_reasons", last_reasons_},
        {"history", std::move(history)},
        {"candidates", std::move(candidates)}
    };
}
