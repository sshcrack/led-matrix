#pragma once

#include "shared/matrix/Scene.h"
#include "shared/matrix/plugin/main.h"
#include <vector>
#include <cmath>

namespace AmbientScenes {
    class OrbitingPlanetsScene : public Scenes::Scene {
    private:
        struct Planet {
            float orbital_radius;
            float orbital_speed;
            float angle;
            float size;
            uint8_t r, g, b;

            Planet(float radius, float speed, float sz, uint8_t _r, uint8_t _g, uint8_t _b)
                : orbital_radius(radius), orbital_speed(speed), angle(0), size(sz), r(_r), g(_g), b(_b) {}
        };

        std::vector<Planet> planets;
        float time_elapsed = 0;

        PropertyPointer<float> rotation_speed = MAKE_PROPERTY("rotation_speed", float, 0.5f);
        PropertyPointer<bool> show_trails = MAKE_PROPERTY("show_trails", bool, false);

    public:
        explicit OrbitingPlanetsScene();
        ~OrbitingPlanetsScene() override = default;

        void register_properties() override;
        bool render(RGBMatrixBase *matrix) override;
        void initialize(RGBMatrixBase *matrix, rgb_matrix::FrameCanvas *l_offscreen_canvas) override;

        tmillis_t get_default_duration() override { return 45000; }
        int get_default_weight() override { return 2; }
        [[nodiscard]] std::string get_name() const override;

        using Scene::Scene;
    };

    class OrbitingPlanetsSceneWrapper : public Plugins::SceneWrapper {
        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create();
    };
}
