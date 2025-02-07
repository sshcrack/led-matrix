#pragma once

#include "Scene.h"
#include "plugin.h"
#include <vector>
#include <random>

namespace Scenes {
    class FireScene : public Scene {
    private:
        int width = 0;
        int height = 0;
        float time = 0.0f;  // Simulation time
        std::vector<std::vector<int>> fire_pixels;
        std::mt19937 rng;
        
        // Helper methods for shader simulation
        static float hash12(float x, float y) ;
        float linear_noise(float x, float y) const;
        float fbm(float x, float y) const;
        
        // Configuration
        int cooling_factor;      // How much the fire cools down (default 70)
        int sparking_chance;     // Chance of sparks at bottom (default 120)
        int spark_height;        // Height of spark region (default 3)
        float update_delay;      // Delay between updates in seconds
        float accumulated_time;
        
        std::chrono::steady_clock::time_point last_update;
        
        void cool_down();
        void spark_bottom();
        void spread_fire();
        void render_fire(rgb_matrix::Canvas* canvas);

    public:
        explicit FireScene(const nlohmann::json &config);
        bool render(rgb_matrix::RGBMatrix *matrix) override;
        void initialize(rgb_matrix::RGBMatrix *matrix) override;
        [[nodiscard]] string get_name() const override;
    };

    class FireSceneWrapper : public Plugins::SceneWrapper {
        Scenes::Scene *create_default() override;
        Scenes::Scene *from_json(const nlohmann::json &args) override;
    };
}
