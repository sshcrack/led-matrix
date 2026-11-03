// Private helpers shared by the AutomaticDirector translation units.
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <initializer_list>

#include "AutomaticDirector.h"
#include "shared/matrix/scene_descriptor.h"

namespace AutomaticDirectorDetail {
bool has_tag(const std::vector<std::string>& tags, std::string_view wanted);
bool has_any_tag(const std::vector<std::string>& tags, std::initializer_list<std::string_view> wanted);
std::string presentation_role(const Scenes::EffectiveSceneProfile& profile);

using DirectorContext = AutomaticDirector::Context;

DirectorContext context_for(const RuntimeInputs::Snapshot& runtime_inputs, const VisualJourney::Frame& journey);
Scenes::EffectiveSceneProfile profile_of(const std::shared_ptr<Scenes::Scene>& scene);
float effective_cost(const std::shared_ptr<Scenes::Scene>& scene, const Scenes::EffectiveSceneProfile& profile,
                     const std::optional<float>& measured_load, bool desktop_available);

// Descriptor performance costs are priors on a 0..1 scale where anything above
// the budget is expected to strain the Pi.
inline constexpr float performance_budget = 0.90f;
// SceneRenderer starts desktop offload once a frame exceeds 72% of its budget,
// so a measured load of 0.72 maps onto the edge of the descriptor budget.
inline constexpr float measured_load_to_cost = performance_budget / 0.72f;
// Additive ranking weights. A candidate starts at 1.0; the fit terms dominate
// (up to ~2.3), context bonuses are 0.1-0.4 each, and Spotify lifecycle terms
// are deliberately larger because track timing is a hard presentation cue.
namespace weight {
inline constexpr float journey_motif_music = 0.12f;
inline constexpr float journey_motif_ambient = 0.32f;
inline constexpr float intensity_fit = 1.42f;
inline constexpr float motion_fit = 0.86f;
inline constexpr float showcase = 0.24f;
inline constexpr float music_affinity_live = 1.02f;
inline constexpr float audio_reactive_live = 0.22f;
inline constexpr float audio_reactive_shader_live = 0.30f;
inline constexpr float music_affinity_spotify_only = 0.42f;
inline constexpr float ambient_affinity_quiet = 0.42f;
inline constexpr float scenic_shader_quiet = 0.18f;
inline constexpr float album_art_base = 1.10f;
inline constexpr float album_art_repeat_penalty = 0.48f;
inline constexpr float album_art_track_intro = 0.78f;
inline constexpr float album_art_track_outro = 0.58f;
inline constexpr float spotify_video_base = 0.86f;
inline constexpr float spotify_video_mid_track = 1.02f;
inline constexpr float spotify_video_intro_penalty = 0.72f;
inline constexpr float spotify_video_late_penalty = 1.35f;
inline constexpr float spotify_video_ending_penalty = 1.20f;
inline constexpr float over_budget = 2.35f;
inline constexpr float under_budget = 0.20f;
inline constexpr float freshness = 0.28f;
inline constexpr float continuity = 0.18f;
inline constexpr float abrupt_energy_jump = 0.24f;
inline constexpr float follows_energy_trend = 0.16f;
inline constexpr float favorite = 0.40f;
}  // namespace weight

inline constexpr std::uint64_t freshness_delay_ms = 6 * 60000;
inline constexpr std::uint64_t freshness_ramp_ms = 24 * 60000;
inline constexpr std::uint64_t failure_cooldown_base_ms = 2 * 60000;
inline constexpr std::uint64_t failure_cooldown_max_ms = 30 * 60000;
}  // namespace AutomaticDirectorDetail
