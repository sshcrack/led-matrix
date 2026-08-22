#include "TransitionPlanner.h"

#include <algorithm>
#include <cmath>

#include "shared/matrix/scene_descriptor.h"

namespace {
float feature(const AudioState::Snapshot &audio, AudioProtocol::Feature id)
{
    return std::clamp(audio.feature(id), 0.0f, 1.0f);
}

Scenes::EffectiveSceneProfile profile(const std::shared_ptr<Scenes::Scene> &scene)
{
    if (!scene) return {};
    const auto descriptor = scene->get_descriptor();
    return Scenes::effective_profile(
        descriptor, Scenes::find_variant(descriptor, scene->get_variant_id()));
}
}

TransitionPlan TransitionPlanner::plan(
    const std::shared_ptr<Scenes::Scene> &from,
    const std::shared_ptr<Scenes::Scene> &to,
    tmillis_t fallback_duration,
    const std::string &fallback_name,
    const AudioState::Snapshot &audio) const
{
    TransitionPlan result;
    result.duration_ms = std::max<tmillis_t>(1, fallback_duration);
    result.name = fallback_name.empty() ? "blend" : fallback_name;
    result.reason = "configured fallback";
    if (!from || !to) return result;

    const auto from_descriptor = from->get_descriptor();
    const auto to_descriptor = to->get_descriptor();
    const auto a = profile(from);
    const auto b = profile(to);
    const float intensity_delta = std::abs(a.intensity - b.intensity);
    const float motion = 0.5f * (a.motion + b.motion);

    if (from_descriptor.family == to_descriptor.family) {
        result.name = "morph";
        result.reason = "same visual family";
    } else if (motion > 0.72f) {
        result.name = "zoom_blend";
        result.reason = "high-motion handoff";
    } else if (intensity_delta < 0.22f) {
        result.name = "ordered_dissolve";
        result.reason = "similar visual energy";
    } else {
        result.name = "radial_reveal";
        result.reason = "energy-state change";
    }

    if (!audio.fresh()) {
        result.duration_ms = std::clamp<tmillis_t>(result.duration_ms, 300, 1200);
        return result;
    }

    const float confidence = feature(audio, AudioProtocol::Feature::BeatConfidence);
    const float stability = feature(audio, AudioProtocol::Feature::TempoStability);
    const float bpm = audio.feature(AudioProtocol::Feature::Bpm);
    const bool reliable_tempo = confidence >= 0.52f && stability >= 0.42f && bpm >= 55.0f && bpm <= 220.0f;
    const bool drop = feature(audio, AudioProtocol::Feature::Drop) >= 0.45f || audio.event(AudioProtocol::DropEvent);

    if (drop && b.intensity >= a.intensity) {
        result.name = "glitch_cut";
        result.duration_ms = 260;
        result.start_delay_ms = 0;
        result.reason = "drop accent into higher energy";
        return result;
    }

    if (!reliable_tempo) {
        result.duration_ms = std::clamp<tmillis_t>(result.duration_ms, 300, 1200);
        result.reason += "; tempo not reliable";
        return result;
    }

    const float beat_ms = 60000.0f / bpm;
    const float phase = feature(audio, AudioProtocol::Feature::BeatPhase);
    float until_beat = (1.0f - phase) * beat_ms;
    if (until_beat < 45.0f || until_beat > 700.0f) until_beat = 0.0f;
    result.start_delay_ms = static_cast<tmillis_t>(std::lround(until_beat));
    result.beat_synchronized = true;

    const float beat_count = (intensity_delta > 0.34f || motion > 0.78f) ? 1.0f : 2.0f;
    result.duration_ms = std::clamp<tmillis_t>(
        static_cast<tmillis_t>(std::lround(beat_ms * beat_count)), 280, 1600);
    result.reason += "; beat-aligned";
    return result;
}
