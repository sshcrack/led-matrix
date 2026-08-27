#ifdef _WIN32
#include "shared/common/win_compat.h"
#endif
#include "spdlog/spdlog.h"
#include "WeatherScene.h"
#include "matrix/Constants.h"
#include "shared/matrix/utils/canvas_image.h"
#include "shared/matrix/utils/utils.h"
#include <algorithm>
#include <ctime>
#ifdef _WIN32
inline std::tm* compat_localtime_r(const std::time_t* t, std::tm* out) { return localtime_s(out, t) == 0 ? out : nullptr; }
#else
inline std::tm* compat_localtime_r(const std::time_t* t, std::tm* out) { return localtime_r(t, out); }
#endif
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

void Scenes::WeatherScene::renderCurrentWeather(rgb_matrix::FrameCanvas *canvas, const WeatherData &data, const RGB &theme_color)
{
    // Blend the panel into the rendered sky instead of replacing it with an
    // almost-black rectangle. FrameCanvas::GetPixel reads the off-screen canvas
    // backing store here; the renderer regressions are covered separately.
    const RGB panel_tint{
        static_cast<uint8_t>(std::max(8, static_cast<int>(theme_color.r * 0.18f))),
        static_cast<uint8_t>(std::max(18, static_cast<int>(theme_color.g * 0.18f))),
        static_cast<uint8_t>(std::max(34, static_cast<int>(theme_color.b * 0.18f)))
    };
    constexpr float panel_alpha = 0.54f;
    const int card_x = 4, card_y = 16, card_w = matrix_width - 8, card_h = 43;
    for (int y = card_y; y < card_y + card_h && y < matrix_height; ++y)
        for (int x = card_x; x < card_x + card_w && x < matrix_width; ++x)
            SetPixelAlpha(canvas, x, y, panel_tint.r, panel_tint.g, panel_tint.b, panel_alpha);

    if (images.has_value())
        SetImageTransparent(canvas, card_x + 3, card_y + 4, images->currentIcon);

    const int text_x = card_x + MAIN_ICON_SIZE + 8;
    DrawText(canvas, *HEADER_FONT, text_x, card_y + 14, {245, 250, 255}, data.temperature.c_str());
    DrawText(canvas, *BODY_FONT, text_x, card_y + 27, {175, 215, 245}, data.description.c_str());

    const std::string humidity = "H " + data.humidity;
    const std::string wind = "W " + data.wind_speed;
    DrawText(canvas, *SMALL_FONT, text_x, card_y + 38, {135, 185, 225}, humidity.c_str());
    DrawText(canvas, *SMALL_FONT, text_x + 35, card_y + 38, {135, 185, 225}, wind.c_str());

    const float indoor = indoor_temperature.load();
    if (show_indoor_temperature->get() && indoor > -100.0f)
    {
        const std::string indoor_text = "IN " + std::to_string(static_cast<int>(std::round(indoor))) + "C";
        DrawText(canvas, *SMALL_FONT, matrix_width - 34, card_y + 9, {255, 196, 105}, indoor_text.c_str());
    }
}

void Scenes::WeatherScene::renderForecast(rgb_matrix::FrameCanvas *canvas, const WeatherData &data, const RGB &theme_color) const
{
    if (data.forecast.empty() || !images.has_value())
        return;

    const int top = 72;
    const int count = std::min<size_t>(3, data.forecast.size());
    const int gap = 3;
    const int card_w = (matrix_width - 8 - gap * (count - 1)) / count;
    const RGB panel_tint{
        static_cast<uint8_t>(std::max(7, static_cast<int>(theme_color.r * 0.15f))),
        static_cast<uint8_t>(std::max(16, static_cast<int>(theme_color.g * 0.15f))),
        static_cast<uint8_t>(std::max(30, static_cast<int>(theme_color.b * 0.15f)))
    };
    constexpr float panel_alpha = 0.58f;

    for (int i = 0; i < count; ++i)
    {
        const auto &day = data.forecast[i];
        const int x0 = 4 + i * (card_w + gap);
        for (int y = top; y < std::min(matrix_height - 4, top + 47); ++y)
            for (int x = x0; x < std::min(matrix_width, x0 + card_w); ++x)
                SetPixelAlpha(canvas, x, y, panel_tint.r, panel_tint.g, panel_tint.b, panel_alpha);

        DrawText(canvas, *SMALL_FONT, x0 + 4, top + 9, {205, 225, 245}, day.day_name.c_str());
        if (i < static_cast<int>(images->forecastIcons.size()))
            SetImageTransparent(canvas, x0 + (card_w - FORECAST_ICON_SIZE) / 2, top + 11, images->forecastIcons[i]);

        const std::string temp = day.temperature_max + " " + day.temperature_min;
        DrawText(canvas, *SMALL_FONT, x0 + 3, top + 37, {245, 248, 255}, temp.c_str());
        if (day.precipitation_chance > 0.1f)
        {
            const std::string rain = std::to_string(static_cast<int>(day.precipitation_chance * 100)) + "%";
            DrawText(canvas, *SMALL_FONT, x0 + card_w - 18, top + 46, {105, 185, 255}, rain.c_str());
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
    constexpr int base_y = 66;

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
    DrawText(canvas, *SMALL_FONT, base_x + icon_size + 2, base_y + 2,
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
    DrawText(canvas, *SMALL_FONT, sunset_x + icon_size + 2, base_y + 2,
             {255, 180, 80}, sunset_text.c_str());
}

void Scenes::WeatherScene::renderClock(rgb_matrix::FrameCanvas *canvas) const
{
    const time_t timestamp = time(nullptr);
    tm local_time_storage{};
    const tm datetime = *compat_localtime_r(&timestamp, &local_time_storage);

    char output[50];
    strftime(output, 50, "%H:%M", &datetime);

    const int x = std::max(2, matrix_width - 31);
    rgb_matrix::DrawText(canvas, *BODY_FONT, x, 11, {215, 235, 255}, output);
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
            DrawText(canvas, *BODY_FONT, 2, BODY_FONT->baseline() + 5,
                     {255, 100, 100}, "Weather data error");
            DrawText(canvas, *SMALL_FONT, 2, BODY_FONT->baseline() + 15,
                     {200, 200, 200}, data_res.error().c_str());

            SleepMillis(1000);

            return false;
        }

        last_weather_fetch = now;
        data = data_res.value();
    }

    RGB theme_color = getThemeColor(color_theme->get().get(), data);

    if (parser.has_changed() || !images.has_value())
    {
        reloadImages();
    }

    animation_frame = (animation_frame + 1) % std::max(1, get_target_fps());

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

    renderCurrentWeather(canvas, data, theme_color);
    renderSunriseSunset(canvas, data);
    renderForecast(canvas, data, theme_color);

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
