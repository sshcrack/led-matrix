#pragma once

#include "Scene.h"
#include "wrappers.h"
#include "../WeatherParser.h"

namespace Scenes {
    class WeatherScene : public Scene {
    public:
        bool render(RGBMatrixBase *matrix) override;

        [[nodiscard]] string get_name() const override;

        void after_render_stop(RGBMatrixBase *matrix) override;

        void register_properties() override {}

        using Scene::Scene;
        ~WeatherScene() override = default;
    };


    class WeatherSceneWrapper : public Plugins::SceneWrapper {
        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create() override;
    };
}

