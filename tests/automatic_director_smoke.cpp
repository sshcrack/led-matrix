#include <shared/matrix/config/MainConfig.h>
#include <shared/matrix/input_ids.h>
#include <shared/matrix/runtime_inputs.h>

#include <filesystem>
#include <iostream>

#include "matrix_control/AutomaticDirector.h"

namespace {
class TestScene final : public Scenes::Scene {
public:
    TestScene(std::string name, float intensity, float music, float cost, bool needs_audio, std::string family, float motion = .5f,
              std::vector<std::string> tags = {}, std::vector<std::string> required_inputs = {}, tmillis_t duration = 1000)
        : name_(std::move(name)),
          intensity_(intensity),
          music_(music),
          cost_(cost),
          needs_audio_(needs_audio),
          family_(std::move(family)),
          motion_(motion),
          tags_(std::move(tags)),
          required_inputs_(std::move(required_inputs)),
          duration_(duration)
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
        d.automatic_eligible = true;
        d.family = family_;
        d.intensity = intensity_;
        d.motion = motion_;
        d.music_affinity = music_;
        d.performance_cost = cost_;
        d.tags = tags_;
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

private:
    std::string name_;
    float intensity_, music_, cost_;
    bool needs_audio_;
    std::string family_;
    float motion_;
    std::vector<std::string> tags_;
    std::vector<std::string> required_inputs_;
    tmillis_t duration_;
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
                                    std::vector<std::string>{"music", "director", "adaptive"}),
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
    if (ranked.size() != 4 || ranked.front().scene->get_name() != "music_director") {
        std::cerr << "live music did not prioritize continuously adaptive director scene\n";
        return 2;
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
    RuntimeInputs::set_available(RuntimeInputIds::SpotifyMVReady, true, {{"ready", true}});
    RuntimeInputs::publish(
        RuntimeInputIds::SpotifyPlayback,
        {{"playing", true}, {"progress_ms", std::int64_t{10000}}, {"duration_ms", std::int64_t{200000}}},
        std::chrono::seconds(1));
    AutomaticDirector spotify_director(23);
    ranked = spotify_director.rank(spotify_scenes, RuntimeInputs::snapshot());
    if (ranked.size() != 2 || ranked.front().scene->get_name() != "cover") {
        std::cerr << "Spotify track intro did not prefer album art\n";
        return 13;
    }

    RuntimeInputs::publish(
        RuntimeInputIds::SpotifyPlayback,
        {{"playing", true}, {"progress_ms", std::int64_t{90000}}, {"duration_ms", std::int64_t{200000}}},
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

    RuntimeInputs::publish(
        RuntimeInputIds::SpotifyPlayback,
        {{"playing", true}, {"progress_ms", std::int64_t{190000}}, {"duration_ms", std::int64_t{200000}}},
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
        if (!second.save())
            return 10;
    }
    {
        Config::MainConfig third(config_path.string());
        if (third.get_automatic_director_seed() != 424242) {
            std::cerr << "explicit Director seed did not persist\n";
            return 11;
        }
    }
    std::filesystem::remove(config_path);

    std::cout << "automatic director ranks context, persists seeds, and reproduces complete seeded sequences\n";
}
