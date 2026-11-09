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
/// A look that visibly reacts to the music (as opposed to ambient scenery).
bool is_music_look(const Scenes::EffectiveSceneProfile& profile);

using DirectorContext = AutomaticDirector::Context;

DirectorContext context_for(const RuntimeInputs::Snapshot& runtime_inputs, const VisualJourney::Frame& journey);
Scenes::EffectiveSceneProfile profile_of(const std::shared_ptr<Scenes::Scene>& scene);
float effective_cost(const std::shared_ptr<Scenes::Scene>& scene, const Scenes::EffectiveSceneProfile& profile,
                     const std::optional<float>& measured_load);

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
// Live-music session casting.
inline constexpr float music_showcase = 0.55f;       // "music-showcase" tier
inline constexpr float style_fit = 2.00f;            // * confidence * (fit - neutral)
inline constexpr float non_music_in_session = 1.60f; // ambient scenery while music plays
inline constexpr float breather_penalty = 0.30f;     // ...unless a calm passage invites a breather
// Director modes and the real-world moment (see environment_fit()).
inline constexpr float environment_fit_mixed = 0.35f;  // * fit (-1..1)
inline constexpr float environment_fit_strong = 1.20f;
inline constexpr float environment_adaptive = 0.50f;
inline constexpr float occasion_mixed = 0.60f;
inline constexpr float occasion_strong = 1.00f;
inline constexpr float party_music = 0.50f;
inline constexpr float off_mode = 1.40f;                // scene does not suit the mode
}  // namespace weight

// Live-music dwell (seconds) and minimum time before mid-scene cuts (ms).
inline constexpr float showcase_dwell_s = 45.0f;
inline constexpr float showcase_dwell_calm_s = 52.0f;
inline constexpr float showcase_dwell_peak_s = 38.0f;
inline constexpr float breather_dwell_s = 20.0f;
inline constexpr std::uint64_t showcase_min_dwell_ms = 24000;
inline constexpr std::uint64_t music_min_dwell_ms = 18000;
inline constexpr std::uint64_t mismatch_hold_ms = 10000;
inline constexpr float phrase_tau_s = 10.0f;

inline constexpr std::uint64_t freshness_delay_ms = 6 * 60000;
inline constexpr std::uint64_t freshness_ramp_ms = 24 * 60000;
inline constexpr std::uint64_t failure_cooldown_base_ms = 2 * 60000;
inline constexpr std::uint64_t failure_cooldown_max_ms = 30 * 60000;
}  // namespace AutomaticDirectorDetail
