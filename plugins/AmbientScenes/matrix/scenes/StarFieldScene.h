#pragma once

#include "shared/matrix/Scene.h"
#include "shared/matrix/plugin/main.h"
#include <vector>
#include <random>
#include <chrono>

namespace AmbientScenes {
    class StarFieldScene : public Scenes::Scene {
    private:
        struct Star {
            float x = 0.0f, y = 0.0f, z = 1.0f;
            float previous_z = 1.0f;
            float hue = 0.0f;
            void respawn(float max_depth, std::mt19937& rng);
            void update(float speed);
        };

        std::vector<Star> stars;
        std::random_device rd;
        std::mt19937 gen;
        std::uniform_real_distribution<> dis;
        float time = 0.0f;
        std::chrono::steady_clock::time_point last_update;

        PropertyPointer<int> num_stars = MAKE_PROPERTY("num_stars", int, 90);
        PropertyPointer<float> speed = MAKE_PROPERTY("speed", float, 0.025f);
        PropertyPointer<bool> enable_twinkle = MAKE_PROPERTY("enable_twinkle", bool, true);
        PropertyPointer<float> max_depth = MAKE_PROPERTY("max_depth", float, 3.0f);
        PropertyPointer<bool> colored_stars = MAKE_PROPERTY("colored_stars", bool, true);
        PropertyPointer<float> streak_length = MAKE_PROPERTY_MINMAX("streak_length", float, 0.7f, 0.0f, 2.0f);
        PropertyPointer<bool> drifting_center = MAKE_PROPERTY("drifting_center", bool, true);

        static void hsv_to_rgb(float h, float s, float v, uint8_t& r, uint8_t& g, uint8_t& b);
        static void draw_line(rgb_matrix::FrameCanvas* canvas, int x0, int y0, int x1, int y1,
                              uint8_t r, uint8_t g, uint8_t b, int width, int height);

    public:
        explicit StarFieldScene();
        ~StarFieldScene() override = default;
        void register_properties() override;
        bool render(rgb_matrix::FrameCanvas *canvas) override;
        void initialize(int width, int height) override;
        tmillis_t get_default_duration() override { return 20000; }
        int get_default_weight() override { return 1; }
        [[nodiscard]] std::string get_name() const override;
        using Scene::Scene;
    };

    class StarFieldSceneWrapper : public Plugins::SceneWrapper {
    public:
        std::unique_ptr<Scenes::Scene> create() override;
    };
}
