#pragma once

#include "shared/matrix/Scene.h"
#include "shared/matrix/plugin/main.h"
#include <vector>
#include <random>

namespace AmbientScenes {
    class DigitalRainScene : public Scenes::Scene {
    private:
        struct Drop {
            int x;
            float y;
            float speed;
            int length;
        };

        std::vector<Drop> drops;
        std::vector<std::vector<float>> matrix_brightness;
        
        std::random_device rd;
        std::mt19937 gen;
        std::uniform_real_distribution<> dis_speed;
        std::uniform_int_distribution<> dis_length;
        std::uniform_int_distribution<> dis_x;

        PropertyPointer<int> num_drops = MAKE_PROPERTY("num_drops", int, 40);
        PropertyPointer<float> base_speed = MAKE_PROPERTY("base_speed", float, 1.0f);
        PropertyPointer<float> fade_factor = MAKE_PROPERTY("fade_factor", float, 0.90f);
        PropertyPointer<rgb_matrix::Color> color = MAKE_PROPERTY("color", rgb_matrix::Color, rgb_matrix::Color(0, 255, 0));

        void reset_drop(Drop& drop);

    public:
        explicit DigitalRainScene();
        ~DigitalRainScene() override = default;

        void register_properties() override;
        bool render(rgb_matrix::FrameCanvas *canvas) override;
        void initialize(int width, int height) override;

        tmillis_t get_default_duration() override { return 20000; }
        int get_default_weight() override { return 1; }
        [[nodiscard]] std::string get_name() const override;

        using Scene::Scene;
    };

    class DigitalRainSceneWrapper : public Plugins::SceneWrapper {
        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create() override;
    };
}
