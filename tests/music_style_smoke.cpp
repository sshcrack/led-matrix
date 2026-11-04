// Music style: genre mapping, metadata/audio fusion and style-aware casting.
#include <iostream>
#include <memory>

#include "matrix_control/AutomaticDirector.h"
#include "shared/matrix/input_ids.h"
#include "shared/matrix/music_style.h"
#include "shared/matrix/runtime_inputs.h"

namespace {
int failures = 0;
void check(bool ok, const char* what)
{
    if (!ok) {
        std::cerr << "FAIL: " << what << '\n';
        ++failures;
    }
}

MusicStyle::Style dominant(const MusicStyle::Weights& w)
{
    int best = 0;
    for (int i = 1; i < MusicStyle::StyleCount; ++i)
        if (w[i] > w[best]) best = i;
    return static_cast<MusicStyle::Style>(best);
}

class StyledScene final : public Scenes::Scene {
public:
    StyledScene(std::string name, std::vector<std::string> tags, float intensity = .55f)
        : name_(std::move(name)), tags_(std::move(tags)), intensity_(intensity)
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
        d.family = name_;
        d.intensity = intensity_;
        d.motion = intensity_;
        d.music_affinity = 1.0f;
        d.performance_cost = .3f;
        d.tags = tags_;
        d.automatic_default = true;
        d.automatic_eligible = true;
        return d;
    }
    tmillis_t get_default_duration() override { return 30000; }
    int get_default_weight() override { return 1; }

private:
    std::string name_;
    std::vector<std::string> tags_;
    float intensity_;
};

void publish_music(const std::string& genres)
{
    RuntimeInputs::clear_all();
    RuntimeInputs::publish(RuntimeInputIds::Audio,
        {{"loudness", 0.55}, {"loudness_slow", 0.55}, {"loudness_fast", 0.55}, {"bass", 0.5},
         {"beat_confidence", 0.8}, {"tempo_stability", 0.8}, {"silence", false}},
        std::chrono::seconds(5));
    RuntimeInputs::publish(RuntimeInputIds::SpotifyPlayback,
        {{"playing", true}, {"track_id", std::string("t1")}, {"progress_ms", std::int64_t{60000}},
         {"duration_ms", std::int64_t{200000}}},
        std::chrono::seconds(5));
    if (!genres.empty())
        RuntimeInputs::publish(RuntimeInputIds::MusicGenre,
            {{"track_id", std::string("t1")}, {"genres", genres}, {"source", std::string("deezer")}},
            std::chrono::seconds(5));
}
}  // namespace

