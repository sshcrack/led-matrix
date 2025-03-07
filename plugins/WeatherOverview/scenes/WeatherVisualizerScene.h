#pragma once

#include "Scene.h"
#include "plugin/main.h"
#include "../WeatherParser.h"
#include "../Constants.h"
#include "../elements/WeatherElements.h"
#include "../rendering/WeatherRenderer.h"
#include <vector>
#include <random>
#include <chrono>

namespace Scenes
{
    class WeatherVisualizerScene : public Scenes::Scene
    {
    private:
        // Weather data
        std::optional<WeatherData> current_weather;
        bool is_day = true;
        int weather_code = 0;

        // Properties
        PropertyPointer<int> update_interval_ms = MAKE_PROPERTY("update_interval_ms", int, 60000); // Update weather every minute
        PropertyPointer<float> animation_speed = MAKE_PROPERTY("animation_speed", float, 1.0f);

        // Animation state
        std::chrono::time_point<std::chrono::steady_clock> last_weather_update;
        std::chrono::time_point<std::chrono::steady_clock> start_time;
        float elapsed_time = 0.0f;

        // Weather element containers
        std::vector<WeatherElements::Star> stars;
        std::vector<WeatherElements::Raindrop> raindrops;
        std::vector<WeatherElements::Snowflake> snowflakes;
        std::vector<WeatherElements::Cloud> clouds;
        std::vector<WeatherElements::LightningBolt> lightning_bolts;

        // RNG
        std::mt19937 rng;

        // Helper methods
        void update_weather();

    public:
        explicit WeatherVisualizerScene();
        ~WeatherVisualizerScene() override = default;

        bool render(RGBMatrixBase *matrix) override;
        void initialize(RGBMatrixBase *matrix, rgb_matrix::FrameCanvas *l_offscreen_canvas) override;
        [[nodiscard]] std::string get_name() const override;
        void register_properties() override;
    };

    class WeatherVisualizerSceneWrapper final : public Plugins::SceneWrapper
    {
        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create() override;
    };
}
