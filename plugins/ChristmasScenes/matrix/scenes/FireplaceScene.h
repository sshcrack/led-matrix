#pragma once
#include "shared/matrix/Scene.h"
#include "shared/matrix/wrappers.h"
#include "shared/matrix/utils/FrameTimer.h"
#include <vector>
#include <random>

namespace Scenes {
    struct Flame {
        float x, y;           // Position
        float vy;             // Upward velocity
        float vx;             // Horizontal drift
        float life;           // Remaining lifetime (0-1)
        float max_life;       // Maximum lifetime
        float intensity;      // Brightness intensity
        float heat;           // Heat value (affects color)
    };

    class FireplaceScene : public Scene {
    private:
        FrameTimer frameTimer;
        std::vector<Flame> flames;
        std::mt19937 rng;
        std::uniform_real_distribution<float> dist_spawn_x;
        std::uniform_real_distribution<float> dist_vy;
        std::uniform_real_distribution<float> dist_vx;
        std::uniform_real_distribution<float> dist_life;
        std::uniform_real_distribution<float> dist_intensity;
        std::uniform_real_distribution<float> dist_heat;
        
        // Properties
        PropertyPointer<int> flame_density = MAKE_PROPERTY_MINMAX("flame_density", int, 80, 20, 200);
        PropertyPointer<float> flicker_speed = MAKE_PROPERTY("flicker_speed", float, 1.0f);
        PropertyPointer<float> heat_intensity = MAKE_PROPERTY("heat_intensity", float, 1.0f);
        PropertyPointer<bool> show_logs = MAKE_PROPERTY("show_logs", bool, true);
        PropertyPointer<bool> show_embers = MAKE_PROPERTY("show_embers", bool, true);
        PropertyPointer<rgb_matrix::Color> bg_color = MAKE_PROPERTY("bg_color", rgb_matrix::Color, rgb_matrix::Color(5, 5, 10));
        
        void spawnFlame();
        void drawLogs();
        void drawEmbers(float t);
        void setPixelSafe(int x, int y, uint8_t r, uint8_t g, uint8_t b);
        rgb_matrix::Color getFlameColor(float heat, float intensity);

    public:
        FireplaceScene();
        ~FireplaceScene() override = default;

        bool render(RGBMatrixBase *matrix) override;
        string get_name() const override { return "fireplace"; }
        void register_properties() override;
        
        tmillis_t get_default_duration() override { return 30000; }
        int get_default_weight() override { return 10; }
    };

    class FireplaceSceneWrapper : public Plugins::SceneWrapper {
    public:
        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create() override;
    };
}
