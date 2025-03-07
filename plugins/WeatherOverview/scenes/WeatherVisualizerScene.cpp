#include "WeatherVisualizerScene.h"
#include <cmath>
#include <algorithm>
#include <spdlog/spdlog.h>

namespace Scenes {
    WeatherVisualizerScene::WeatherVisualizerScene() : Scene() {
        start_time = std::chrono::steady_clock::now();
        last_weather_update = std::chrono::time_point<std::chrono::steady_clock>::min();

        // Initialize random number generator
        std::random_device rd;
        rng = std::mt19937(rd());
    }

    void WeatherVisualizerScene::initialize(RGBMatrixBase *matrix, rgb_matrix::FrameCanvas *l_offscreen_canvas) {
        Scene::initialize(matrix, l_offscreen_canvas);
        update_weather();
    }

    void WeatherVisualizerScene::register_properties() {
        add_property(update_interval_ms);
        add_property(animation_speed);
    }

    std::string WeatherVisualizerScene::get_name() const {
        return "weather_visualizer";
    }

    void WeatherVisualizerScene::update_weather() {
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_weather_update).count();

        if (duration < (int64_t) update_interval_ms.get() && current_weather.has_value()) {
            return;
        }

        spdlog::debug("Updating weather data for visualization");
        auto weather_result = parser->get_data();
        if (weather_result) {
            current_weather = weather_result.value();
            weather_code = current_weather->weatherCode;

            // Check if it's day or night
            is_day = current_weather->is_day;

            // Clear existing elements
            stars.clear();
            raindrops.clear();
            snowflakes.clear();
            clouds.clear();
            lightning_bolts.clear();

            // Initialize appropriate weather elements based on weather code
            if (!is_day) {
                WeatherRenderer::initialize_stars(stars, 50, matrix_width, matrix_height, rng);
            }

            // Initialize based on weather conditions
            if (weather_code >= 0 && weather_code <= 3) {
                // Clear to partly cloudy
                const int cloud_count = weather_code * 2; // 0, 2, 4, 6 clouds
                WeatherRenderer::initialize_clouds(clouds, cloud_count, matrix_width, matrix_height, rng, animation_speed->get());
            } else if (weather_code >= 45 && weather_code <= 48) {
                // Fog
                WeatherRenderer::initialize_clouds(clouds, 10, matrix_width, matrix_height, rng, animation_speed->get());
            } else if ((weather_code >= 51 && weather_code <= 57) ||
                      (weather_code >= 61 && weather_code <= 67) ||
                      (weather_code >= 80 && weather_code <= 82)) {
                // Rain or drizzle
                int intensity = 30;
                if (weather_code == 53 || weather_code == 63 || weather_code == 81)
                    intensity = 60;
                else if (weather_code == 55 || weather_code == 65 || weather_code == 82)
                    intensity = 90;

                WeatherRenderer::initialize_raindrops(raindrops, intensity, matrix_width, matrix_height, rng, animation_speed->get());
                WeatherRenderer::initialize_clouds(clouds, 8, matrix_width, matrix_height, rng, animation_speed->get());
            } else if ((weather_code >= 71 && weather_code <= 77) ||
                      (weather_code >= 85 && weather_code <= 86)) {
                // Snow
                int intensity = 30;
                if (weather_code == 73 || weather_code == 86)
                    intensity = 60;
                else if (weather_code == 75)
                    intensity = 90;

                WeatherRenderer::initialize_snowflakes(snowflakes, intensity, matrix_width, matrix_height, rng, animation_speed->get());
                WeatherRenderer::initialize_clouds(clouds, 8, matrix_width, matrix_height, rng, animation_speed->get());
            } else if (weather_code >= 95) {
                // Thunderstorm
                WeatherRenderer::initialize_raindrops(raindrops, 80, matrix_width, matrix_height, rng, animation_speed->get());
                WeatherRenderer::initialize_clouds(clouds, 10, matrix_width, matrix_height, rng, animation_speed->get());
                lightning_bolts.push_back({static_cast<float>(rand() % matrix_width), 0, 500, false});
                lightning_bolts.push_back({static_cast<float>(rand() % matrix_width), 0, 500, false});
            } else {
                // Default - partly cloudy
                WeatherRenderer::initialize_clouds(clouds, 4, matrix_width, matrix_height, rng, animation_speed->get());
            }
        }

        last_weather_update = now;
    }

    bool WeatherVisualizerScene::render(RGBMatrixBase *matrix) {
        update_weather();

        auto now = std::chrono::steady_clock::now();
        float delta_time = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count() / 1000.0f;
        elapsed_time = delta_time * animation_speed->get();

        // Clear the canvas with the sky color
        RGB sky_color = WeatherRenderer::get_sky_color(is_day, weather_code);
        for (int y = 0; y < matrix_height; y++) {
            for (int x = 0; x < matrix_width; x++) {
                offscreen_canvas->SetPixel(x, y, sky_color.r, sky_color.g, sky_color.b);
            }
        }

        // Render weather based on weather code
        if (weather_code >= 0 && weather_code <= 1) {
            // Clear sky
            if (is_day) {
                WeatherRenderer::render_clear_day(offscreen_canvas, matrix_width, matrix_height, elapsed_time);
            } else {
                WeatherRenderer::render_clear_night(offscreen_canvas, stars, matrix_width, matrix_height, elapsed_time);
            }
        } else if (weather_code <= 3) {
            // Partly cloudy
            WeatherRenderer::render_cloudy(offscreen_canvas, clouds, matrix_width, matrix_height, is_day, weather_code * 2, elapsed_time);
        } else if (weather_code >= 45 && weather_code <= 48) {
            // Fog
            WeatherRenderer::render_fog(offscreen_canvas, clouds, matrix_width, matrix_height);
        } else if ((weather_code >= 51 && weather_code <= 57) ||
                  (weather_code >= 61 && weather_code <= 67) ||
                  (weather_code >= 80 && weather_code <= 82)) {
            // Rain or drizzle
            int intensity = 30;
            if (weather_code == 53 || weather_code == 63 || weather_code == 81)
                intensity = 60;
            else if (weather_code == 55 || weather_code == 65 || weather_code == 82)
                intensity = 90;

            WeatherRenderer::render_rain(offscreen_canvas, clouds, raindrops, matrix_width, matrix_height, intensity);
        } else if ((weather_code >= 71 && weather_code <= 77) ||
                  (weather_code >= 85 && weather_code <= 86)) {
            // Snow
            int intensity = 30;
            if (weather_code == 73 || weather_code == 86)
                intensity = 60;
            else if (weather_code == 75)
                intensity = 90;

            WeatherRenderer::render_snow(offscreen_canvas, clouds, snowflakes, matrix_width, matrix_height, intensity);
        } else if (weather_code >= 95) {
            // Thunderstorm
            WeatherRenderer::render_thunderstorm(offscreen_canvas, clouds, raindrops, lightning_bolts, 
                                               matrix_width, matrix_height, elapsed_time, start_time, rng);
        } else {
            // Default - partly cloudy
            WeatherRenderer::render_cloudy(offscreen_canvas, clouds, matrix_width, matrix_height, is_day, 4, elapsed_time);
        }

        // Swap the canvas
        offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas);
        return true;
    }

    std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> WeatherVisualizerSceneWrapper::create() {
        return {
            new WeatherVisualizerScene(), [](Scenes::Scene *p) { delete static_cast<WeatherVisualizerScene *>(p); }
        };
    }
}
