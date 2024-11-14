#pragma once

#include "Scene.h"
#include "shared/matrix.h"
#include "wrappers.h"
#include "shared/utils/FrameTimer.h"

using namespace Scenes;

namespace Scenes {
    class WaveScene : public Scene {
    private:
        FrameTimer frameTimer;

        void drawMap(ProxyMatrix *matrix, float *iMap);

    public:
        bool render(ProxyMatrix *matrix) override;
        using Scene::Scene::Scene;

        void initialize(ProxyMatrix *matrix) override;
        [[nodiscard]] string get_name() const override;
    };


    class WaveSceneWrapper : public Plugins::SceneWrapper {
    public:

        Scenes::Scene * create_default() override;
        Scenes::Scene * from_json(const nlohmann::json &args) override;
    };
}