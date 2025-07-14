#pragma once

#include "Scene.h"
#include "wrappers.h"
#include "shared/utils/FrameTimer.h"

namespace Scenes {
    class ColorPulseScene : public Scene {
    private:
        FrameTimer frameTimer;
        Property<float> pulseSpeed;
        Property<int> colorMode;
        
    public:
        ColorPulseScene() : 
            pulseSpeed("speed", 1.0f),
            colorMode("color_mode", 0) {}
            
        ~ColorPulseScene() override = default;
        bool render(rgb_matrix::RGBMatrix *matrix) override;
        string get_name() const override;
        void register_properties() override;
    };

    class ColorPulseSceneWrapper : public Plugins::SceneWrapper {
    public:
        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create() override;
    };
}
