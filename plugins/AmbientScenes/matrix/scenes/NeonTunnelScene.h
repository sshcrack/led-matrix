#pragma once

#include "shared/matrix/Scene.h"
#include "shared/matrix/plugin/main.h"

namespace AmbientScenes {
    class NeonTunnelScene : public Scenes::Scene {
    private:
        PropertyPointer<float> speed = MAKE_PROPERTY("speed", float, 2.0f);
        PropertyPointer<float> distance_factor = MAKE_PROPERTY("distance_factor", float, 100.0f);
        PropertyPointer<float> angle_factor = MAKE_PROPERTY("angle_factor", float, 8.0f);
        PropertyPointer<float> hue_shift_speed = MAKE_PROPERTY("hue_shift_speed", float, 1.0f);
        
        float time_counter = 0.0f;

        void hsl_to_rgb(float h, float s, float l, uint8_t& r, uint8_t& g, uint8_t& b);

    public:
        explicit NeonTunnelScene();
        ~NeonTunnelScene() override = default;

        void register_properties() override;
        bool render(RGBMatrixBase *matrix) override;
        void initialize(RGBMatrixBase *matrix, rgb_matrix::FrameCanvas *l_offscreen_canvas) override;

        tmillis_t get_default_duration() override { return 30000; }
        int get_default_weight() override { return 1; }
        [[nodiscard]] std::string get_name() const override;

        using Scene::Scene;
    };

    class NeonTunnelSceneWrapper : public Plugins::SceneWrapper {
        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create() override;
    };
}
