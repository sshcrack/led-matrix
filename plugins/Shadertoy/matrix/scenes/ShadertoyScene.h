#pragma once

#include "shared/matrix/Scene.h"
#include "shared/matrix/wrappers.h"
#include "shared/matrix/utils/FrameTimer.h"
#include "shared/matrix/plugin/color.h"
#include "../ShadertoyPlugin.h"

namespace Scenes {
    class ShadertoyScene : public Scene {
        // Make lastUrlSent a static member variable, declared here
        static std::string lastUrlSent;
        ShadertoyPlugin* plugin;

    public:
        ShadertoyScene();
        ~ShadertoyScene() override = default;

        bool render(rgb_matrix::RGBMatrixBase *matrix) override;
        string get_name() const override;
        void register_properties() override;
        
        tmillis_t get_default_duration() override {
            return 20000;
        }
        
        int get_default_weight() override {
            return 5;
        }

        // Properties for the scene
        PropertyPointer<std::string> toy_url = MAKE_PROPERTY("url", std::string, "https://www.shadertoy.com/view/33cGDj");
    };

    class ShadertoySceneWrapper : public Plugins::SceneWrapper {
    public:
        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create() override;
    };
}
