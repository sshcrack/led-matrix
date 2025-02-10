#pragma once

#include "Scene.h"
#include "wrappers.h"
#include "../WeatherParser.h"

namespace Scenes {
    class WeatherScene : public Scene {
    public:
        bool render(RGBMatrix *matrix) override;

        [[nodiscard]] string get_name() const override;

        void after_render_stop(rgb_matrix::RGBMatrix *matrix) override;

        void register_properties() override {}

        using Scene::Scene;
    };


    class WeatherSceneWrapper : public Plugins::SceneWrapper {
        Scenes::Scene *create() override;
    };
}

