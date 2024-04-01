#pragma once

#include "Scene.h"
#include "wrappers.h"
#include "shared/utils/FrameTimer.h"

using namespace Scenes;

namespace Scenes {
    class WaveScene : public Scene {
    private:
        FrameTimer frameTimer;

        void drawMap(rgb_matrix::RGBMatrix *matrix, float *iMap);

    public:
        bool tick(rgb_matrix::RGBMatrix *matrix) override;
        using Scene::Scene::Scene;

        void initialize(rgb_matrix::RGBMatrix *matrix) override;
    };


    class WaveSceneWrapper : public Plugins::SceneWrapper {
    public:
        string get_name() override;

        Scenes::Scene * create_default() override;
        Scenes::Scene * from_json(const nlohmann::json &args) override;
    };
}