// Regression tests for plugin lifecycle hook handling and registry publishing.
#include <shared/matrix/Scene.h>
#include <shared/matrix/plugin/lifecycle.h>
#include <shared/matrix/plugin/main.h>
#include <shared/matrix/plugin_loader/loader.h>

#include <atomic>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
int failures = 0;

void expect(const bool condition, const std::string &what) {
    if (!condition) {
        std::cerr << "FAIL: " << what << "\n";
        ++failures;
    }
}

enum class Behaviour { Ok, ReturnError, Throw };

class FakePlugin final : public Plugins::BasicPlugin {
public:
    FakePlugin(std::string name, Behaviour behaviour) : name_(std::move(name)), behaviour_(behaviour) {}

    std::optional<string> before_server_init() override { return act("before"); }
    std::optional<string> after_server_init() override { return act("after"); }
    std::optional<string> pre_exit() override { ++pre_exit_calls; return act("exit"); }
    std::string get_plugin_name() const override { return name_; }

    int pre_exit_calls = 0;

private:
    vector<std::unique_ptr<Plugins::ImageProviderWrapper>> create_image_providers() override { return {}; }
    vector<std::unique_ptr<Plugins::SceneWrapper>> create_scenes() override { return {}; }

    std::optional<string> act(const char *stage) {
        if (behaviour_ == Behaviour::ReturnError) return std::string(stage) + " error";
        if (behaviour_ == Behaviour::Throw) throw std::runtime_error(std::string(stage) + " threw");
        return std::nullopt;
    }

    std::string name_;
    Behaviour behaviour_;
};

class NamedScene final : public Scenes::Scene {
public:
    explicit NamedScene(std::string name) : name_(std::move(name)) {}
    bool render(rgb_matrix::FrameCanvas *) override { return true; }
    void register_properties() override {}
    std::string get_name() const override { return name_; }

protected:
    tmillis_t get_default_duration() override { return 1000; }
    int get_default_weight() override { return 1; }

private:
    std::string name_;
};

class NamedSceneWrapper final : public Plugins::SceneWrapper {
public:
    explicit NamedSceneWrapper(std::string name) : name_(std::move(name)) {}
    std::unique_ptr<Scenes::Scene> create() override { return std::make_unique<NamedScene>(name_); }

private:
    std::string name_;
};

void lifecycle_runner() {
    using Plugins::LifecycleStage;
    FakePlugin ok("ok", Behaviour::Ok);
    FakePlugin returns("returns", Behaviour::ReturnError);
    FakePlugin throws("throws", Behaviour::Throw);

    expect(!Plugins::run_lifecycle_stage(&ok, LifecycleStage::BeforeServerInit), "ok plugin reports no failure");
    expect(!Plugins::run_lifecycle_stage(nullptr, LifecycleStage::BeforeServerInit), "null plugin is skipped");

    const auto returned = Plugins::run_lifecycle_stage(&returns, LifecycleStage::BeforeServerInit);
    expect(returned && returned->message == "before error" && returned->plugin == "returns",
           "returned error string becomes a failure");
    expect(returned && returned->describe() == "Plugin 'returns' failed during before_server_init: before error",
           "failure description names plugin and stage");

    const auto thrown = Plugins::run_lifecycle_stage(&throws, LifecycleStage::AfterServerInit);
    expect(thrown && thrown->message == "after threw", "thrown exception becomes a failure");

    bool threw = false;
    try {
        Plugins::run_lifecycle_stage_or_throw({&ok, &returns, &throws}, LifecycleStage::AfterServerInit);
    } catch (const std::runtime_error &e) {
        threw = std::string(e.what()).find("'returns'") != std::string::npos;
    }
    expect(threw, "or_throw raises the first failure");

    const auto all = Plugins::run_lifecycle_stage_all({&returns, &ok, &throws}, LifecycleStage::PreExit);
    expect(all.size() == 2, "run_all collects every failure");
    expect(ok.pre_exit_calls == 1 && throws.pre_exit_calls == 1,
           "run_all keeps going after a failing plugin");
}

void registry_publishing() {
    auto *pm = Plugins::PluginManager::instance();
    pm->add_scene(std::make_shared<NamedSceneWrapper>("lifecycle_alpha"));
    expect(pm->get_validation_report().ok(), "valid dynamic scene keeps registry ok");

    bool rejected = false;
    try {
        // A '/' in the id fails registry validation (not the duplicate check).
        pm->add_scene(std::make_shared<NamedSceneWrapper>("bad/scene"));
    } catch (const std::runtime_error &) {
        rejected = true;
    }
    expect(rejected, "invalid dynamic scene is rejected");
    expect(pm->get_validation_report().ok(), "report describes the live registry, not the rejected candidate");

    pm->remove_scene("lifecycle_alpha");
    expect(pm->get_validation_report().ok(), "remove keeps report ok");

    // Mirror the Shadertoy watcher: mutate on one thread while diagnostics
    // copy the report on another. Run under TSan/ASan to catch regressions.
    std::atomic<bool> stop{false};
    std::thread writer([&] {
        for (int i = 0; i < 200; ++i) {
            const auto name = "lifecycle_dyn_" + std::to_string(i % 4);
            try { pm->add_scene(std::make_shared<NamedSceneWrapper>(name)); } catch (...) {}
            pm->remove_scene(name);
        }
        stop = true;
    });
    size_t reads = 0;
    while (!stop) {
        const auto report = pm->get_validation_report();
        reads += report.errors.size() + 1;
    }
    writer.join();
    expect(reads > 0, "concurrent report reads completed");
    pm->delete_references();
}
}

int main() {
    lifecycle_runner();
    registry_publishing();
    if (failures == 0) std::cout << "plugin_lifecycle_smoke: all checks passed\n";
    return failures == 0 ? 0 : 1;
}
