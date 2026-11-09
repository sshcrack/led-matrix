#include <shared/matrix/Scene.h>
#include <shared/matrix/plugin_loader/loader.h>
#include <shared/matrix/remote_render.h>
#include <shared/common/remote_render_protocol.h>

#include <fstream>
#include <iostream>
#include <memory>

namespace {
int properties_loaded = 0;

class ContractProbe : public Scenes::Scene {
    PropertyPointer<float> speed_ = MAKE_PROPERTY("speed", float, 1.0f);
    PropertyPointer<bool> enabled_ = MAKE_PROPERTY("enabled", bool, false);
public:
    int contract = 1;
    bool remote = true;
    double phase = 0.0;
    int style = 0;
    std::string get_name() const override { return "contract_probe"; }
    int remote_contract_version() const override { return contract; }
    Scenes::SceneCapabilities get_capabilities() const override {
        auto caps = Scene::get_capabilities();
        caps.supports_remote_rendering = remote;
        return caps;
    }
    void register_properties() override { add_property(speed_); add_property(enabled_); }
    void load_properties(const nlohmann::json &arguments) override {
        ++properties_loaded;
        Scene::load_properties(arguments);
    }
    float speed() const { return speed_->get(); }
    bool enabled() const { return enabled_->get(); }
    nlohmann::json snapshot_runtime_state() const override { return {{"phase", phase}}; }
    void restore_runtime_state(const nlohmann::json &value) override { phase = value.at("phase").get<double>(); }
    bool render(rgb_matrix::FrameCanvas *) override { style = 1; return true; }
    tmillis_t get_default_duration() override { return 1000; }
    int get_default_weight() override { return 1; }
};

// Different renderer implementation and object layout, identical wire contract.
class UpdatedRenderer final : public ContractProbe {
    float extra_render_cache_[16]{};
public:
    bool render(rgb_matrix::FrameCanvas *) override {
        extra_render_cache_[0] = speed();
        style = 2;
        return true;
    }
};

class ProbeWrapper final : public Plugins::SceneWrapper {
public:
    bool updated = false;
    int contract = 1;
    bool remote = true;
    std::unique_ptr<Scenes::Scene> create() override {
        std::unique_ptr<ContractProbe> scene = updated
            ? std::make_unique<UpdatedRenderer>() : std::make_unique<ContractProbe>();
        scene->contract = contract;
        scene->remote = remote;
        return scene;
    }
};
}

int main(int argc, char **argv)
{
    if (argc != 2) return 1;
    std::ifstream input(argv[1]);
    const auto fixture = nlohmann::json::parse(input);
    auto *registry = Plugins::PluginManager::instance();
    (void)registry->get_scenes();
    auto install = [&](bool updated, int contract = 1, bool remote = true) {
        registry->remove_scene("contract_probe");
        auto wrapper = std::make_shared<ProbeWrapper>();
        wrapper->updated = updated;
        wrapper->contract = contract;
        wrapper->remote = remote;
        registry->add_scene(wrapper);
    };
    install(false);
    RemoteRender::report_worker_heartbeat(RemoteRenderProtocol::Version, {{"contract_probe", 1}});
    if (!RemoteRender::worker_available("contract_probe")) return 2;
    for (const bool updated : {false, true}) {
        install(updated);
        if (!RemoteRender::worker_available("contract_probe")) return 3;
        auto scene = Scenes::Scene::from_remote_command(fixture);
        auto &probe = dynamic_cast<ContractProbe &>(*scene);
        probe.initialize(32, 32);
        probe.restore_runtime_state(fixture.at("runtime_state"));
        probe.render(nullptr);
        if (probe.speed() != 2.5f || !probe.enabled() || probe.phase != 0.25
            || probe.style != (updated ? 2 : 1)
            || probe.snapshot_runtime_state() != fixture.at("runtime_state")) {
            std::cerr << "saved v1 properties/state broke across renderer implementations\n";
            return 4;
        }
    }
    auto must_reject = [&](const nlohmann::json &command) {
        properties_loaded = 0;
        try { (void)Scenes::Scene::from_remote_command(command); }
        catch (const std::exception &) { return properties_loaded == 0; }
        return false;
    };
    for (const auto &field : {"protocol", "scene_contract"}) {
        auto invalid = fixture;
        invalid.erase(field);
        if (!must_reject(invalid)) return 5;
        for (const auto &value : {nlohmann::json(0), nlohmann::json(-1), nlohmann::json(99),
                                  nlohmann::json("1"), nlohmann::json(1.5),
                                  nlohmann::json(std::uint64_t{4294967297})}) {
            invalid = fixture;
            invalid[field] = value;
            if (!must_reject(invalid)) return 6;
        }
    }
    auto unknown = fixture;
    unknown["scene"] = "uninstalled_scene";
    if (!must_reject(unknown)) return 7;
    install(true, 2);
    if (RemoteRender::worker_available("contract_probe") || !must_reject(fixture)) return 8;
    auto v2 = fixture;
    v2["scene_contract"] = 2;
    if (!Scenes::Scene::from_remote_command(v2)) return 9;
    install(true, 1, false);
    if (RemoteRender::worker_available("contract_probe") || !must_reject(fixture)) return 10;
    registry->remove_scene("contract_probe");
    if (RemoteRender::worker_available("contract_probe")) return 11;
    std::cout << "saved scene contract payload and renderer compatibility passed\n";
}
