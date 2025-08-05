#pragma once

#include "shared/matrix/Scene.h"
#include "shared/matrix/wrappers.h"
#include "shared/matrix/utils/FrameTimer.h"

namespace Scenes {
    // Demo enums to showcase the enum property system
    enum class AnimationMode {
        STATIC = 0,      // No animation  
        FADE = 1,        // Fade in/out effect
        PULSE = 2,       // Pulsing effect
        RAINBOW = 3,     // Color cycling
        BOUNCE = 4       // Bouncing animation
    };
    
    enum class DisplayPattern {
        SOLID,           // Solid color fill
        STRIPES,         // Horizontal stripes
        CHECKERBOARD,    // Checkerboard pattern
        GRADIENT,        // Gradient effect
        SPIRAL           // Spiral pattern
    };

    class PropertyDemoScene : public Scene {
    private:
        FrameTimer frameTimer;
        
        // Demonstrate all property types
        PropertyPointer<int> number_demo = MAKE_PROPERTY_MINMAX("number", int, 5, 1, 10);
        PropertyPointer<float> float_demo = MAKE_PROPERTY_MINMAX("float_value", float, 1.5f, 0.1f, 5.0f);
        PropertyPointer<bool> boolean_demo = MAKE_PROPERTY("enable_feature", bool, true);
        PropertyPointer<std::string> string_demo = MAKE_PROPERTY("text_message", std::string, "Hello LED Matrix!");
        PropertyPointer<rgb_matrix::Color> color_demo = MAKE_PROPERTY("primary_color", rgb_matrix::Color, rgb_matrix::Color(0x00, 0xFF, 0x00));
        PropertyPointer<rgb_matrix::Color> secondary_color = MAKE_PROPERTY("secondary_color", rgb_matrix::Color, rgb_matrix::Color(0xFF, 0x00, 0x00));
        
        // Required property example
        PropertyPointer<std::string> required_demo = MAKE_PROPERTY_REQ("required_setting", std::string, "must_be_set");
        
        // NEW: Enum property examples
        PropertyPointer<Plugins::EnumProperty<AnimationMode>> animation_mode = MAKE_ENUM_PROPERTY("animation_mode", AnimationMode, AnimationMode::FADE);
        PropertyPointer<Plugins::EnumProperty<DisplayPattern>> display_pattern = MAKE_ENUM_PROPERTY("display_pattern", DisplayPattern, DisplayPattern::SOLID);
        
        // Animation state
        float animation_time = 0.0f;
        
    public:
        PropertyDemoScene() = default;
        ~PropertyDemoScene() override = default;

        bool render(rgb_matrix::RGBMatrixBase *matrix) override;
        std::string get_name() const override { return "property_demo"; }
        void register_properties() override;
        
        tmillis_t get_default_duration() override { return 15000; }
        int get_default_weight() override { return 8; }
    };

    class PropertyDemoSceneWrapper : public Plugins::SceneWrapper {
    public:
        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create() override;
    };
}