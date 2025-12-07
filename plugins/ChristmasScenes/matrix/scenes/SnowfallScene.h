#pragma once
#include "shared/matrix/Scene.h"
#include "shared/matrix/wrappers.h"
#include "shared/matrix/utils/FrameTimer.h"
#include <vector>
#include <random>

namespace Scenes {
    struct Snowflake {
        float x, y;           // Position
        float vy;             // Fall velocity
        float vx;             // Horizontal drift (wind)
        float size;           // Snowflake size (0.5 - 2.0)
        float phase;          // Animation phase for flicker effect
        uint8_t brightness;   // Brightness (200-255)
    };

    class SnowfallScene : public Scene {
    private:
        FrameTimer frameTimer;
        std::vector<Snowflake> snowflakes;
        std::mt19937 rng;
        std::uniform_real_distribution<float> dist_x;
        std::uniform_real_distribution<float> dist_y;
        std::uniform_real_distribution<float> dist_vy;
        std::uniform_real_distribution<float> dist_size;
        std::uniform_real_distribution<float> dist_brightness;
        std::uniform_real_distribution<float> dist_phase;
        
        // Properties
        PropertyPointer<int> density = MAKE_PROPERTY_MINMAX("density", int, 50, 10, 200);
        PropertyPointer<float> fall_speed = MAKE_PROPERTY("fall_speed", float, 1.0f);
        PropertyPointer<float> wind = MAKE_PROPERTY("wind", float, 0.5f);
        PropertyPointer<bool> accumulate = MAKE_PROPERTY("accumulate", bool, true);
        PropertyPointer<rgb_matrix::Color> bg_color = MAKE_PROPERTY("bg_color", rgb_matrix::Color, rgb_matrix::Color(0, 0, 20));
        
        std::vector<uint8_t> ground_height; // Accumulated snow height per column
        
        void initializeSnowflakes();
        void resetSnowflake(Snowflake& flake);
        void setPixelSafe(int x, int y, uint8_t r, uint8_t g, uint8_t b);

    public:
        SnowfallScene();
        ~SnowfallScene() override = default;

        bool render(RGBMatrixBase *matrix) override;
        string get_name() const override { return "snowfall"; }
        void register_properties() override;
        
        tmillis_t get_default_duration() override { return 30000; }
        int get_default_weight() override { return 10; }
    };

    class SnowfallSceneWrapper : public Plugins::SceneWrapper {
    public:
        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create() override;
    };
}
