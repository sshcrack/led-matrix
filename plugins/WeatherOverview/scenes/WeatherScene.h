#pragma once

#include "Scene.h"
#include "wrappers.h"
#include "../WeatherParser.h"

namespace Scenes {
    class WeatherScene : public Scene {
    private:
        int scroll_position = 0;
        int scroll_direction = 1;
        int scroll_pause_counter = 0;
        bool animation_active = false;
        tmillis_t last_animation_time = 0;
        int animation_frame = 0;
        
        void renderCurrentWeather(RGBMatrixBase *matrix, const WeatherData &data);
        void renderForecast(RGBMatrixBase *matrix, const WeatherData &data);
        
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

