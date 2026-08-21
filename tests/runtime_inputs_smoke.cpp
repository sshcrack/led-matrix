#include <shared/matrix/Scene.h>
#include <shared/matrix/input_ids.h>
#include <shared/matrix/runtime_inputs.h>

#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>

namespace {
class DesktopScene final : public Scenes::Scene {
public:
    bool render(rgb_matrix::FrameCanvas *) override { return true; }
    void register_properties() override {}
    std::string get_name() const override { return "desktop_test"; }
    bool needs_desktop_app() override { return true; }

protected:
    tmillis_t get_default_duration() override { return 1000; }
    int get_default_weight() override { return 1; }
};

class OptionalAudioScene final : public Scenes::Scene {
public:
    bool render(rgb_matrix::FrameCanvas *) override { return true; }
    void register_properties() override {}
    std::string get_name() const override { return "optional_audio_test"; }
    Scenes::SceneCapabilities get_capabilities() const override {
        auto caps = Scene::get_capabilities();
        caps.supports_audio = true;
        return caps;
    }

protected:
    tmillis_t get_default_duration() override { return 1000; }
    int get_default_weight() override { return 1; }
};

class RequiredAudioScene final : public Scenes::Scene {
public:
    bool render(rgb_matrix::FrameCanvas *) override { return true; }
    void register_properties() override {}
    std::string get_name() const override { return "required_audio_test"; }
    bool needs_desktop_app() override { return true; }
    Scenes::SceneCapabilities get_capabilities() const override {
        auto caps = Scene::get_capabilities();
        caps.requires_audio = true;
        caps.supports_audio = true;
        return caps;
    }

protected:
    tmillis_t get_default_duration() override { return 1000; }
    int get_default_weight() override { return 1; }
};
}

int main()
{
    RuntimeInputs::clear_all();
    auto empty = RuntimeInputs::snapshot();
    if (empty.available(RuntimeInputIds::Desktop)) {
        std::cerr << "empty Runtime Input snapshot unexpectedly reports desktop\n";
        return 1;
    }

    RuntimeInputs::set_available(
        RuntimeInputIds::Desktop, true,
        {{"connected", true}, {"connections", std::int64_t{2}}});
    auto desktop = RuntimeInputs::snapshot();
    if (!desktop.available(RuntimeInputIds::Desktop)
        || desktop.boolean(RuntimeInputIds::Desktop, "connected") != true
        || desktop.number(RuntimeInputIds::Desktop, "connections") != 2.0) {
        std::cerr << "sticky desktop Runtime Input was not published correctly\n";
        return 1;
    }

    RuntimeInputs::publish(
        RuntimeInputIds::Audio,
        {{"bass", 0.75}, {"profile", std::string("percussion")}},
        std::chrono::milliseconds(2));
    auto audio = RuntimeInputs::snapshot();
    if (!audio.available(RuntimeInputIds::Audio)
        || !audio.number(RuntimeInputIds::Audio, "bass").has_value()
        || std::abs(*audio.number(RuntimeInputIds::Audio, "bass") - 0.75) > 0.0001
        || audio.text(RuntimeInputIds::Audio, "profile") != std::optional<std::string>("percussion")) {
        std::cerr << "audio Runtime Input signals were not readable through the snapshot\n";
        return 1;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(8));
    const auto stale_audio = RuntimeInputs::snapshot();
    const auto *audio_state = stale_audio.find(RuntimeInputIds::Audio);
    if (stale_audio.available(RuntimeInputIds::Audio) || audio_state == nullptr || !audio_state->stale) {
        std::cerr << "TTL Runtime Input did not become stale\n";
        return 1;
    }

    DesktopScene desktop_scene;
    const auto desktop_spec = desktop_scene.get_effective_runtime_inputs();
    if (!desktop_spec.is_required(RuntimeInputIds::Desktop)
        || !RuntimeInputs::satisfies(desktop_spec, desktop)) {
        std::cerr << "legacy desktop requirement was not mapped to Runtime Inputs\n";
        return 1;
    }

    OptionalAudioScene optional_audio_scene;
    const auto optional_audio_spec = optional_audio_scene.get_effective_runtime_inputs();
    if (!optional_audio_spec.accepts(RuntimeInputIds::Audio)
        || optional_audio_spec.is_required(RuntimeInputIds::Audio)) {
        std::cerr << "optional audio capability became a hard requirement\n";
        return 1;
    }

    RequiredAudioScene required_audio_scene;
    const auto required_audio_spec = required_audio_scene.get_effective_runtime_inputs();
    if (!required_audio_spec.is_required(RuntimeInputIds::Desktop)
        || !required_audio_spec.is_required(RuntimeInputIds::Audio)
        || RuntimeInputs::satisfies(required_audio_spec, desktop)) {
        std::cerr << "required audio/desktop eligibility was not enforced\n";
        return 1;
    }

    RuntimeInputs::publish(RuntimeInputIds::Audio, {{"bass", 0.5}}, std::chrono::seconds(1));
    const auto ready = RuntimeInputs::snapshot();
    if (!RuntimeInputs::satisfies(required_audio_spec, ready)) {
        std::cerr << "scene remained ineligible after all required Runtime Inputs became available\n";
        return 1;
    }

    const auto json = RuntimeInputs::to_json(ready);
    if (!json.contains("desktop") || !json.contains("audio")
        || !json["desktop"].value("available", false)
        || !json["audio"]["signals"].contains("bass")) {
        std::cerr << "Runtime Input JSON did not expose availability/signals\n";
        return 1;
    }

    RuntimeInputs::clear_all();
    return 0;
}
