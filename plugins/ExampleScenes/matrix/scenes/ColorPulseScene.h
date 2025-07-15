#pragma once

#include "shared/matrix/Scene.h"
#include "shared/matrix/wrappers.h"
#include "shared/matrix/utils/FrameTimer.h"

namespace Scenes {
    class ColorPulseScene : public Scene {
    private:
        FrameTimer frameTimer;
        PropertyPointer<float> pulseSpeed = MAKE_PROPERTY("speed", float, 1.0f);
        PropertyPointer<int> colorMode = MAKE_PROPERTY("color_mode", int, 0);
        
    public:
        ColorPulseScene() {}
            
        ~ColorPulseScene() override = default;
        bool render(RGBMatrixBase *matrix) override;
        string get_name() const override;
        void register_properties() override;

        tmillis_t get_default_duration() override;
        int get_default_weight() override;
    };

    class ColorPulseSceneWrapper : public Plugins::SceneWrapper {
    public:
        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create() override;
    };
}
