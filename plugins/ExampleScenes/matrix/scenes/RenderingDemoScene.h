#pragma once

#include "shared/matrix/Scene.h"
#include "shared/matrix/wrappers.h"
#include "shared/matrix/utils/FrameTimer.h"
#include "shared/matrix/plugin/color.h"
#include "graphics.h"
#include <random>

namespace Scenes {
    class RenderingDemoScene : public Scene {
    private:
        FrameTimer frameTimer;
        std::mt19937 rng;
        
        // Demonstration properties
        PropertyPointer<int> demo_mode = MAKE_PROPERTY_MINMAX("demo_mode", int, 0, 0, 4);
        PropertyPointer<float> animation_speed = MAKE_PROPERTY_MINMAX("animation_speed", float, 1.0f, 0.1f, 3.0f);
        PropertyPointer<Plugins::Color> color1 = MAKE_PROPERTY("color1", Plugins::Color, Plugins::Color(0xFF0000));
        PropertyPointer<Plugins::Color> color2 = MAKE_PROPERTY("color2", Plugins::Color, Plugins::Color(0x0000FF));
        PropertyPointer<bool> smooth_animation = MAKE_PROPERTY("smooth_animation", bool, true);
        
        // Animation state
        float time = 0.0f;
        
        // Demo functions for different rendering techniques
        void renderPixelManipulation();
        void renderGeometricPatterns();
        void renderColorInterpolation();
        void renderParticleSystem();
        void renderMathematicalVisualization();
        
        // Utility functions
        void setPixelSafe(int x, int y, uint8_t r, uint8_t g, uint8_t b);
        void drawCircle(int center_x, int center_y, int radius, uint8_t r, uint8_t g, uint8_t b);
        void drawLine(int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b);
        rgb_matrix::Color interpolateColors(const rgb_matrix::Color& c1, const rgb_matrix::Color& c2, float t);
        void hsv_to_rgb(float h, float s, float v, uint8_t& r, uint8_t& g, uint8_t& b);
        
        struct Particle {
            float x, y, vx, vy;
            float life, max_life;
            rgb_matrix::Color color;
        };
        std::vector<Particle> particles;
        
    public:
        RenderingDemoScene();
        ~RenderingDemoScene() override = default;

        bool render(rgb_matrix::RGBMatrixBase *matrix) override;
        std::string get_name() const override { return "rendering_demo"; }
        void register_properties() override;
        void initialize(rgb_matrix::RGBMatrixBase *matrix, rgb_matrix::FrameCanvas *canvas) override;
        
        tmillis_t get_default_duration() override { return 20000; }
        int get_default_weight() override { return 7; }
    };

    class RenderingDemoSceneWrapper : public Plugins::SceneWrapper {
    public:
        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create() override;
    };
}