#include "matrix_control/AutomaticDirector.h"
#include <shared/matrix/input_ids.h>
#include <shared/matrix/runtime_inputs.h>

#include <iostream>

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

    AutomaticDirector a(123), b(123);
    const auto sa = a.choose(scenes, RuntimeInputs::snapshot());
    const auto sb = b.choose(scenes, RuntimeInputs::snapshot());
    if (!sa.scene || !sb.scene || sa.scene->get_name() != sb.scene->get_name()) {
        std::cerr << "seeded selection is not repeatable\n"; return 4;
    }
    std::cout << "automatic director ranks context, availability, performance and history deterministically\n";
}
