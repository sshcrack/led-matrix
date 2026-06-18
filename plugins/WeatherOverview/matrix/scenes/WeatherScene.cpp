#include "spdlog/spdlog.h"
#include "WeatherScene.h"
#include "../Constants.h"
#include "shared/matrix/utils/canvas_image.h"
#include <iostream>
#ifdef ENABLE_EMULATOR
#include "emulator.h"
#endif

std::unique_ptr<Scenes::Scene> Scenes::WeatherSceneWrapper::create()
{
    return std::make_unique<WeatherScene>();
}

string Scenes::WeatherScene::get_name() const
{
    return "weather";
}

void Scenes::WeatherScene::renderCurrentWeather(rgb_matrix::FrameCanvas *canvas, const WeatherData &data)
{
    if (images.has_value())
    {
        SetImageTransparent(canvas, 2, 12,
                            images->currentIcon);
    }

    constexpr int temp_x = MAIN_ICON_SIZE + 6;
    constexpr int temp_y = 20;
    DrawText(canvas, HEADER_FONT, temp_x, temp_y,
             {255, 255, 255}, data.temperature.c_str());

    const std::string desc = data.description;
    const int desc_y = temp_y + 14;

    DrawText(canvas, BODY_FONT, temp_x, desc_y,
             {220, 220, 255}, desc.c_str());

    constexpr int add_info_y = desc_y + 10;
    const std::string humidity_info = "Humidity: " + data.humidity;
    DrawText(canvas, SMALL_FONT, temp_x, add_info_y,
             {200, 200, 255}, humidity_info.c_str());

    const std::string wind_info = "Wind: " + data.wind_speed;
    DrawText(canvas, SMALL_FONT, temp_x, add_info_y + 7,
             {200, 200, 255}, wind_info.c_str());
}

void Scenes::WeatherScene::renderForecast(rgb_matrix::FrameCanvas *canvas, const WeatherData &data) const
{
    int base_offset_x = 5;

    DrawText(canvas, SMALL_FONT, base_offset_x, 65,
             {255, 255, 255}, "3-Day Forecast:");

    if (data.forecast.size() >= 3 && images.has_value())
    {
        const int forecast_width = matrix_width / 3;

        for (int i = 0; i < std::min(size_t(3), data.forecast.size()); i++)
        {
            const auto &day = data.forecast[i];
            const int base_x = i * forecast_width + base_offset_x;

            DrawText(canvas, SMALL_FONT, base_x + 2, 78,
                     {255, 255, 255}, day.day_name.c_str());

            if (i < images->forecastIcons.size())
            {
                SetImageTransparent(canvas, base_x + (forecast_width - FORECAST_ICON_SIZE) / 2 - 7, 79,
                                    images->forecastIcons[i]);
            }

            std::string temp = day.temperature_min + "/" + day.temperature_max;
            const int temp_width = SMALL_FONT.CharacterWidth('A') * temp.length();
            DrawText(canvas, SMALL_FONT, base_x + (forecast_width - temp_width) / 2, 100,
                     {220, 220, 255}, temp.c_str());

            if (day.precipitation_chance > 0.1f)
            {
                const int indicator_x = base_x + forecast_width - 8;
                const int indicator_y = 95;

                drawPrecipitationIndicator(canvas, day.precipitation_chance, indicator_x, indicator_y);

                std::string prob = std::to_string(static_cast<int>(day.precipitation_chance * 100)) + "%";
                DrawText(canvas, SMALL_FONT, base_x + 10, 110,
                         {150, 200, 255}, prob.c_str());
            }
        }
    }
}

