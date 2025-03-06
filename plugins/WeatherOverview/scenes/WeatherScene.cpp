#include <spdlog/spdlog.h>
#include "WeatherScene.h"
#include "../Constants.h"
#include "shared/utils/consts.h"
#include "shared/utils/canvas_image.h"
#include "shared/utils/image_fetch.h"

namespace fs = std::filesystem;

const int MAIN_ICON_SIZE = 42;
const int FORECAST_ICON_SIZE = 16;
const int MAX_SCROLL = 20;
const int SCROLL_PAUSE = 40;
const int ANIMATION_INTERVAL = 100; // milliseconds

struct Images
{
    std::vector<uint8_t> currentIcon;
    std::vector<std::vector<uint8_t>> forecastIcons;
};

std::optional<Images> images;

// Instead of a global should_render flag, track update state inside the scene
std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> Scenes::WeatherSceneWrapper::create()
{
    return {
        new WeatherScene(), [](Scenes::Scene *scene)
        {
            delete scene;
        }};
}

string Scenes::WeatherScene::get_name() const
{
    return "weather";
}

void Scenes::WeatherScene::renderCurrentWeather(RGBMatrixBase *matrix, const WeatherData &data)
{
    // Draw the main weather icon
    if (images.has_value())
    {
        SetImageTransparent(offscreen_canvas, 2, 12,
                            images->currentIcon.data(), images->currentIcon.size(),
                            MAIN_ICON_SIZE, MAIN_ICON_SIZE, 0, 0, 0);
    }

    // Draw temperature in large font
    int temp_x = MAIN_ICON_SIZE + 6;
    int temp_y = 20;
    DrawText(offscreen_canvas, HEADER_FONT, temp_x, temp_y,
             {255, 255, 255}, data.temperature.c_str());

    // Draw weather description with scroll effect if needed
    std::string desc = data.description;
    int desc_width = BODY_FONT.CharacterWidth('A') * desc.length();
    int available_width = matrix->width() - temp_x;

    int desc_y = temp_y + 14;

    if (desc_width > available_width)
    {
        // Handle text scrolling for long descriptions
        int start_pos = temp_x - scroll_position;
        DrawText(offscreen_canvas, BODY_FONT, start_pos, desc_y,
                 {220, 220, 255}, desc.c_str());

        // Update scroll position for next frame
        if (scroll_pause_counter > 0)
        {
            scroll_pause_counter--;
        }
        else
        {
            scroll_position += scroll_direction;
            if (scroll_position >= desc_width - available_width + 5)
            {
                scroll_direction = -1;
                scroll_pause_counter = SCROLL_PAUSE;
            }
            else if (scroll_position <= 0)
            {
                scroll_direction = 1;
                scroll_pause_counter = SCROLL_PAUSE;
            }
        }
    }
    else
    {
        DrawText(offscreen_canvas, BODY_FONT, temp_x, desc_y,
                 {220, 220, 255}, desc.c_str());
    }

    // Draw additional weather info
    int add_info_y = desc_y + 10;
    std::string humidity_info = "Humidity: " + data.humidity;
    DrawText(offscreen_canvas, SMALL_FONT, temp_x, add_info_y,
             {200, 200, 255}, humidity_info.c_str());

    std::string wind_info = "Wind: " + data.wind_speed;
    DrawText(offscreen_canvas, SMALL_FONT, temp_x, add_info_y + 7,
             {200, 200, 255}, wind_info.c_str());
}

void Scenes::WeatherScene::renderForecast(RGBMatrixBase *matrix, const WeatherData &data)
{
    // Draw separator line
    for (int x = 0; x < matrix->width(); x++)
    {
        offscreen_canvas->SetPixel(x, 55, 100, 100, 150);
    }

    // Draw forecast title
    DrawText(offscreen_canvas, SMALL_FONT, 2, 65,
             {255, 255, 255}, "3-Day Forecast:");

    // Draw forecast data
    if (data.forecast.size() >= 3 && images.has_value() && !images->forecastIcons.empty())
    {
        const int forecast_width = matrix->width() / 3;

        for (int i = 0; i < std::min(size_t(3), data.forecast.size()); i++)
        {
            const auto &day = data.forecast[i];
            int base_x = i * forecast_width;

            // Draw day name
            DrawText(offscreen_canvas, SMALL_FONT, base_x + 2, 78,
                     {255, 255, 255}, day.day_name.c_str());

            // Draw forecast icon
            if (i < images->forecastIcons.size() && !images->forecastIcons[i].empty())
            {
                SetImageTransparent(offscreen_canvas, base_x + (forecast_width - FORECAST_ICON_SIZE) / 2, 81,
                                    images->forecastIcons[i].data(), images->forecastIcons[i].size(),
                                    FORECAST_ICON_SIZE, FORECAST_ICON_SIZE, 0, 0, 0);
            }

            // Draw min/max temperature
            std::string temp = day.temperature_min + "/" + day.temperature_max;
            int temp_width = SMALL_FONT.CharacterWidth('A') * temp.length();
            DrawText(offscreen_canvas, SMALL_FONT, base_x + (forecast_width - temp_width) / 2, 100,
                     {220, 220, 255}, temp.c_str());
        }
    }
}

