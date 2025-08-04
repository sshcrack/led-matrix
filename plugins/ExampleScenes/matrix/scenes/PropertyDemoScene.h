#pragma once

#include "shared/matrix/Scene.h"
#include "shared/matrix/wrappers.h"
#include "shared/matrix/utils/FrameTimer.h"
#include "shared/matrix/plugin/color.h"

namespace Scenes {
    class PropertyDemoScene : public Scene {
    private:
        FrameTimer frameTimer;
        
        // Demonstrate all property types
        PropertyPointer<int> number_demo = MAKE_PROPERTY_MINMAX("number", int, 5, 1, 10);
        PropertyPointer<float> float_demo = MAKE_PROPERTY_MINMAX("float_value", float, 1.5f, 0.1f, 5.0f);
        PropertyPointer<bool> boolean_demo = MAKE_PROPERTY("enable_feature", bool, true);
        PropertyPointer<std::string> string_demo = MAKE_PROPERTY("text_message", std::string, "Hello LED Matrix!");
        PropertyPointer<Plugins::Color> color_demo = MAKE_PROPERTY("primary_color", Plugins::Color, Plugins::Color(0x00FF00));
        PropertyPointer<Plugins::Color> secondary_color = MAKE_PROPERTY("secondary_color", Plugins::Color, Plugins::Color(0xFF0000));
        
        // Required property example
        PropertyPointer<std::string> required_demo = MAKE_PROPERTY_REQ("required_setting", std::string, "must_be_set");
        
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