void Scenes::WeatherScene::renderSunriseSunset(rgb_matrix::FrameCanvas *canvas, const WeatherData &data) const
{
    if (data.sunrise.empty() || data.sunset.empty() || !show_sunrise_sunset->get())
    {
        return;
    }

    constexpr int icon_size = 5;
    constexpr int base_x = 10;
    constexpr int base_y = 55;

    for (int i = -1; i <= 1; i++)
    {
        for (int j = -1; j <= 1; j++)
        {
            if (i == 0 || j == 0)
            {
                canvas->SetPixel(base_x + i, base_y + j, 255, 200, 50);
            }
        }
    }
    canvas->SetPixel(base_x, base_y, 255, 220, 100);

    std::string sunrise_text = "\u2191 " + data.sunrise;
    DrawText(canvas, SMALL_FONT, base_x + icon_size + 2, base_y + 2,
             {255, 220, 100}, sunrise_text.c_str());

    const int sunset_x = matrix_width / 2 + 20;
    for (int i = -1; i <= 1; i++)
    {
        for (int j = -1; j <= 1; j++)
        {
            if (i == 0 || j == 0)
            {
                canvas->SetPixel(sunset_x + i, base_y + j, 255, 150, 50);
            }
        }
    }
    canvas->SetPixel(sunset_x, base_y, 255, 180, 80);

    std::string sunset_text = "\u2193 " + data.sunset;
    DrawText(canvas, SMALL_FONT, sunset_x + icon_size + 2, base_y + 2,
             {255, 180, 80}, sunset_text.c_str());
}

void Scenes::WeatherScene::renderClock(rgb_matrix::FrameCanvas *canvas) const
{
    const time_t timestamp = time(nullptr);
    tm local_time_storage{};
    const tm datetime = *localtime_r(&timestamp, &local_time_storage);

    char output[50];
    strftime(output, 50, "%H:%M", &datetime);

    rgb_matrix::DrawText(canvas, BODY_FONT, 98, 11, {255, 255, 255}, output);
}

void Scenes::WeatherScene::updateAnimationState(const WeatherData &data)
{
    has_precipitation = false;

    if (!enable_animations->get())
    {
        return;
    }

    if (particles.empty())
    {
        initializeParticles();
    }

    updateEnhancedParticles(data);
}

void Scenes::WeatherScene::renderAnimations(rgb_matrix::FrameCanvas *canvas, const WeatherData &data)
{
    if (!enable_animations->get())
    {
        return;
    }

    renderEnhancedParticles(canvas, data);

    renderClouds(canvas, data);
    renderLightning(canvas);
    renderSunRays(canvas, data);
    renderFogMist(canvas, data);
    renderAurora(canvas);
}

bool Scenes::WeatherScene::render(rgb_matrix::FrameCanvas *canvas)
{
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_weather_fetch).count();

    if (elapsed >= weather_refresh_interval_seconds)
    {
        auto data_res = parser.get_data(location_lat->get(), location_lon->get());
        if (!data_res)
        {
            spdlog::warn("Could not get weather data: {}", data_res.error());
            canvas->Clear();
            DrawText(canvas, BODY_FONT, 2, BODY_FONT.baseline() + 5,
                     {255, 100, 100}, "Weather data error");
            DrawText(canvas, SMALL_FONT, 2, BODY_FONT.baseline() + 15,
                     {200, 200, 200}, data_res.error().c_str());

#ifdef ENABLE_EMULATOR
            ((rgb_matrix::EmulatorMatrix *)canvas)->Render();
#endif
            SleepMillis(1000);
            return false;
        }

        data = data_res.value();
        last_weather_fetch = now;
    }

    RGB theme_color = getThemeColor(color_theme->get().get(), data);

    if (parser.has_changed() || !images.has_value())
    {
        reloadImages();
    }

    animation_frame = (animation_frame + 1) % get_target_fps();

    updateEnhancedParticles(data);
    canvas->Clear();

    if (gradient_background->get())
    {
        applyBackgroundEffects(canvas, theme_color);
    }
    else
    {
        canvas->Fill(theme_color.r, theme_color.g, theme_color.b);
    }

    renderRainbowEffect(canvas, data);
    if (show_border->get())
    {
        drawWeatherBorder(canvas, theme_color, 40);
    }

    if (enable_clock->get())
        renderClock(canvas);

    renderCurrentWeather(canvas, data);
    renderSunriseSunset(canvas, data);
    renderForecast(canvas, data);

    if (enable_animations->get())
    {
        renderAnimations(canvas, data);
    }

    wait_until_next_frame();
    return true;
}

void Scenes::WeatherScene::after_render_stop()
{
    if (reset_stars_on_exit->get())
    {
        resetStars();
        for (auto &star : shooting_stars)
        {
            star.active = false;
        }
    }
    Scene::after_render_stop();
}