bool Scenes::WeatherScene::render(RGBMatrixBase *matrix)
{
    auto data_res = parser->get_data();
    if (!data_res)
    {
        spdlog::warn("Could not get weather data: {}", data_res.error());
        // Instead of returning false, show an error message and continue
        offscreen_canvas->Clear();
        DrawText(offscreen_canvas, BODY_FONT, 2, BODY_FONT.baseline() + 5,
                 {255, 100, 100}, "Weather data error");
        DrawText(offscreen_canvas, SMALL_FONT, 2, BODY_FONT.baseline() + 15,
                 {200, 200, 200}, data_res.error().c_str());
        offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas, 1);
        return true; // Continue running despite error
    }

    auto data = data_res.value();
    bool should_update_display = false;

    // Check if we need to reload images
    if (parser->has_changed() || !images.has_value())
    {
        should_update_display = true;
        const auto weather_dir_path = fs::path(weather_dir);
        if (!exists(weather_dir_path))
        {
            fs::create_directory(weather_dir);
        }

        // Download and process current weather icon
        string file_path = weather_dir_path / ("weather_icon" + std::to_string(data.weatherCode) + ".png");
        fs::path processed_img = to_processed_path(file_path);

        if (!fs::exists(processed_img) && !data.icon_url.empty())
        {
            try_remove(file_path);
            auto res = utils::download_image(data.icon_url, file_path);
            if (!res)
            {
                spdlog::warn("Could not download main image {}", res.error());
            }
        }

        bool contain_img = true;
        auto res = LoadImageAndScale(file_path, MAIN_ICON_SIZE, MAIN_ICON_SIZE, true, true, contain_img, true);

        Images img;

        if (res)
        {
            img.currentIcon = magick_to_rgb(res.value().at(0));
            try_remove(file_path);
        }
        else
        {
            spdlog::error("Error loading main image: {}", res.error());
            // Don't return false here, continue with empty icon
        }

        // Process forecast icons
        for (const auto &forecast_day : data.forecast)
        {
            if (!forecast_day.icon_url.empty())
            {
                string forecast_file = weather_dir_path / ("forecast_icon" +
                                                           std::to_string(forecast_day.weatherCode) + ".png");
                fs::path forecast_processed = to_processed_path(forecast_file);

                if (!fs::exists(forecast_processed))
                {
                    try_remove(forecast_file);
                    auto dl_res = utils::download_image(forecast_day.icon_url, forecast_file);
                    if (!dl_res)
                    {
                        spdlog::warn("Could not download forecast image {}", dl_res.error());
                        img.forecastIcons.push_back(std::vector<uint8_t>{}); // Empty placeholder
                        continue;
                    }
                }

                auto f_res = LoadImageAndScale(forecast_file, FORECAST_ICON_SIZE, FORECAST_ICON_SIZE,
                                               true, true, contain_img, true);

                if (f_res)
                {
                    img.forecastIcons.push_back(magick_to_rgb(f_res.value().at(0)));
                }
                else
                {
                    spdlog::warn("Could not load forecast image: {}", f_res.error());
                    img.forecastIcons.push_back(std::vector<uint8_t>{}); // Empty placeholder
                }

                try_remove(forecast_file);
            }
            else
            {
                img.forecastIcons.push_back(std::vector<uint8_t>{}); // Empty placeholder
            }
        }

        parser->unmark_changed();
        images = img;
    }

    // Check if animation frame needs to be updated
    tmillis_t current_time = GetTimeInMillis();
    if (current_time - last_animation_time > ANIMATION_INTERVAL)
    {
        animation_frame = (animation_frame + 1) % 60; // 60 frames for subtle animations
        last_animation_time = current_time;
        should_update_display = true;
    }

    // Always update on scroll position changes
    if (scroll_position > 0 || scroll_direction > 0)
    {
        should_update_display = true;
    }

    // Update the display if needed
    if (should_update_display)
    {
        offscreen_canvas->Clear();
        offscreen_canvas->Fill(data.color.r, data.color.g, data.color.b);

        // Render all components
        renderCurrentWeather(matrix, data);
        renderForecast(matrix, data);

        // Add a subtle animation effect - slight color pulsing in the background
        int pulse = (animation_frame < 30) ? animation_frame : 60 - animation_frame;
        int pulse_intensity = pulse / 6;

        // Apply subtle overlay
        for (int x = 0; x < matrix->width(); x += 4)
        {
            for (int y = 10; y < 50; y += 4)
            {
                offscreen_canvas->SetPixel(x, y,
                                           std::min(255, data.color.r + pulse_intensity),
                                           std::min(255, data.color.g + pulse_intensity),
                                           std::min(255, data.color.b + pulse_intensity));
            }
        }

        offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas, 1);
    }

    return true; // Always return true to keep the scene running
}

void Scenes::WeatherScene::after_render_stop(RGBMatrixBase *matrix)
{
    // Reset scroll position when stopping render
    scroll_position = 0;
    scroll_direction = 1;
    scroll_pause_counter = 0;
    Scene::after_render_stop(matrix);
}
