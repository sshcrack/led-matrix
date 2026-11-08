#pragma once

#include "VisualJourney.h"

#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "shared/matrix/Scene.h"
#include "shared/matrix/music_style.h"
#include "shared/matrix/runtime_inputs.h"

class AutomaticDirector {
public:
    /// What Automatic Mode is for. Mixed is the general default; the others
    /// shift scoring and pacing towards one purpose.
    enum class Mode {
        Mixed,        // everything, music-first while music plays
        Environment,  // calm scenery that fits the real moment (time, weather, season, holidays)
        Party,        // high-energy music looks, quicker cuts
        Chill,        // low intensity, slow motion, long dwell
    };
    [[nodiscard]] static std::optional<Mode> parse_mode(std::string_view id);
    [[nodiscard]] static const char* mode_name(Mode mode);

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

    /// What happened while a scene was presented. `render_load` is the p95
    /// local render time divided by the scene's frame budget (1.0 = the full
    /// frame interval); leave it empty when the scene did not render locally.
    struct PresentationOutcome {
        std::optional<float> render_load;
        bool failed = false;
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
    /// into sparse switch opportunities while enforcing dwell/hysteresis, and
    /// ends the scene once `planned_ms` (from presentation_duration) is reached.
    SwitchOpportunity consider_switch(
        const std::vector<std::shared_ptr<Scenes::Scene>>& scenes,
        const std::shared_ptr<Scenes::Scene>& current_scene,
        const RuntimeInputs::Snapshot& runtime_inputs,
        tmillis_t elapsed_ms,
        tmillis_t planned_ms);

    /// How long a finished scene may wait for a musical phrase boundary. The
    /// coordinator extends its render deadline by this much in Automatic Mode.
    static constexpr tmillis_t phrase_grace_ms = 8000;

    void advance_journey(tmillis_t elapsed_ms);

    void record_played(const std::shared_ptr<Scenes::Scene>& scene);
    /// Feeds back how the most recent presentation of `scene` went on this Pi.
    /// Render loads persist across reseeds because they describe the hardware,
    /// not the seeded choice sequence.
    void report_outcome(const std::shared_ptr<Scenes::Scene>& scene, const PresentationOutcome& outcome);
    void reseed(std::uint64_t seed);
    /// User curation keyed by look_key(): "favorite", "on" or "hidden". Looks
    /// without an entry follow their descriptor's automatic_default.
    void set_preferences(std::map<std::string, std::string> preferences);
    void set_mode(Mode mode) { mode_ = mode; }
    [[nodiscard]] Mode mode() const { return mode_; }
    [[nodiscard]] static std::string look_key(const Scenes::Scene& scene);

    [[nodiscard]] std::uint64_t seed() const { return seed_; }
    [[nodiscard]] nlohmann::json diagnostics() const;

    struct Context {
        bool audio_available = false;
        bool audio_active = false;
        bool spotify = false;
        bool spotify_mv_tools_ready = false;
        bool spotify_mv_first_frame_ready = false;
        bool spotify_mv_ready = false;
        std::string spotify_track_id;
        std::string spotify_mv_track_id;
        std::string spotify_mv_state;
        std::uint64_t drop_counter = 0;
        std::uint64_t section_counter = 0;
        float loudness = 0.35f;
        float loudness_fast = 0.35f;
        float bass = 0.0f;
        float treble = 0.0f;
        float brightness = 0.0f;
        float onset = 0.0f;
        float kick = 0.0f;
        float snare = 0.0f;
        float hihat = 0.0f;
        float beat_strength = 0.0f;
        float rhythmicity = 0.0f;
        float energy_trend = 0.0f;
        float drop = 0.0f;
        float tempo_trust = 0.0f;
        float spotify_progress = 0.0f;
        float spotify_remaining_seconds = 0.0f;
        float target_intensity = 0.42f;
        float target_motion = 0.46f;
        /// Fused genre metadata + audio character of the current music.
        MusicStyle::Info style;
        std::string mode = "ambient";
    };

private:
    struct HistoryEntry {
        std::string scene;
        std::string family;
        std::string variant;
        std::string role;
        float intensity = 0.5f;
        float motion = 0.5f;
        bool music_look = false;
    };

    /// Long-lived per-scene knowledge, keyed by scene name (variants share
    /// render characteristics and failure modes).
    struct SceneMemory {
        std::optional<std::uint64_t> last_shown_ms;
        std::uint64_t presentations = 0;
        std::optional<float> render_load;
        std::uint32_t failures = 0;
        std::uint64_t cooldown_until_ms = 0;
    };

