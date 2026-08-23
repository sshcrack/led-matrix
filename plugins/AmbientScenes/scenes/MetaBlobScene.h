#pragma once

#include "shared/matrix/Scene.h"
#include "shared/matrix/plugin/main.h"
#include "MetaballRenderer.h"

namespace AmbientScenes {
class MetaBlobScene : public Scenes::Scene {
private:
    PropertyPointer<int> num_blobs = MAKE_PROPERTY_MINMAX("num_blobs", int, 10, 1, 24);
    PropertyPointer<float> threshold = MAKE_PROPERTY_MINMAX("threshold", float, 0.0003f, 0.00003f, 0.0012f);
    PropertyPointer<float> speed = MAKE_PROPERTY_MINMAX("speed", float, 0.25f, 0.03f, 1.5f);
    PropertyPointer<float> move_range = MAKE_PROPERTY_MINMAX("move_range", float, 0.5f, 0.05f, 1.0f);
    PropertyPointer<bool> audio_reactive = MAKE_PROPERTY("audio_reactive", bool, false);
    PropertyPointer<float> audio_strength = MAKE_PROPERTY_MINMAX("audio_strength", float, 0.75f, 0.0f, 2.0f);
    PropertyPointer<float> color_speed = MAKE_PROPERTY_MINMAX("color_speed", float, 0.033f, 0.0f, 0.25f);

    float time = 0.0f;
    float audio_bass = 0.0f;
    float audio_mids = 0.0f;
    float audio_treble = 0.0f;
    float audio_balance = 0.0f;
    float beat_pulse = 0.0f;
    float drop_pulse = 0.0f;
    float section_hue = 0.0f;
    uint64_t last_beat_counter = 0;
    uint64_t last_drop_counter = 0;
    uint64_t last_section_counter = 0;
    AmbientScenes::MetaballRenderer renderer_;

public:
    explicit MetaBlobScene();
    ~MetaBlobScene() override = default;

    bool render(rgb_matrix::FrameCanvas *canvas) override;
    void initialize(int width, int height) override;

    [[nodiscard]] bool supports_virtual_time() const override { return true; }
    tmillis_t get_default_duration() override { return 20000; }
    int get_default_weight() override { return 1; }

    [[nodiscard]] std::string get_name() const override;
    [[nodiscard]] std::string get_category() const override { return "Ambient"; }
    [[nodiscard]] Scenes::SceneDescriptor get_descriptor() const override;

    Scenes::SceneCapabilities get_capabilities() const override {
        auto caps = Scenes::Scene::get_capabilities();
        caps.supports_audio = true;
        caps.supports_remote_rendering = true;
        return caps;
    }

    [[nodiscard]] Previews::SceneSpec get_preview_spec() const override {
        auto spec = Previews::SceneSpec::with_inputs({Previews::Inputs::Audio});
        spec.set_property("audio_reactive", true).set_property("audio_strength", 1.0f);
        return spec;
    }

    void register_properties() override;
    using Scene::Scene;
};

class MetaBlobSceneWrapper : public Plugins::SceneWrapper {
    std::unique_ptr<Scenes::Scene> create() override;
};
} // namespace AmbientScenes
