#include "AutomaticDirector.h"

#include <algorithm>
#include <cmath>
#include <numeric>

#include "shared/matrix/input_ids.h"
#include "shared/matrix/scene_descriptor.h"

namespace {
float signal_number(const RuntimeInputs::Snapshot& snapshot, std::string_view input, std::string_view signal, float fallback)
{
    const auto value = snapshot.number(input, signal);
    return value.has_value() ? static_cast<float>(*value) : fallback;
}

bool has_tag(const std::vector<std::string>& tags, std::string_view wanted)
{
    return std::find(tags.begin(), tags.end(), wanted) != tags.end();
}

bool signal_bool(const RuntimeInputs::Snapshot& snapshot, std::string_view input, std::string_view signal, bool fallback)
{
    const auto value = snapshot.boolean(input, signal);
    return value.has_value() ? *value : fallback;
}

struct DirectorContext {
    bool audio_available = false;
    bool audio_active = false;
    bool spotify = false;
    float loudness = 0.35f;
    float bass = 0.0f;
    float treble = 0.0f;
    float onset = 0.0f;
    float hihat = 0.0f;
    float energy_trend = 0.0f;
    float tempo_trust = 0.0f;
    float target_intensity = 0.42f;
    float target_motion = 0.46f;
    float performance_budget = 0.88f;
};

DirectorContext context_for(const RuntimeInputs::Snapshot& runtime_inputs, float render_quality)
{
    DirectorContext c;
    c.audio_available = runtime_inputs.available(RuntimeInputIds::Audio);
    c.spotify = runtime_inputs.available(RuntimeInputIds::SpotifyPlayback);
    const bool silence = c.audio_available && signal_bool(runtime_inputs, RuntimeInputIds::Audio, "silence", false);
    c.audio_active = c.audio_available && !silence;
    const float fallback_loudness = std::clamp(signal_number(runtime_inputs, RuntimeInputIds::Audio, "loudness", 0.35f), 0.0f, 1.0f);
    c.loudness = std::clamp(signal_number(runtime_inputs, RuntimeInputIds::Audio, "loudness_fast", fallback_loudness), 0.0f, 1.0f);
    c.bass = std::clamp(0.58f * signal_number(runtime_inputs, RuntimeInputIds::Audio, "bass", 0.0f) +
                            0.42f * signal_number(runtime_inputs, RuntimeInputIds::Audio, "sub_bass", 0.0f),
                        0.0f, 1.0f);
    c.treble = std::clamp(signal_number(runtime_inputs, RuntimeInputIds::Audio, "treble", 0.0f), 0.0f, 1.0f);
    c.hihat = std::clamp(signal_number(runtime_inputs, RuntimeInputIds::Audio, "hihat", 0.0f), 0.0f, 1.0f);
    c.onset = std::clamp(signal_number(runtime_inputs, RuntimeInputIds::Audio, "onset_strength", 0.0f), 0.0f, 1.0f);
    c.energy_trend = std::clamp(signal_number(runtime_inputs, RuntimeInputIds::Audio, "energy_trend", 0.0f), -1.0f, 1.0f);
    const float confidence = std::clamp(signal_number(runtime_inputs, RuntimeInputIds::Audio, "beat_confidence", 0.0f), 0.0f, 1.0f);
    const float stability = std::clamp(signal_number(runtime_inputs, RuntimeInputIds::Audio, "tempo_stability", 0.0f), 0.0f, 1.0f);
    c.tempo_trust = std::clamp((confidence - 0.30f) / 0.50f, 0.0f, 1.0f) * stability;

    if (c.audio_active) {
        const float energy = std::clamp(
            0.50f * c.loudness + 0.22f * c.bass + 0.10f * c.onset + 0.06f * c.hihat + 0.12f * std::max(0.0f, c.energy_trend), 0.0f, 1.0f);
        c.target_intensity = std::clamp(0.20f + energy * 0.76f, 0.20f, 0.96f);
        c.target_motion = std::clamp(0.24f + c.loudness * 0.24f + c.onset * 0.18f + c.hihat * 0.12f + c.tempo_trust * 0.20f, 0.22f, 0.94f);
    }
    else {
        c.target_intensity = silence ? 0.24f : 0.42f;
        c.target_motion = silence ? 0.28f : 0.46f;
    }
    c.performance_budget = render_quality < 0.82f ? 0.48f : (render_quality < 0.94f ? 0.68f : 0.88f);
    return c;
}
}  // namespace

AutomaticDirector::AutomaticDirector(std::uint64_t seed) : seed_(seed), rng_(seed) {}

float AutomaticDirector::history_multiplier(const std::string& scene, const std::string& family) const
{
    float multiplier = 1.0f;
    for (std::size_t index = 0; index < history_.size(); ++index) {
        const auto& entry = history_[history_.size() - 1 - index];
        const float recency = index == 0 ? 0.18f : (index == 1 ? 0.38f : 0.62f);
        if (entry.scene == scene)
            multiplier = std::min(multiplier, recency);
        else if (!family.empty() && entry.family == family)
            multiplier = std::min(multiplier, 0.72f + 0.08f * static_cast<float>(std::min<std::size_t>(index, 2)));
    }
    return multiplier;
}