    /// Per-track introduction sequence. A new Spotify track is first
    /// introduced with an album-cover look (cover: Pending → Shown, or
    /// Skipped when no cover look is available); independently the prepared
    /// music video may be inserted once per track. A track change restarts
    /// the sequence; an empty track id means no track (NotNeeded).
    struct TrackIntroduction {
        enum class Cover { NotNeeded, Pending, Shown, Skipped };
        std::string track_id;
        Cover cover = Cover::NotNeeded;
        bool music_video_shown = false;

        /// Returns true when the track changed (the sequence restarted).
        bool sync(const std::string& id)
        {
            if (id == track_id)
                return false;
            track_id = id;
            cover = id.empty() ? Cover::NotNeeded : Cover::Pending;
            music_video_shown = false;
            return true;
        }
        void mark_cover_shown() { cover = Cover::Shown; }
        void skip_cover() { if (cover == Cover::Pending) cover = Cover::Skipped; }
        [[nodiscard]] bool awaiting_cover() const { return cover == Cover::Pending; }
        [[nodiscard]] bool cover_shown() const { return cover == Cover::Shown; }
    };

    /// Turns a monotonically increasing event counter into one-shot events.
    /// Unprimed: the first observation only records the counter, so events
    /// that happened before observation started are not replayed. A counter
    /// that goes backwards (analyser restart) re-baselines without firing.
    struct CounterEdge {
        std::optional<std::uint64_t> seen;

        bool observe(std::uint64_t value)
        {
            const bool fired = seen.has_value() && value > *seen;
            seen = value;
            return fired;
        }
        void reset() { seen.reset(); }
    };

    VisualJourney journey_;
    std::uint64_t seed_;
    mutable std::mt19937_64 rng_;
    /// Monotonic Automatic Mode clock. It only advances through
    /// advance_journey(), so it is deterministic under tests and survives
    /// reseeds (cooldowns are expressed against it).
    std::uint64_t clock_ms_ = 0;
    std::deque<HistoryEntry> history_;
    std::unordered_map<std::string, SceneMemory> memory_;
    std::map<std::string, std::string> preferences_;
    Mode mode_ = Mode::Mixed;
    Context last_context_;
    std::string last_scene_;
    std::string last_variant_;
    float last_score_ = 0.0f;
    std::vector<std::string> last_reasons_;
    std::vector<Candidate> last_ranked_;
    std::uint64_t decision_count_ = 0;
    std::string last_exclude_name_;
    TrackIntroduction track_intro_;
    CounterEdge drop_edge_;
    CounterEdge section_edge_;
    std::uint64_t next_fit_check_ms_ = 0;
    std::string last_switch_reason_;
    // Phrase-level music energy: the analyser's loudness swings within seconds,
    // so casting and fit checks follow a ~10 s average instead.
    bool phrase_primed_ = false;
    float phrase_intensity_ = 0.42f;
    float phrase_motion_ = 0.46f;
    std::uint64_t phrase_updated_ms_ = 0;
    std::string mismatch_scene_;
    std::string style_scene_;
    MusicStyle::Style style_at_start_ = MusicStyle::Style::Chill;
    float fit_error_at_start_ = 0.0f;
    std::uint64_t mismatch_since_ms_ = 0;

    [[nodiscard]] Context observe(const RuntimeInputs::Snapshot& runtime_inputs);
    /// Context with the phrase-smoothed music energy applied (const view).
    [[nodiscard]] Context music_context(const RuntimeInputs::Snapshot& runtime_inputs) const;
    /// Moves the energy targets towards what the mode is for.
    void shape_for_mode(Context& context) const;
    /// Live music leads casting and cuts (Mixed and Party only).
    [[nodiscard]] bool music_led(const Context& context) const
    {
        return context.audio_active && music_cuts_allowed();
    }
    /// Whether the mode lets the music cut scenes (track intros, music videos).
    [[nodiscard]] bool music_cuts_allowed() const { return mode_ == Mode::Mixed || mode_ == Mode::Party; }
    /// Consecutive music-reactive presentations at the end of the history.
    [[nodiscard]] int music_streak() const;
    [[nodiscard]] float history_multiplier(
        const std::string& scene,
        const std::string& family,
        const std::string& role) const;
    [[nodiscard]] bool cooling_down(const std::string& scene) const;
    bool sync_track_context(const std::string& track_id);
};
