#pragma once

#include "Scene.h"
#include "plugin/main.h"
#include <vector>
#include <random>

namespace AmbientScenes {
class FireScene : public Scenes::Scene {
        int width = 0;
        int height = 0;
        float time = 0.0f;  // Simulation time
        std::vector<std::vector<int>> fire_pixels;
        std::vector<std::tuple<float, float, float, float>> sparks; // x, y, velocity, lifespan

        // RNG for spark generation
        std::mt19937 rng;
        std::uniform_real_distribution<float> dist_x;
        std::uniform_real_distribution<float> dist_vel;
        std::uniform_real_distribution<float> dist_life;

        // Helper methods for shader simulation
        static float hash12(float x, float y);

        [[nodiscard]] static float linear_noise(float x, float y);

        [[nodiscard]] float fbm(float x, float y) const;

        PropertyPointer<float> frames_per_second = MAKE_PROPERTY("fps", float, 30.0f);
        PropertyPointer<float> fire_intensity = MAKE_PROPERTY("intensity", float, 1.0f);
        PropertyPointer<float> fire_speed = MAKE_PROPERTY("speed", float, 1.0f);
        PropertyPointer<float> wind_strength = MAKE_PROPERTY("wind", float, 0.2f);
        PropertyPointer<bool> show_sparks = MAKE_PROPERTY("sparks", bool, true);
        PropertyPointer<int> color_scheme = MAKE_PROPERTY("color_scheme", int, 0);
        
        [[maybe_unused]] float update_delay{};      // Delay between updates in seconds
        float accumulated_time;

        std::chrono::steady_clock::time_point last_update;

        void render_fire(rgb_matrix::Canvas *canvas);
        void update_sparks(float delta_time);
        void render_sparks(rgb_matrix::Canvas *canvas);
        void maybe_emit_spark();

    public:
        explicit FireScene();
        ~FireScene() override = default;

        bool render(RGBMatrixBase *matrix) override;

        void initialize(RGBMatrixBase *matrix, FrameCanvas *l_offscreen_canvas) override;

        [[nodiscard]] string get_name() const override;

        void register_properties() override;
        void load_properties(const nlohmann::json &j) override;
    };

    class FireSceneWrapper : public Plugins::SceneWrapper {
        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create();
    };
}
