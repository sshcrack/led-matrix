#include <shared/matrix/config/MainConfig.h>
#include <shared/matrix/input_ids.h>
#include <shared/matrix/runtime_inputs.h>

#include <algorithm>
#include <filesystem>
#include <iostream>

#include "matrix_control/AutomaticDirector.h"

namespace {
class TestScene final : public Scenes::Scene {
public:
    TestScene(std::string name, float intensity, float music, float cost, bool needs_audio, std::string family, float motion = .5f,
              std::vector<std::string> tags = {}, std::vector<std::string> required_inputs = {}, tmillis_t duration = 1000,
              bool automatic_eligible = true)
        : name_(std::move(name)),
          intensity_(intensity),
          music_(music),
          cost_(cost),
          needs_audio_(needs_audio),
          family_(std::move(family)),
          motion_(motion),
          tags_(std::move(tags)),
          required_inputs_(std::move(required_inputs)),
          duration_(duration),
          automatic_eligible_(automatic_eligible)
    {
        update_default_properties();
        register_properties();
        load_properties(nlohmann::json::object());
    }
    bool render(rgb_matrix::FrameCanvas*) override { return true; }
    void register_properties() override {}
    std::string get_name() const override { return name_; }
    Scenes::SceneDescriptor get_descriptor() const override
    {
        auto d = Scene::get_descriptor();
        d.automatic_eligible = automatic_eligible_;
        d.family = family_;
        d.intensity = intensity_;
        d.motion = motion_;
        d.music_affinity = music_;
        d.performance_cost = cost_;
        d.tags = tags_;
        d.automatic_default = automatic_default;
        return d;
    }
    Scenes::SceneInputSpec get_runtime_input_spec() const override
    {
        Scenes::SceneInputSpec s;
        if (needs_audio_)
            s.require(RuntimeInputIds::Audio);
        for (const auto& input : required_inputs_)
            s.require(input);
        return s;
    }
    tmillis_t get_default_duration() override { return duration_; }
    int get_default_weight() override { return 1; }
    bool automatic_default = true;

private:
    std::string name_;
    float intensity_, music_, cost_;
    bool needs_audio_;
    std::string family_;
    float motion_;
    std::vector<std::string> tags_;
    std::vector<std::string> required_inputs_;
    tmillis_t duration_;
    bool automatic_eligible_;
};
}  // namespace

