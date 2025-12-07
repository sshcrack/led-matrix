#pragma once
#include "shared/matrix/Scene.h"
#include "shared/matrix/wrappers.h"
#include "shared/matrix/utils/FrameTimer.h"
#include <vector>
#include <random>
#include <ctime>

namespace Scenes {
    struct Sparkle {
        float x, y;
        float vx, vy;
        float life;
        float max_life;
        rgb_matrix::Color color;
    };

    class ChristmasCountdownScene : public Scene {
    private:
        FrameTimer frameTimer;
        std::vector<Sparkle> sparkles;
        std::mt19937 rng;
        
        // Properties
        PropertyPointer<bool> show_sparkles = MAKE_PROPERTY("show_sparkles", bool, true);
        PropertyPointer<int> sparkle_density = MAKE_PROPERTY_MINMAX("sparkle_density", int, 30, 0, 100);
        PropertyPointer<rgb_matrix::Color> text_color = MAKE_PROPERTY("text_color", rgb_matrix::Color, rgb_matrix::Color(255, 215, 0));
        PropertyPointer<rgb_matrix::Color> bg_color = MAKE_PROPERTY("bg_color", rgb_matrix::Color, rgb_matrix::Color(10, 0, 20));
        PropertyPointer<bool> show_hours = MAKE_PROPERTY("show_hours", bool, false);
        
        void spawnSparkle();
        void drawCountdown(float t);
        void setPixelSafe(int x, int y, uint8_t r, uint8_t g, uint8_t b);
        int getDaysUntilChristmas();
        int getHoursUntilChristmas();

    public:
        ChristmasCountdownScene();
        ~ChristmasCountdownScene() override = default;

        bool render(RGBMatrixBase *matrix) override;
        string get_name() const override { return "christmas_countdown"; }
        void register_properties() override;
        
        tmillis_t get_default_duration() override { return 30000; }
        int get_default_weight() override { return 10; }
    };

    class ChristmasCountdownSceneWrapper : public Plugins::SceneWrapper {
    public:
        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create() override;
    };
}
