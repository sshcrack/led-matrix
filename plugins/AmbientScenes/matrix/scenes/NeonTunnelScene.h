#pragma once

#include "shared/matrix/Scene.h"
#include "shared/matrix/plugin/main.h"
#include <chrono>

namespace AmbientScenes {
    class NeonTunnelScene : public Scenes::Scene {
    private:
        PropertyPointer<float> speed = MAKE_PROPERTY("speed", float, 2.0f);
        PropertyPointer<float> distance_factor = MAKE_PROPERTY("distance_factor", float, 100.0f);
        PropertyPointer<float> angle_factor = MAKE_PROPERTY("angle_factor", float, 8.0f);
        PropertyPointer<bool> audio_reactive = MAKE_PROPERTY("audio_reactive", bool, false);
        PropertyPointer<float> audio_strength = MAKE_PROPERTY_MINMAX("audio_strength", float, 0.8f, 0.0f, 2.0f);
        PropertyPointer<float> hue_shift_speed = MAKE_PROPERTY("hue_shift_speed", float, 1.0f);
        
        float time_counter = 0.0f;
        std::chrono::steady_clock::time_point last_update;
        float audio_bass = 0.0f, audio_mids = 0.0f, audio_treble = 0.0f;
        uint64_t last_beat_counter = 0;
        float beat_pulse = 0.0f;

    public:
        explicit NeonTunnelScene();
        ~NeonTunnelScene() override = default;

        void register_properties() override;
        bool render(rgb_matrix::FrameCanvas *canvas) override;
        void initialize(int width, int height) override;

        tmillis_t get_default_duration() override { return 30000; }
        int get_default_weight() override { return 1; }
        [[nodiscard]] std::string get_name() const override;

        using Scene::Scene;
    };

    class NeonTunnelSceneWrapper : public Plugins::SceneWrapper {
        std::unique_ptr<Scenes::Scene> create() override;
    };
}
