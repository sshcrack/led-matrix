#pragma once

#include "Scene.h"
#include "wrappers.h"

namespace Scenes {
    class WeatherScene : public Scene {
    public:
        bool tick(RGBMatrix *matrix) override;
        [[nodiscard]] string get_name() const override;

        using Scene::Scene;
    };


    class WeatherSceneWrapper : public Plugins::SceneWrapper {
        Scenes::Scene *create_default() override;

        Scenes::Scene *from_json(const nlohmann::json &args) override;
    };
}

