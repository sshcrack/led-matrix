#pragma once

#include "Scene.h"
#include "wrappers.h"
#include "shared/utils/FrameTimer.h"

using Scenes::Scene;
namespace Scenes {
    class WatermelonPlasmaScene : public Scene {
    private:
        FrameTimer frameTimer;
    public:
        ~WatermelonPlasmaScene() override = default;
        bool render(rgb_matrix::RGBMatrix *matrix) override;

        string get_name() const override;

        void register_properties() override {}

        using Scenes::Scene::Scene;
    };

    class WatermelonPlasmaSceneWrapper : public Plugins::SceneWrapper {
    public:
        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create() override;
    };
}