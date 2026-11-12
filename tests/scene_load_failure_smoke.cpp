// A scene whose load_properties() throws must not abort config loading: it is
// kept as a FallbackScene and its saved config survives the next save.
#include <shared/matrix/Scene.h>
#include <shared/matrix/config/data.h>
#include <shared/matrix/plugin/main.h>
#include <shared/matrix/plugin_loader/loader.h>

#include <iostream>
#include <stdexcept>
#include <string>

namespace {
int failures = 0;

void expect(const bool condition, const std::string &what) {
    if (!condition) {
        std::cerr << "FAIL: " << what << "\n";
        ++failures;
    }
}

class ThrowingScene final : public Scenes::Scene {
public:
    bool render(rgb_matrix::FrameCanvas *) override { return true; }
    void register_properties() override {}
    std::string get_name() const override { return "load_failure_throwing"; }
    void load_properties(const nlohmann::json &) override { throw std::runtime_error("Could not get page size"); }

protected:
    tmillis_t get_default_duration() override { return 1000; }
    int get_default_weight() override { return 1; }
};

class OkScene final : public Scenes::Scene {
public:
    bool render(rgb_matrix::FrameCanvas *) override { return true; }
    void register_properties() override {}
    std::string get_name() const override { return "load_failure_ok"; }

protected:
    tmillis_t get_default_duration() override { return 1000; }
    int get_default_weight() override { return 1; }
};

template<typename T>
class Wrapper final : public Plugins::SceneWrapper {
public:
    std::unique_ptr<Scenes::Scene> create() override { return std::make_unique<T>(); }
};
}

int main() {
    auto *pm = Plugins::PluginManager::instance();
    pm->add_scene(std::make_shared<Wrapper<ThrowingScene>>());
    pm->add_scene(std::make_shared<Wrapper<OkScene>>());

    const nlohmann::json bad = {
        {"type", "load_failure_throwing"},
        {"arguments", {{"page", 7}, {"source", "keep-me"}}},
        {"uuid", "uuid-bad"},
        {"variant", "night"},
    };
    const nlohmann::json good = {{"type", "load_failure_ok"}, {"arguments", nlohmann::json::object()}, {"uuid", "uuid-ok"}};

    std::shared_ptr<ConfigData::Preset> preset;
    bool threw = false;
    try {
        ConfigData::from_json(nlohmann::json{{"scenes", nlohmann::json::array({good, bad, good})}}, preset);
    } catch (const std::exception &e) {
        threw = true;
        std::cerr << "unexpected: " << e.what() << "\n";
    }
    expect(!threw, "a throwing load_properties does not escape preset loading");
    expect(preset && preset->scenes.size() == 3, "all scenes of the preset are kept");

    if (preset && preset->scenes.size() == 3) {
        expect(preset->scenes[0]->get_name() == "load_failure_ok", "other scenes load normally");

        nlohmann::json saved;
        ConfigData::to_json(saved, preset);
        const auto &round = saved.at("scenes").at(1);
        expect(round.at("type") == "load_failure_throwing", "failed scene keeps its type");
        expect(round.at("arguments") == bad.at("arguments"), "failed scene keeps its arguments on save");
        expect(round.at("uuid") == "uuid-bad", "failed scene keeps its uuid");
        expect(round.value("variant", "") == "night", "failed scene keeps its variant");
    }

    pm->delete_references();
    if (failures == 0) std::cout << "scene_load_failure_smoke: all checks passed\n";
    return failures == 0 ? 0 : 1;
}