int main()
{
    using MusicStyle::Style;
    check(dominant(MusicStyle::from_genres({"Electro", "Dance"})) == Style::Edm, "Electro/Dance -> edm");
    check(dominant(MusicStyle::from_genres({"Rap/Hip Hop"})) == Style::HipHop, "Rap/Hip Hop -> hiphop");
    check(dominant(MusicStyle::from_genres({"Singer & Songwriter"})) == Style::Acoustic, "Deezer singer & songwriter -> acoustic");
    check(dominant(MusicStyle::from_genres({"german hip hop", "german indie", "german indie pop"})) != Style::Rock,
          "german indie/rap is not classed as rock");
    check(dominant(MusicStyle::from_genres({"heavy metal"})) == Style::Rock, "heavy metal -> rock");
    check(dominant(MusicStyle::from_genres({"Chill Out", "Ambient"})) == Style::Chill, "Chill Out -> chill");
    const auto none = MusicStyle::from_genres({"zzz unknown"});
    check(none[0] == 0 && none[1] == 0 && none[3] == 0, "unknown genre stays unclassified");
    check(MusicStyle::split_genres("Rock, Pop ,").size() == 2, "split_genres trims and drops empties");

    // Metadata only counts for the track it was looked up for.
    publish_music("Rock");
    auto info = MusicStyle::describe(RuntimeInputs::snapshot());
    check(info.dominant == Style::Rock && info.confidence > 0.5f, "genre metadata drives the style");
    RuntimeInputs::publish(RuntimeInputIds::SpotifyPlayback,
        {{"playing", true}, {"track_id", std::string("t2")}}, std::chrono::seconds(5));
    info = MusicStyle::describe(RuntimeInputs::snapshot());
    check(info.source.rfind("genre", 0) != 0, "stale genre of the previous track is ignored");

    check(MusicStyle::style_fit(MusicStyle::describe([] { publish_music("Rock"); return RuntimeInputs::snapshot(); }()),
                                {"style:rock"}) > 0.8f, "rock look fits rock");
    check(MusicStyle::style_fit(info, {"music"}, 0.35f) == 0.35f, "untagged looks are neutral");

    // Casting: the style-matched showcase look leads; showcase beats plain music looks.
    std::vector<std::shared_ptr<Scenes::Scene>> scenes{
        std::make_shared<StyledScene>("rock_look", std::vector<std::string>{"music", "audio-reactive", "music-showcase", "style:rock"}),
        std::make_shared<StyledScene>("chill_look", std::vector<std::string>{"music", "audio-reactive", "music-showcase", "style:chill"}),
        std::make_shared<StyledScene>("plain_music", std::vector<std::string>{"music", "audio-reactive"}),
        std::make_shared<StyledScene>("ambient", std::vector<std::string>{"nature"}),
    };
    for (const auto& [genre, expected] : {std::pair{"Rock", "rock_look"}, std::pair{"Chill Out", "chill_look"}}) {
        publish_music(genre);
        AutomaticDirector director(11);
        const auto ranked = director.rank(scenes, RuntimeInputs::snapshot());
        if (ranked.empty()) { check(false, "ranking empty"); continue; }
        check(ranked.front().scene->get_name() == expected, "style-matched showcase look ranks first");
        float plain = 0, ambient = 0;
        for (const auto& c : ranked) {
            if (c.scene->get_name() == "plain_music") plain = c.score;
            if (c.scene->get_name() == "ambient") ambient = c.score;
        }
        check(plain > ambient, "music looks beat ambient looks while music plays");

        // Showcase looks get a longer dwell than plain music looks.
        const auto showcase_ms = director.presentation_duration(ranked.front().scene, RuntimeInputs::snapshot());
        const auto plain_ms = director.presentation_duration(scenes[2], RuntimeInputs::snapshot());
        check(showcase_ms > plain_ms, "showcase looks dwell longer");
    }

    // A drop does not cut a drop-reactive look early in its dwell.
    {
        publish_music("Electro");
        auto drop_look = std::make_shared<StyledScene>(
            "drop_look", std::vector<std::string>{"music", "audio-reactive", "music-showcase", "drop-reactive", "style:edm"}, .5f);
        std::vector<std::shared_ptr<Scenes::Scene>> catalog{drop_look,
            std::make_shared<StyledScene>("hot", std::vector<std::string>{"music", "audio-reactive", "style:edm"}, .95f)};
        AutomaticDirector director(5);
        director.record_played(drop_look);
        auto publish_drop = [](std::int64_t drops) {
            RuntimeInputs::publish(RuntimeInputIds::Audio,
                {{"loudness", 0.55}, {"loudness_slow", 0.55}, {"loudness_fast", 0.55}, {"bass", 0.5},
                 {"beat_confidence", 0.8}, {"tempo_stability", 0.8}, {"drop_counter", drops}, {"silence", false}},
                std::chrono::seconds(5));
        };
        publish_drop(0);
        (void)director.consider_switch(catalog, drop_look, RuntimeInputs::snapshot(), 1000, 50000);
        publish_drop(1);
        const auto op = director.consider_switch(catalog, drop_look, RuntimeInputs::snapshot(), 26000, 50000);
        check(!op.should_switch, "drop-reactive showcase plays through a drop mid-dwell");
    }

    RuntimeInputs::clear_all();
    if (failures == 0)
        std::cout << "music_style_smoke passed\n";
    return failures == 0 ? 0 : 1;
}