std::vector<AutomaticDirector::Candidate> AutomaticDirector::rank(const std::vector<std::shared_ptr<Scenes::Scene>>& scenes,
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
        Candidate candidate;
        candidate.scene = scene;
        candidate.score = 1.0f;

        const float intensity_fit = 1.0f - std::abs(profile.intensity - context.target_intensity);
        const float motion_fit = 1.0f - std::abs(profile.motion - context.target_motion);
        candidate.score += intensity_fit * 1.45f + motion_fit * 0.82f;
        candidate.reasons.push_back(intensity_fit > 0.78f ? "energy fits current context" : "energy is usable");
        if (motion_fit > 0.80f)
            candidate.reasons.push_back("motion fits current groove");

        if (context.audio_active) {
            candidate.score += profile.music_affinity * 1.28f;
            if (profile.music_affinity > 0.7f)
                candidate.reasons.push_back("matches live music");
            if (has_tag(profile.tags, "director")) {
                candidate.score += 1.35f;
                candidate.reasons.push_back("adapts continuously to music");
            }
            if ((has_tag(profile.tags, "depth") || has_tag(profile.tags, "tunnel")) && context.bass > 0.42f)
                candidate.score += context.bass * 0.24f;
            if (has_tag(profile.tags, "particles") && (context.onset > 0.24f || context.hihat > 0.28f))
                candidate.score += context.onset * 0.14f + context.hihat * 0.16f;
            if ((has_tag(profile.tags, "geometric") || has_tag(profile.tags, "symmetry")) && context.tempo_trust > 0.45f)
                candidate.score += context.tempo_trust * 0.16f;
            if (context.target_intensity < 0.36f &&
                (has_tag(profile.tags, "calm") || has_tag(profile.tags, "soft") || has_tag(profile.tags, "minimal")))
                candidate.score += 0.24f;
            if (context.target_intensity > 0.74f &&
                (has_tag(profile.tags, "energetic") || has_tag(profile.tags, "vivid") || has_tag(profile.tags, "dense")))
                candidate.score += 0.26f;
        }
        else {
            candidate.score += (1.0f - profile.music_affinity) * 0.35f;
            if (context.audio_available && context.target_intensity < 0.30f && profile.intensity > 0.70f)
                candidate.score -= 0.45f;
        }

        if (context.spotify && (has_tag(profile.tags, "album-art") || has_tag(profile.tags, "spotify"))) {
            candidate.score += 0.9f;
            candidate.reasons.push_back("Spotify playback is available");
        }

        if (profile.performance_cost > context.performance_budget) {
            const float excess = profile.performance_cost - context.performance_budget;
            candidate.score -= excess * 2.2f;
            candidate.reasons.push_back("deprioritized for Pi render headroom");
        }
        else {
            candidate.score += 0.18f * (context.performance_budget - profile.performance_cost);
        }

        const float history = history_multiplier(scene->get_name(), descriptor.family);
        candidate.score *= history;
        if (history < 0.9f)
            candidate.reasons.push_back("recent-history repetition penalty");

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

AutomaticDirector::Decision AutomaticDirector::choose(const std::vector<std::shared_ptr<Scenes::Scene>>& scenes,
                                                      const RuntimeInputs::Snapshot& runtime_inputs, const std::string& exclude_name)
{
    Decision decision;
    const auto context = context_for(runtime_inputs, render_quality_);
    last_audio_available_ = context.audio_available;
    last_audio_active_ = context.audio_active;
    last_spotify_available_ = context.spotify;
    last_loudness_ = context.loudness;
    last_bass_ = context.bass;
    last_treble_ = context.treble;
    last_onset_ = context.onset;
    last_tempo_trust_ = context.tempo_trust;
    last_target_intensity_ = context.target_intensity;
    last_target_motion_ = context.target_motion;
    last_performance_budget_ = context.performance_budget;
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

    const std::size_t pool_size = std::min<std::size_t>(4, decision.ranked.size());
    const float best_score = decision.ranked.front().score;
    std::vector<double> weights(pool_size);
    // Softmax keeps seeded variety when several looks are genuinely close, but
    // stops a clearly context-aware winner from losing to a mediocre candidate
    // merely because it happened to be in the top five.
    constexpr double selection_temperature = 0.34;
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

void AutomaticDirector::record_played(const std::shared_ptr<Scenes::Scene>& scene)
{
    if (!scene)
        return;
    history_.push_back({scene->get_name(), scene->get_descriptor().family, scene->get_variant_id()});
    while (history_.size() > 6) history_.pop_front();
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
    last_loudness_ = 0.0f;
    last_bass_ = 0.0f;
    last_treble_ = 0.0f;
    last_onset_ = 0.0f;
    last_tempo_trust_ = 0.0f;
    last_target_intensity_ = 0.42f;
    last_target_motion_ = 0.46f;
    last_performance_budget_ = 0.88f;
    last_exclude_name_.clear();
}

nlohmann::json AutomaticDirector::diagnostics() const
{
    nlohmann::json history = nlohmann::json::array();
    for (const auto& entry : history_) history.push_back({{"scene", entry.scene}, {"family", entry.family}, {"variant", entry.variant}});
    nlohmann::json candidates = nlohmann::json::array();
    for (std::size_t i = 0; i < std::min<std::size_t>(8, last_ranked_.size()); ++i) {
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
             {{"audio_available", last_audio_available_},
              {"audio_active", last_audio_active_},
              {"spotify_available", last_spotify_available_},
              {"loudness", last_loudness_},
              {"bass", last_bass_},
              {"treble", last_treble_},
              {"onset", last_onset_},
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
