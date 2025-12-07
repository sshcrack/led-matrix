#pragma once
#include "shared/matrix/Scene.h"
#include "shared/matrix/wrappers.h"
#include "shared/matrix/utils/FrameTimer.h"
#include <vector>
#include <random>

namespace Scenes {
    struct TreeLight {
        int x, y;
        float phase;          // For blinking animation
        rgb_matrix::Color color;
    };

    class ChristmasTreeScene : public Scene {
    private:
        FrameTimer frameTimer;
        std::vector<TreeLight> lights;
        std::mt19937 rng;
        
        // Properties
        PropertyPointer<float> blink_speed = MAKE_PROPERTY("blink_speed", float, 1.5f);
        PropertyPointer<bool> rainbow_mode = MAKE_PROPERTY("rainbow_mode", bool, false);
        PropertyPointer<rgb_matrix::Color> light_color_1 = MAKE_PROPERTY("light_color_1", rgb_matrix::Color, rgb_matrix::Color(255, 0, 0));
        PropertyPointer<rgb_matrix::Color> light_color_2 = MAKE_PROPERTY("light_color_2", rgb_matrix::Color, rgb_matrix::Color(0, 255, 0));
        PropertyPointer<rgb_matrix::Color> light_color_3 = MAKE_PROPERTY("light_color_3", rgb_matrix::Color, rgb_matrix::Color(0, 0, 255));
        PropertyPointer<rgb_matrix::Color> tree_color = MAKE_PROPERTY("tree_color", rgb_matrix::Color, rgb_matrix::Color(0, 100, 0));
        PropertyPointer<rgb_matrix::Color> trunk_color = MAKE_PROPERTY("trunk_color", rgb_matrix::Color, rgb_matrix::Color(101, 67, 33));
        PropertyPointer<bool> star_enabled = MAKE_PROPERTY("star_enabled", bool, true);
        PropertyPointer<int> light_pattern = MAKE_PROPERTY_MINMAX("light_pattern", int, 0, 0, 2);
        
        void initializeLights();
        void drawTree();
        void drawStar(float t);
        void setPixelSafe(int x, int y, uint8_t r, uint8_t g, uint8_t b);
        rgb_matrix::Color hsv_to_rgb(float h, float s, float v);

    public:
        ChristmasTreeScene();
        ~ChristmasTreeScene() override = default;

        bool render(RGBMatrixBase *matrix) override;
        string get_name() const override { return "christmas_tree"; }
        void register_properties() override;
        
        tmillis_t get_default_duration() override { return 30000; }
        int get_default_weight() override { return 10; }
    };

    class ChristmasTreeSceneWrapper : public Plugins::SceneWrapper {
    public:
        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create() override;
    };
}
