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
        bool tick(rgb_matrix::RGBMatrix *matrix) override;
        string get_name() const override;

        using Scenes::Scene::Scene;
    };

    class WatermelonPlasmaSceneWrapper : public Plugins::SceneWrapper {
    public:

        Scenes::Scene *create_default() override;

        Scenes::Scene *from_json(const nlohmann::json &json) override;
    };
}