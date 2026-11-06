#include "AutomaticDirectorInternal.h"

#include <algorithm>

using namespace AutomaticDirectorDetail;

AutomaticDirector::AutomaticDirector(std::uint64_t seed) : journey_(seed), seed_(seed), rng_(seed) {}

std::string AutomaticDirector::look_key(const Scenes::Scene& scene)
{
    const auto& variant = scene.get_variant_id();
    return variant.empty() ? scene.get_name() : scene.get_name() + "/" + variant;
}

void AutomaticDirector::set_preferences(std::map<std::string, std::string> preferences)
{
    preferences_ = std::move(preferences);
}

bool AutomaticDirector::cooling_down(const std::string& scene) const
{
    const auto it = memory_.find(scene);
    return it != memory_.end() && it->second.cooldown_until_ms > clock_ms_;
}

bool AutomaticDirector::sync_track_context(const std::string& track_id)
{
    if (!track_intro_.sync(track_id))
        return false;
    // Counters from the previous track must not fire on the new one.
    drop_edge_.reset();
    section_edge_.reset();
    return true;
}

void AutomaticDirector::advance_journey(tmillis_t elapsed_ms)
{
    if (elapsed_ms <= 0)
        return;
    journey_.advance(static_cast<std::uint64_t>(elapsed_ms));
    clock_ms_ += static_cast<std::uint64_t>(elapsed_ms);
}

void AutomaticDirector::record_played(const std::shared_ptr<Scenes::Scene>& scene)
{
    if (!scene)
        return;
    const auto descriptor = scene->get_descriptor();
    const auto* variant = Scenes::find_variant(descriptor, scene->get_variant_id());
    const auto profile = Scenes::effective_profile(descriptor, variant);
    history_.push_back({scene->get_name(), descriptor.family, scene->get_variant_id(), presentation_role(profile),
                        profile.intensity, profile.motion, is_music_look(profile)});
    while (history_.size() > 8)
        history_.pop_front();
    auto& memory = memory_[scene->get_name()];
    memory.last_shown_ms = clock_ms_;
    ++memory.presentations;

    if (!track_intro_.track_id.empty()) {
        if (has_tag(profile.tags, "album-art"))
            track_intro_.mark_cover_shown();
        if (has_tag(profile.tags, "spotify-video"))
            track_intro_.music_video_shown = true;
    }
}

void AutomaticDirector::report_outcome(const std::shared_ptr<Scenes::Scene>& scene, const PresentationOutcome& outcome)
{
    if (!scene)
        return;
    auto& memory = memory_[scene->get_name()];
    if (outcome.render_load.has_value()) {
        const float load = std::max(0.0f, *outcome.render_load);
        memory.render_load = memory.render_load.has_value() ? *memory.render_load * 0.65f + load * 0.35f : load;
    }
    if (!outcome.failed) {
        memory.failures = 0;
        return;
    }
    ++memory.failures;
    const auto shift = std::min<std::uint32_t>(memory.failures - 1, 4);
    memory.cooldown_until_ms = clock_ms_ + std::min(failure_cooldown_base_ms << shift, failure_cooldown_max_ms);
}

void AutomaticDirector::reseed(std::uint64_t seed)
{
    if (seed == 0)
        seed = 1;
    journey_ = VisualJourney(seed);
    seed_ = seed;
    rng_.seed(seed_);
    history_.clear();
    for (auto& [_, memory] : memory_) {
        memory.last_shown_ms.reset();
        memory.presentations = 0;
    }
    last_context_ = {};
    last_scene_.clear();
    last_variant_.clear();
    last_score_ = 0.0f;
    last_reasons_.clear();
    last_ranked_.clear();
    decision_count_ = 0;
    last_exclude_name_.clear();
    track_intro_ = {};
    drop_edge_.reset();
    section_edge_.reset();
    next_fit_check_ms_ = 0;
    last_switch_reason_.clear();
    phrase_primed_ = false;
    mismatch_scene_.clear();
    style_scene_.clear();
    style_at_start_ = MusicStyle::Style::Chill;
    fit_error_at_start_ = 0.0f;
    mismatch_since_ms_ = 0;
}