int main()
{
    RuntimeInputs::clear_all();
    std::vector<std::shared_ptr<Scenes::Scene>> scenes{
        std::make_shared<TestScene>("ambient_a", .4f, .1f, .2f, false, "ambient"),
        std::make_shared<TestScene>("ambient_b", .5f, .2f, .3f, false, "organic"),
        std::make_shared<TestScene>("music", .8f, 1.f, .4f, true, "music", .78f, std::vector<std::string>{"music", "energetic"}),
        std::make_shared<TestScene>("music_director", .7f, 1.f, .6f, true, "music-director", .72f,
                                    std::vector<std::string>{"music", "director", "adaptive"},
                                    std::vector<std::string>{}, 1000, false),
    };
    AutomaticDirector director(7);
    auto ranked = director.rank(scenes, RuntimeInputs::snapshot());
    if (ranked.size() != 2) {
        std::cerr << "audio-required scene was not filtered\n";
        return 1;
    }

    RuntimeInputs::publish(RuntimeInputIds::Audio,
                           {{"loudness", 0.9},
                            {"loudness_fast", 0.92},
                            {"bass", 0.80},
                            {"sub_bass", 0.76},
                            {"treble", 0.58},
                            {"hihat", 0.44},
                            {"onset_strength", 0.62},
                            {"energy_trend", 0.28},
                            {"beat_confidence", 0.88},
                            {"tempo_stability", 0.90},
                            {"silence", false}},
                           std::chrono::seconds(1));
    ranked = director.rank(scenes, RuntimeInputs::snapshot());
    if (ranked.size() != 3 || ranked.front().scene->get_name() != "music") {
        std::cerr << "live music did not prioritize the strongest directly selectable reactive scene\n";
        return 2;
    }

    // Repo-owned Shadertoy scenes are visually curated against actual matrix
    // renders. When desktop rendering is present, an otherwise equivalent
    // shader should outrank the older native approximation instead of merely
    // tying it and disappearing into the stochastic candidate pool.
    RuntimeInputs::set_available(RuntimeInputIds::Desktop, true, {{"connected", true}});
    std::vector<std::shared_ptr<Scenes::Scene>> music_quality_scenes{
        std::make_shared<TestScene>(
            "legacy_reactive", .78f, 1.0f, .30f, true, "orbital", .82f,
            std::vector<std::string>{"music", "audio-reactive", "geometric"}),
        std::make_shared<TestScene>(
            "shader:showcase", .78f, 1.0f, .30f, false, "orbital", .82f,
            std::vector<std::string>{"music", "audio-reactive", "geometric", "shader", "shadertoy", "gpu-rendered", "showcase"},
            std::vector<std::string>{std::string(RuntimeInputIds::Desktop)}),
    };
    AutomaticDirector shader_music_director(71);
    const auto shader_music_ranked = shader_music_director.rank(music_quality_scenes, RuntimeInputs::snapshot());
    if (shader_music_ranked.size() != 2 || shader_music_ranked.front().scene->get_name() != "shader:showcase") {
        std::cerr << "curated audio-reactive shader was not preferred over an equivalent legacy visual\n";
        return 22;
    }

    const auto previously_best = ranked.front().scene;
    director.record_played(previously_best);
    ranked = director.rank(scenes, RuntimeInputs::snapshot());
    if (ranked.front().scene->get_name() == previously_best->get_name()) {
        std::cerr << "recent-history penalty did not diversify selection\n";
        return 3;
    }

    // Audio input can remain connected while playback is paused. In that state
    // automatic mode should prefer restrained ambient motion rather than treating
    // a stale loudness value as active music.
    RuntimeInputs::publish(RuntimeInputIds::Audio, {{"loudness", 0.9}, {"loudness_fast", 0.9}, {"silence", true}}, std::chrono::seconds(1));
    AutomaticDirector paused(19);
    ranked = paused.rank(scenes, RuntimeInputs::snapshot());
    if (ranked.empty() || ranked.front().scene->get_name().starts_with("music")) {
        std::cerr << "paused/silent audio still prioritized high-energy music visuals\n";
        return 12;
    }

    // The same preference applies to scenic shader ambience when music is not
    // active, while high-music-affinity shaders remain naturally penalized by
    // the normal context score.
    RuntimeInputs::clear_all();
    RuntimeInputs::set_available(RuntimeInputIds::Desktop, true, {{"connected", true}});
    std::vector<std::shared_ptr<Scenes::Scene>> ambient_quality_scenes{
        std::make_shared<TestScene>(
            "legacy_scenic", .46f, .10f, .25f, false, "aurora", .48f,
            std::vector<std::string>{"ambient", "scenic", "calm"}),
        std::make_shared<TestScene>(
            "shader:scenic", .46f, .10f, .25f, false, "aurora", .48f,
            std::vector<std::string>{"ambient", "scenic", "calm", "shader", "shadertoy", "gpu-rendered", "showcase"},
            std::vector<std::string>{std::string(RuntimeInputIds::Desktop)}),
    };
    AutomaticDirector shader_ambient_director(73);
    const auto shader_ambient_ranked = shader_ambient_director.rank(ambient_quality_scenes, RuntimeInputs::snapshot());
    if (shader_ambient_ranked.size() != 2 || shader_ambient_ranked.front().scene->get_name() != "shader:scenic") {
        std::cerr << "curated scenic shader was not preferred for desktop-backed ambient playback\n";
        return 23;
    }

    // Spotify media should follow the track lifecycle rather than monopolizing
    // every music decision. Album art introduces/closes a track, while SpotifyMV
    // becomes attractive only once the track is underway and its desktop
    // toolchain has explicitly reported ready.
    std::vector<std::shared_ptr<Scenes::Scene>> spotify_scenes{
        std::make_shared<TestScene>(
            "cover", .36f, 1.0f, .68f, false, "album-art", .28f,
            std::vector<std::string>{"music", "media", "album-art", "spotify"},
            std::vector<std::string>{std::string(RuntimeInputIds::SpotifyPlayback)}),
        std::make_shared<TestScene>(
            "spotifymv", .70f, 1.0f, .18f, false, "spotify-video", .86f,
            std::vector<std::string>{"music", "media", "spotify", "spotify-video", "cinematic"},
            std::vector<std::string>{std::string(RuntimeInputIds::Desktop),
                                     std::string(RuntimeInputIds::SpotifyPlayback),
                                     std::string(RuntimeInputIds::SpotifyMVReady)},
            210000),
    };
    RuntimeInputs::clear_all();
    RuntimeInputs::set_available(RuntimeInputIds::Desktop, true, {{"connected", true}});
    RuntimeInputs::set_available(RuntimeInputIds::SpotifyMVReady, true,
        {{"tools_ready", true}, {"state", std::string("preparing")}, {"first_frame_ready", false},
         {"track_id", std::string("track-a")}});
    RuntimeInputs::publish(
        RuntimeInputIds::SpotifyPlayback,
        {{"playing", true}, {"track_id", std::string("track-a")},
         {"progress_ms", std::int64_t{10000}}, {"duration_ms", std::int64_t{200000}}},
        std::chrono::seconds(1));
    AutomaticDirector spotify_director(23);
    ranked = spotify_director.rank(spotify_scenes, RuntimeInputs::snapshot());
    if (ranked.size() != 1 || ranked.front().scene->get_name() != "cover") {
        std::cerr << "Spotify track intro did not prefer album art\n";
        return 13;
    }

    RuntimeInputs::set_available(RuntimeInputIds::SpotifyMVReady, true,
        {{"tools_ready", true}, {"state", std::string("playing")}, {"first_frame_ready", true},
         {"track_id", std::string("track-a")}});
    RuntimeInputs::publish(
        RuntimeInputIds::SpotifyPlayback,
        {{"playing", true}, {"track_id", std::string("track-a")},
         {"progress_ms", std::int64_t{90000}}, {"duration_ms", std::int64_t{200000}}},
        std::chrono::seconds(1));
    const auto mid_track = RuntimeInputs::snapshot();
    ranked = spotify_director.rank(spotify_scenes, mid_track);
    if (ranked.size() != 2 || ranked.front().scene->get_name() != "spotifymv") {
        std::cerr << "mid-track Spotify playback did not prioritize SpotifyMV\n";
        return 14;
    }
    const auto mv_duration = spotify_director.presentation_duration(ranked.front().scene, mid_track);
    if (mv_duration < 30000 || mv_duration > 45000) {
        std::cerr << "Automatic Mode did not clamp SpotifyMV to a sensible presentation duration\n";
        return 15;
    }

    // Event-driven handoffs carry the candidate that caused the event. The
    // subsequent stochastic selector must honor that candidate while it remains
    // eligible, otherwise an MV-ready event could switch to unrelated visuals.
    AutomaticDirector mv_handoff_director(31);
    const auto mv_handoff = mv_handoff_director.consider_switch(
        spotify_scenes, spotify_scenes.front(), mid_track, 9000, 60000);
    if (!mv_handoff.should_switch || !mv_handoff.preferred_scene
        || mv_handoff.preferred_scene->get_name() != "spotifymv") {
        std::cerr << "prepared SpotifyMV handoff did not preserve its preferred scene\n";
        return 20;
    }
    const auto mv_decision = mv_handoff_director.choose(
        spotify_scenes, mid_track, "cover", mv_handoff.preferred_scene);
    if (!mv_decision.scene || mv_decision.scene->get_name() != "spotifymv") {
        std::cerr << "preferred event candidate was lost during final Director selection\n";
        return 21;
    }

    RuntimeInputs::publish(
        RuntimeInputIds::SpotifyPlayback,
        {{"playing", true}, {"track_id", std::string("track-a")},
         {"progress_ms", std::int64_t{190000}}, {"duration_ms", std::int64_t{200000}}},
        std::chrono::seconds(1));
    ranked = spotify_director.rank(spotify_scenes, RuntimeInputs::snapshot());
    if (ranked.empty() || ranked.front().scene->get_name() != "cover") {
        std::cerr << "end-of-track Spotify playback still tried to start a music video\n";
        return 16;
    }

    RuntimeInputs::set_available(RuntimeInputIds::SpotifyMVReady, false, {{"ready", false}});
    ranked = spotify_director.rank(spotify_scenes, RuntimeInputs::snapshot());
    if (ranked.size() != 1 || ranked.front().scene->get_name() != "cover") {
        std::cerr << "SpotifyMV remained eligible after desktop toolchain became unavailable\n";
        return 17;
    }

    RuntimeInputs::InputState aged_playback;
    aged_playback.available = true;
    aged_playback.age_seconds = 5.0;
    aged_playback.signals = {
        {"playing", true}, {"track_id", std::string("aged-track")},
        {"progress_ms", std::int64_t{20000}}, {"duration_ms", std::int64_t{100000}}
    };
    RuntimeInputs::Snapshot aged_snapshot({
        {std::string(RuntimeInputIds::SpotifyPlayback), aged_playback}
    });
    AutomaticDirector aged_director(27);
    (void)aged_director.choose(scenes, aged_snapshot);
    const auto aged_context = aged_director.diagnostics()["context"];
    const float aged_progress = aged_context.value("spotify_progress", 0.0f);
    if (aged_progress < 0.249f || aged_progress > 0.251f) {
        std::cerr << "Automatic Director did not interpolate Spotify playback age: "
                  << aged_progress << "\n";
        return 24;
    }

    // The global director should consume musical structure itself instead of
    // relying on the nested MusicDirector scene. A section event after minimum
    // dwell creates a handoff when the current look no longer fits the music.
    RuntimeInputs::clear_all();
    RuntimeInputs::publish(RuntimeInputIds::Audio,
        {{"loudness", 0.72}, {"loudness_slow", 0.70}, {"loudness_fast", 0.74},
         {"bass", 0.62}, {"beat_confidence", 0.82}, {"tempo_stability", 0.84},
         {"beat_counter", std::int64_t{16}}, {"drop_counter", std::int64_t{1}},
         {"section_counter", std::int64_t{2}}, {"silence", false}},
        std::chrono::seconds(1));
    AutomaticDirector event_director(29);
    const auto current_scene = std::make_shared<TestScene>("too_calm", .08f, .1f, .2f, false, "still", .08f);
    event_director.record_played(current_scene);
    auto opportunity = event_director.consider_switch(scenes, current_scene, RuntimeInputs::snapshot(), 9000, 60000);
    if (opportunity.should_switch) {
        std::cerr << "director reacted while priming musical event counters\n";
        return 18;
    }
    RuntimeInputs::publish(RuntimeInputIds::Audio,
        {{"loudness", 0.72}, {"loudness_slow", 0.70}, {"loudness_fast", 0.74},
         {"bass", 0.62}, {"beat_confidence", 0.82}, {"tempo_stability", 0.84},
         {"beat_counter", std::int64_t{20}}, {"drop_counter", std::int64_t{1}},
         {"section_counter", std::int64_t{3}}, {"silence", false}},
        std::chrono::seconds(1));
    opportunity = event_director.consider_switch(scenes, current_scene, RuntimeInputs::snapshot(), 10000, 60000);
    if (!opportunity.should_switch || opportunity.reason.find("section") == std::string::npos) {
        std::cerr << "global director ignored a durable musical section change\n";
        return 19;
    }

    // Sections arrive every 15-30 s in most music. They must not cut a scene
    // that still fits mid-dwell; instead a finished scene waits briefly for the
    // next section so the handoff lands on a musical phrase.
    auto publish_music = [](std::int64_t section) {
        RuntimeInputs::publish(RuntimeInputIds::Audio,
            {{"loudness", 0.72}, {"loudness_slow", 0.70}, {"loudness_fast", 0.74},
             {"bass", 0.62}, {"beat_confidence", 0.82}, {"tempo_stability", 0.84},
             {"section_counter", section}, {"silence", false}},
            std::chrono::seconds(1));
    };
    RuntimeInputs::clear_all();
    publish_music(1);
    AutomaticDirector phrase_director(97);
    (void)phrase_director.consider_switch(scenes, scenes.front(), RuntimeInputs::snapshot(), 1000, 30000);
    const auto phrase_context = phrase_director.diagnostics()["context"];
    const float fit_intensity = phrase_context["target_intensity"].get<float>();
    const float fit_motion = phrase_context["target_motion"].get<float>();
    std::vector<std::shared_ptr<Scenes::Scene>> phrase_scenes{
        std::make_shared<TestScene>("phrase_a", fit_intensity, .5f, .3f, false, "phrase_a", fit_motion),
        std::make_shared<TestScene>("phrase_b", fit_intensity, .5f, .3f, false, "phrase_b", fit_motion),
    };
    publish_music(2);
    opportunity = phrase_director.consider_switch(phrase_scenes, phrase_scenes[0], RuntimeInputs::snapshot(), 10000, 30000);
    if (opportunity.should_switch) {
        std::cerr << "section change cut a well-fitting scene mid-dwell: " << opportunity.reason << "\n";
        return 41;
    }
    opportunity = phrase_director.consider_switch(phrase_scenes, phrase_scenes[0], RuntimeInputs::snapshot(), 30500, 30000);
    if (opportunity.should_switch) {
        std::cerr << "finished scene did not wait for a musical phrase boundary\n";
        return 42;
    }
    publish_music(3);
    opportunity = phrase_director.consider_switch(phrase_scenes, phrase_scenes[0], RuntimeInputs::snapshot(), 31000, 30000);
    if (!opportunity.should_switch || opportunity.reason.find("phrase") == std::string::npos) {
        std::cerr << "finished scene did not hand off on the next section boundary\n";
        return 43;
    }
    AutomaticDirector grace_director(98);
    (void)grace_director.consider_switch(phrase_scenes, phrase_scenes[0], RuntimeInputs::snapshot(), 1000, 30000);
    opportunity = grace_director.consider_switch(phrase_scenes, phrase_scenes[0], RuntimeInputs::snapshot(),
        30000 + AutomaticDirector::phrase_grace_ms, 30000);
    if (!opportunity.should_switch) {
        std::cerr << "phrase wait was not bounded by the grace period\n";
        return 44;
    }
    RuntimeInputs::clear_all();
    AutomaticDirector ambient_dwell_director(99);
    opportunity = ambient_dwell_director.consider_switch(phrase_scenes, phrase_scenes[0], {}, 30000, 30000);
    if (!opportunity.should_switch) {
        std::cerr << "ambient scene did not end at its planned dwell\n";
        return 45;
    }

    // Restore active music for deterministic sequence/reseed checks below.
    RuntimeInputs::clear_all();
    RuntimeInputs::publish(RuntimeInputIds::Audio,
                           {{"loudness", 0.7},
                            {"loudness_fast", 0.72},
                            {"bass", 0.58},
                            {"treble", 0.46},
                            {"onset_strength", 0.35},
                            {"beat_confidence", 0.75},
                            {"tempo_stability", 0.82},
                            {"silence", false}},
                           std::chrono::seconds(1));

    auto sequence = [&](AutomaticDirector& candidate, int count) {
        std::vector<std::string> result;
        for (int i = 0; i < count; ++i) {
            const auto decision = candidate.choose(scenes, RuntimeInputs::snapshot());
            if (!decision.scene)
                return std::vector<std::string>{};
            result.push_back(decision.scene->get_name() + ":" + decision.scene->get_variant_id());
            candidate.record_played(decision.scene);
        }
        return result;
    };

    AutomaticDirector a(123), b(123);
    const auto sequence_a = sequence(a, 12);
    const auto sequence_b = sequence(b, 12);
    if (sequence_a.empty() || sequence_a != sequence_b) {
        std::cerr << "seeded Director sequence is not repeatable\n";
        return 4;
    }

    AutomaticDirector baseline(777);
    const auto expected_after_reseed = sequence(baseline, 10);
    a.reseed(777);
    const auto actual_after_reseed = sequence(a, 10);
    if (actual_after_reseed != expected_after_reseed) {
        std::cerr << "reseed did not reset Director state reproducibly\n";
        return 5;
    }

    const auto diagnostics = a.diagnostics();
    if (diagnostics.value("seed", std::string{}) != "777" || diagnostics.value("decision_count", std::uint64_t{0}) != 10 ||
        !diagnostics.contains("context") || !diagnostics["context"].value("audio_available", false) || diagnostics["candidates"].empty()) {
        std::cerr << "Director diagnostics are missing reproducibility/reasoning state\n";
        return 6;
    }

    RuntimeInputs::clear_all();
    std::vector<std::shared_ptr<Scenes::Scene>> journey_scenes{
        std::make_shared<TestScene>("rest", .25f, .1f, .2f, false, "soft", .27f),
        std::make_shared<TestScene>("crest", .68f, .1f, .2f, false, "bright", .70f),
    };
    AutomaticDirector journey_director(81);
    const auto duration = journey_director.diagnostics()["journey"]["duration_ms"].get<tmillis_t>();
    if (duration < 24 * 60000 || duration > 36 * 60000
        || journey_director.rank(journey_scenes, {}).front().scene->get_name() != "rest") {
        std::cerr << "journey did not start with a restrained opening\n";
        return 25;
    }
    const auto opening_dwell = journey_director.presentation_duration(journey_scenes.front(), {});
    journey_director.advance_journey(duration * 7 / 10);
    if (journey_director.rank(journey_scenes, {}).front().scene->get_name() != "crest"
        || journey_director.presentation_duration(journey_scenes.front(), {}) >= opening_dwell) {
        std::cerr << "journey crest did not change selection and pacing\n";
        return 26;
    }
    const auto crest_state = journey_director.diagnostics()["journey"];
    (void)journey_director.rank(journey_scenes, {});
    (void)journey_director.diagnostics();
    journey_director.advance_journey(-1000);
    if (journey_director.diagnostics()["journey"] != crest_state) {
        std::cerr << "read-only calls or invalid elapsed time advanced the journey\n";
        return 27;
    }
    journey_director.advance_journey(duration - duration * 7 / 10);
    const auto next_journey = journey_director.diagnostics()["journey"];
    if (next_journey["phase"] != "settle" || next_journey["cycle"] != 1
        || next_journey["motif"] == crest_state["motif"]
        || journey_director.rank(journey_scenes, {}).front().scene->get_name() != "rest") {
        std::cerr << "journey did not resolve and start a new motif\n";
        return 28;
    }
    VisualJourney continuous(81), batched(81);
    float previous_intensity = continuous.frame().intensity;
    for (int second = 0; second < 6 * 60 * 60; ++second) {
        continuous.advance(1000);
        const auto frame = continuous.frame();
        if (std::abs(frame.intensity - previous_intensity) > .01f
            || frame.progress < 0 || frame.progress >= 1 || frame.motion < 0 || frame.motion > 1) {
            std::cerr << "six-hour journey had a discontinuity or invalid target\n";
            return 29;
        }
        previous_intensity = frame.intensity;
    }
    batched.advance(6 * 60 * 60 * 1000);
    if (continuous.elapsed_ms() != batched.elapsed_ms() || continuous.cycle() != batched.cycle()
        || continuous.frame().motif != batched.frame().motif) {
        std::cerr << "journey depends on renderer polling cadence\n";
        return 30;
    }
    journey_director.reseed(81);
    AutomaticDirector fresh_journey(81);
    if (journey_director.diagnostics()["journey"] != fresh_journey.diagnostics()["journey"]) {
        std::cerr << "reseed did not restart the journey\n";
        return 31;
    }

    RuntimeInputs::InputState music_input;
    music_input.available = true;
    music_input.signals = {{"loudness", .9}, {"loudness_slow", .9}, {"bass", .8}, {"silence", false}};
    const RuntimeInputs::Snapshot loud_music({{std::string(RuntimeInputIds::Audio), music_input}});
    (void)fresh_journey.choose(journey_scenes, loud_music);
    const auto opening_music = fresh_journey.diagnostics()["context"]["target_intensity"].get<float>();
    fresh_journey.advance_journey(duration * 7 / 10);
    (void)fresh_journey.choose(journey_scenes, loud_music);
    const auto crest_music = fresh_journey.diagnostics()["context"]["target_intensity"].get<float>();
    music_input.signals["silence"] = true;
    const RuntimeInputs::Snapshot quiet_music({{std::string(RuntimeInputIds::Audio), music_input}});
    (void)fresh_journey.choose(journey_scenes, quiet_music);
    const auto quiet_crest = fresh_journey.diagnostics()["context"]["target_intensity"].get<float>();
    if (opening_music < .5f || crest_music - opening_music > .1f || quiet_crest > .38f) {
        std::cerr << "journey overrode music energy or ignored detected silence\n";
        return 32;
    }

    // Authored performance costs are only priors. A scene measured to blow its
    // frame budget on this Pi must lose to an otherwise identical scene.
    RuntimeInputs::clear_all();
    auto twin = [](std::string name, std::string family = "twin") {
        return std::make_shared<TestScene>(std::move(name), .4f, .1f, .3f, false, std::move(family), .4f);
    };
    std::vector<std::shared_ptr<Scenes::Scene>> twins{twin("twin_a"), twin("twin_b")};
    AutomaticDirector load_director(91);
    load_director.report_outcome(twins[0], {.render_load = 1.3f});
    load_director.report_outcome(twins[1], {.render_load = 0.2f});
    ranked = load_director.rank(twins, {});
    if (ranked.size() != 2 || ranked.front().scene->get_name() != "twin_b") {
        std::cerr << "measured Pi render load did not override the authored cost prior\n";
        return 33;
    }
    load_director.reseed(92);
    ranked = load_director.rank(twins, {});
    if (ranked.front().scene->get_name() != "twin_b"
        || load_director.diagnostics()["scenes"].size() != 2) {
        std::cerr << "reseed discarded measured hardware render loads\n";
        return 34;
    }

    // A failed presentation cools the scene down with backoff, but never leaves
    // the matrix without a scene when nothing else is eligible.
    AutomaticDirector health_director(93);
    health_director.report_outcome(twins[0], {.failed = true});
    ranked = health_director.rank(twins, {});
    if (ranked.size() != 1 || ranked.front().scene->get_name() != "twin_b") {
        std::cerr << "failed scene was not cooled down\n";
        return 35;
    }
    ranked = health_director.rank(twins, {}, "twin_b");
    if (ranked.size() != 1 || ranked.front().scene->get_name() != "twin_a") {
        std::cerr << "cooldown left Automatic Mode without any scene\n";
        return 36;
    }
    health_director.advance_journey(2 * 60000 + 1);
    if (health_director.rank(twins, {}).size() != 2) {
        std::cerr << "first failure cooldown did not expire after two minutes\n";
        return 37;
    }
    health_director.report_outcome(twins[0], {.failed = true});
    health_director.advance_journey(2 * 60000 + 1);
    if (health_director.rank(twins, {}).size() != 1) {
        std::cerr << "repeated failure did not back off the cooldown\n";
        return 38;
    }
    health_director.advance_journey(2 * 60000);
    health_director.report_outcome(twins[0], {});
    health_director.report_outcome(twins[0], {.failed = true});
    health_director.advance_journey(2 * 60000 + 1);
    if (health_director.rank(twins, {}).size() != 2) {
        std::cerr << "successful presentation did not reset the failure backoff\n";
        return 39;
    }

    // Scenes unseen for a long time regain priority so the catalog is explored
    // instead of the same well-fitting looks winning forever.
    AutomaticDirector fresh_director(95);
    std::vector<std::shared_ptr<Scenes::Scene>> fillers;
    for (int i = 0; i < 8; ++i)
        fillers.push_back(twin("filler_" + std::to_string(i), "filler"));
    fresh_director.record_played(twins[0]);
    fresh_director.advance_journey(40 * 60000);
    fresh_director.record_played(twins[1]);
    for (const auto& filler : fillers)
        fresh_director.record_played(filler);
    ranked = fresh_director.rank(twins, {});
    const auto has_reason = [](const AutomaticDirector::Candidate& candidate, std::string_view reason) {
        return std::find(candidate.reasons.begin(), candidate.reasons.end(), reason) != candidate.reasons.end();
    };
    if (ranked.size() != 2 || ranked.front().scene->get_name() != "twin_a"
        || !has_reason(ranked.front(), "not shown for a while")) {
        std::cerr << "long-unseen scene did not regain priority\n";
        return 40;
    }

    // User curation: hidden looks never play, looks that are off by default only
    // play once opted in, favorites get a boost, and hiding the look that is
    // currently visible ends it right away.
    auto off_by_default = twin("twin_c");
    off_by_default->automatic_default = false;
    std::vector<std::shared_ptr<Scenes::Scene>> curated{twins[0], twins[1], off_by_default};
    AutomaticDirector curation_director(101);
    ranked = curation_director.rank(curated, {});
    if (ranked.size() != 2 || std::any_of(ranked.begin(), ranked.end(),
            [](const auto& c) { return c.scene->get_name() == "twin_c"; })) {
        std::cerr << "look that is off by default was played without opting in\n";
        return 46;
    }
    curation_director.set_preferences({{"twin_a", "hidden"}, {"twin_b", "favorite"}, {"twin_c", "on"}});
    ranked = curation_director.rank(curated, {});
    if (ranked.size() != 2 || ranked.front().scene->get_name() != "twin_b"
        || !has_reason(ranked.front(), "marked as a favorite")
        || std::any_of(ranked.begin(), ranked.end(), [](const auto& c) { return c.scene->get_name() == "twin_a"; })) {
        std::cerr << "hidden/favorite/opt-in preferences were not honored\n";
        return 47;
    }
    opportunity = curation_director.consider_switch(curated, twins[0], {}, 2000, 30000);
    if (!opportunity.should_switch || opportunity.reason.find("hidden") == std::string::npos) {
        std::cerr << "hiding the visible look did not end it\n";
        return 48;
    }

    const auto config_path = std::filesystem::temp_directory_path() / "automatic-director-seed-smoke.json";
    std::filesystem::remove(config_path);
    std::uint64_t persisted_seed = 0;
    {
        Config::MainConfig first(config_path.string());
        persisted_seed = first.get_automatic_director_seed();
        if (persisted_seed == 0 || !first.save()) {
            std::cerr << "fresh config did not generate/persist a Director seed\n";
            return 7;
        }
    }
    {
        Config::MainConfig second(config_path.string());
        if (second.get_automatic_director_seed() != persisted_seed) {
            std::cerr << "Director seed changed across config reload\n";
            return 8;
        }
        const auto generation_before = second.get_automatic_director_generation();
        second.set_automatic_director_seed(persisted_seed);
        if (second.get_automatic_director_generation() != generation_before + 1) {
            std::cerr << "reapplying the same seed did not request a Director reset\n";
            return 9;
        }
        second.set_automatic_director_seed(424242);
        const auto preferences_before = second.get_automatic_preferences_version();
        second.set_automatic_preference("shader:scenic_aurora", "favorite");
        second.set_automatic_preference("boids/swarm", "on");
        second.set_automatic_preference("boids/swarm", "default");
        if (second.get_automatic_preferences_version() != preferences_before + 3) {
            std::cerr << "preference changes did not bump the preferences version\n";
            return 49;
        }
        if (!second.save())
            return 10;
    }
    {
        Config::MainConfig third(config_path.string());
        if (third.get_automatic_preferences() != std::map<std::string, std::string>{{"shader:scenic_aurora", "favorite"}}) {
            std::cerr << "Automatic Mode preferences did not persist\n";
            return 50;
        }
        if (third.get_automatic_director_seed() != 424242) {
            std::cerr << "explicit Director seed did not persist\n";
            return 11;
        }
    }
    std::filesystem::remove(config_path);

    std::cout << "automatic director ranks context, persists seeds, and reproduces complete seeded sequences\n";
}
