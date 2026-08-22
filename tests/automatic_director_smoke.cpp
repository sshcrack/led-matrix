#include "matrix_control/AutomaticDirector.h"
#include <shared/matrix/input_ids.h>
#include <shared/matrix/runtime_inputs.h>

#include <iostream>
#include <filesystem>
#include <shared/matrix/config/MainConfig.h>

namespace {
class TestScene final : public Scenes::Scene {
public:
    TestScene(std::string name, float intensity, float music, float cost, bool needs_audio, std::string family)
        : name_(std::move(name)), intensity_(intensity), music_(music), cost_(cost), needs_audio_(needs_audio), family_(std::move(family)) {
        update_default_properties(); register_properties(); load_properties(nlohmann::json::object());
    }
    bool render(rgb_matrix::FrameCanvas *) override { return true; }
    void register_properties() override {}
    std::string get_name() const override { return name_; }
    Scenes::SceneDescriptor get_descriptor() const override {
        auto d = Scene::get_descriptor(); d.automatic_eligible = true; d.family = family_;
        d.intensity = intensity_; d.music_affinity = music_; d.performance_cost = cost_;
        return d;
    }
    Scenes::SceneInputSpec get_runtime_input_spec() const override {
        Scenes::SceneInputSpec s; if (needs_audio_) s.require(RuntimeInputIds::Audio); return s;
    }
    tmillis_t get_default_duration() override { return 1000; }
    int get_default_weight() override { return 1; }
private:
    std::string name_; float intensity_, music_, cost_; bool needs_audio_; std::string family_;
};
}

int main() {
    RuntimeInputs::clear_all();
    std::vector<std::shared_ptr<Scenes::Scene>> scenes{
        std::make_shared<TestScene>("ambient_a", .4f, .1f, .2f, false, "ambient"),
        std::make_shared<TestScene>("ambient_b", .5f, .2f, .3f, false, "organic"),
        std::make_shared<TestScene>("music", .8f, 1.f, .4f, true, "music"),
    };
    AutomaticDirector director(7);
    auto ranked = director.rank(scenes, RuntimeInputs::snapshot());
    if (ranked.size() != 2) { std::cerr << "audio-required scene was not filtered\n"; return 1; }

    RuntimeInputs::publish(RuntimeInputIds::Audio, {{"loudness", 0.9}}, std::chrono::seconds(1));
    ranked = director.rank(scenes, RuntimeInputs::snapshot());
    if (ranked.size() != 3 || ranked.front().scene->get_name() != "music") {
        std::cerr << "live audio did not prioritize music-affine scene\n"; return 2;
    }

    director.record_played(scenes[2]);
    ranked = director.rank(scenes, RuntimeInputs::snapshot());
    if (ranked.front().scene->get_name() == "music") {
        std::cerr << "recent-history penalty did not diversify selection\n"; return 3;
    }

    auto sequence = [&](AutomaticDirector &candidate, int count) {
        std::vector<std::string> result;
        for (int i = 0; i < count; ++i) {
            const auto decision = candidate.choose(scenes, RuntimeInputs::snapshot());
            if (!decision.scene) return std::vector<std::string>{};
            result.push_back(decision.scene->get_name() + ":" + decision.scene->get_variant_id());
            candidate.record_played(decision.scene);
        }
        return result;
    };

    AutomaticDirector a(123), b(123);
    const auto sequence_a = sequence(a, 12);
    const auto sequence_b = sequence(b, 12);
    if (sequence_a.empty() || sequence_a != sequence_b) {
        std::cerr << "seeded Director sequence is not repeatable\n"; return 4;
    }

    AutomaticDirector baseline(777);
    const auto expected_after_reseed = sequence(baseline, 10);
    a.reseed(777);
    const auto actual_after_reseed = sequence(a, 10);
    if (actual_after_reseed != expected_after_reseed) {
        std::cerr << "reseed did not reset Director state reproducibly\n"; return 5;
    }

    const auto diagnostics = a.diagnostics();
    if (diagnostics.value("seed", std::string{}) != "777" ||
        diagnostics.value("decision_count", std::uint64_t{0}) != 10 ||
        !diagnostics.contains("context") || !diagnostics["context"].value("audio_available", false) ||
        diagnostics["candidates"].empty()) {
        std::cerr << "Director diagnostics are missing reproducibility/reasoning state\n"; return 6;
    }

    const auto config_path = std::filesystem::temp_directory_path() / "automatic-director-seed-smoke.json";
    std::filesystem::remove(config_path);
    std::uint64_t persisted_seed = 0;
    {
        Config::MainConfig first(config_path.string());
        persisted_seed = first.get_automatic_director_seed();
        if (persisted_seed == 0 || !first.save()) {
            std::cerr << "fresh config did not generate/persist a Director seed\n"; return 7;
        }
    }
    {
        Config::MainConfig second(config_path.string());
        if (second.get_automatic_director_seed() != persisted_seed) {
            std::cerr << "Director seed changed across config reload\n"; return 8;
        }
        const auto generation_before = second.get_automatic_director_generation();
        second.set_automatic_director_seed(persisted_seed);
        if (second.get_automatic_director_generation() != generation_before + 1) {
            std::cerr << "reapplying the same seed did not request a Director reset\n"; return 9;
        }
        second.set_automatic_director_seed(424242);
        if (!second.save()) return 10;
    }
    {
        Config::MainConfig third(config_path.string());
        if (third.get_automatic_director_seed() != 424242) {
            std::cerr << "explicit Director seed did not persist\n"; return 11;
        }
    }
    std::filesystem::remove(config_path);

    std::cout << "automatic director ranks context, persists seeds, and reproduces complete seeded sequences\n";
}
