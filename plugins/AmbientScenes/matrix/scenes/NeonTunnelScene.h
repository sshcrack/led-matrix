#pragma once

#include "shared/matrix/Scene.h"
#include "shared/matrix/plugin/main.h"

namespace AmbientScenes {
    class NeonTunnelScene : public Scenes::Scene {
    private:
        PropertyPointer<float> speed = MAKE_PROPERTY_MINMAX("speed", float, 2.0f, 0.1f, 6.0f);
        PropertyPointer<float> distance_factor = MAKE_PROPERTY_MINMAX("distance_factor", float, 100.0f, 20.0f, 240.0f);
        PropertyPointer<float> angle_factor = MAKE_PROPERTY_MINMAX("angle_factor", float, 8.0f, 2.0f, 18.0f);
        PropertyPointer<bool> audio_reactive = MAKE_PROPERTY("audio_reactive", bool, false);
        PropertyPointer<float> audio_strength = MAKE_PROPERTY_MINMAX("audio_strength", float, 0.8f, 0.0f, 2.0f);
        PropertyPointer<float> hue_shift_speed = MAKE_PROPERTY_MINMAX("hue_shift_speed", float, 1.0f, 0.0f, 4.0f);
        
        float time_counter = 0.0f;
        float audio_bass = 0.0f, audio_mids = 0.0f, audio_treble = 0.0f;
        uint64_t last_beat_counter = 0;
        float beat_pulse = 0.0f;

    public:
        explicit NeonTunnelScene();
        ~NeonTunnelScene() override = default;

        Scenes::SceneCapabilities get_capabilities() const override { auto caps = Scenes::Scene::get_capabilities(); caps.supports_audio = true; return caps; }
        void register_properties() override;
        bool render(rgb_matrix::FrameCanvas *canvas) override;
        void initialize(int width, int height) override;

        [[nodiscard]] bool supports_virtual_time() const override { return true; }
        tmillis_t get_default_duration() override { return 30000; }
        int get_default_weight() override { return 1; }
        [[nodiscard]] std::string get_name() const override;
        [[nodiscard]] std::string get_category() const override { return "Ambient"; }

        using Scene::Scene;
    };

    class NeonTunnelSceneWrapper : public Plugins::SceneWrapper {
        std::unique_ptr<Scenes::Scene> create() override;
    };
}
