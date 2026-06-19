#include "WeatherScene.h"
#include "../Constants.h"
#include <cmath>
#include <cstring>
#include <shared/matrix/utils/color.h>
#include <shared/matrix/utils/utils.h>

void Scenes::WeatherScene::renderClouds(rgb_matrix::FrameCanvas *canvas, const WeatherData &data)
{
    if (!enable_animations->get())
        return;

    // Initialize cloud layers if empty
    if (cloud_layers.empty())
    {
        std::uniform_int_distribution<int> xcld(0, matrix_width - 1);
        std::uniform_int_distribution<int> ycld(10, 29);
        for (int i = 0; i < 3; i++)
        {
            cloud_layers.push_back({static_cast<float>(xcld(rng)),
                                    static_cast<float>(ycld(rng))});
        }
    }

    // Update cloud animation
    cloud_phase += 0.02f * animation_speed_multiplier->get();

    // Render moving clouds for cloudy weather
    int code = data.weatherCode;
    bool is_cloudy =
        (code >= 0 && code <= 3) ||
        (code >= 80 && code <= 84);
    if (is_cloudy)
    {
        for (size_t i = 0; i < cloud_layers.size(); i++)
        {
            auto &cloud = cloud_layers[i];

            cloud.first += 0.1f * animation_speed_multiplier->get();
            if (cloud.first > matrix_width + 20)
            {
                cloud.first = -20;
                cloud.second = std::uniform_int_distribution<int>(10, 29)(rng);
            }

            float cloud_intensity = 0.18f + 0.12f * sin(cloud_phase + i);
            int cloud_width = 16 + i * 6;
            int cloud_height = 7 + i * 3;
            int num_ellipses = 4 + i;

            for (int e = 0; e < num_ellipses; ++e)
            {
                float ellipse_x = cloud.first + (e - num_ellipses / 2.0f) * (cloud_width / (num_ellipses + 1)) + sin(cloud_phase * 0.7f + e) * 2.0f;
                float ellipse_y = cloud.second + sin(cloud_phase * 0.9f + e * 1.3f) * 1.5f + e;
                float ellipse_w = cloud_width * (0.8f + 0.1f * e / (float)num_ellipses);
                float ellipse_h = cloud_height * (0.8f + 0.1f * e / (float)num_ellipses);

                for (int x = 0; x < static_cast<int>(ellipse_w); x++)
                {
                    for (int y = 0; y < static_cast<int>(ellipse_h); y++)
                    {
                        int px = static_cast<int>(ellipse_x) + x - ellipse_w / 2;
                        int py = static_cast<int>(ellipse_y) + y - ellipse_h / 2;

                        if (px >= 0 && px < matrix_width && py >= 0 && py < matrix_height)
                        {
                            float dx = (x - ellipse_w / 2.0f) / (ellipse_w / 2.0f);
                            float dy = (y - ellipse_h / 2.0f) / (ellipse_h / 2.0f);
                            float dist = dx * dx + dy * dy;
                            float edge_factor = std::max(0.0f, 1.0f - dist);

                            uint8_t base_gray = 185 + static_cast<uint8_t>(25 * edge_factor) + static_cast<uint8_t>(8 * i);
                            uint8_t blue_tint = base_gray + 22 + static_cast<uint8_t>(5 * sin(cloud_phase + e));
                            float alpha = cloud_intensity * edge_factor * (0.7f + 0.3f * (1.0f - i / (float)cloud_layers.size()));

                            SetPixelAlpha(canvas, px, py, base_gray, base_gray, blue_tint, alpha);
                        }
                    }
                }
            }
        }
    }
}

void Scenes::WeatherScene::renderLightning(rgb_matrix::FrameCanvas *canvas)
{
    if (!enable_lightning->get() || !enable_animations->get())
        return;

    int weather_code = data.weatherCode;
    bool is_thunderstorm =
        weather_code == 17 ||
        weather_code == 29 ||
        (weather_code >= 95 && weather_code <= 99);

    if (!is_thunderstorm)
    {
        lightning_active = false;
        lightning_timer = 0;
        return;
    }

    if (lightning_timer > 0)
    {
        lightning_timer--;

        if (lightning_active && lightning_timer > 180)
        {
            for (int y = 0; y < matrix_height; y++)
            {
                for (int x = 0; x < matrix_width; x++)
                {
                    canvas->SetPixel(x, y, 255, 255, 200);
                }
            }
        }
        else if (lightning_active && lightning_timer > 170)
        {
            for (const auto &point : lightning_points)
            {
                if (point.first >= 0 && point.first < matrix_width &&
                    point.second >= 0 && point.second < matrix_height)
                {
                    canvas->SetPixel(point.first, point.second, 200, 200, 255);
                }
            }
        }

        if (lightning_timer <= 0)
        {
            lightning_active = false;
        }
    }

    if (!lightning_active && std::uniform_int_distribution<int>(0, 1799)(rng) == 0)
    {
        lightning_active = true;
        lightning_timer = 200;

        lightning_points.clear();
        std::uniform_int_distribution<int> x_start(0, matrix_width - 1);
        int x = x_start(rng);
        int y = 0;

        while (y < matrix_height)
        {
            lightning_points.push_back({x, y});
            y += 1 + std::uniform_int_distribution<int>(1, 3)(rng);
            x += std::uniform_int_distribution<int>(-1, 1)(rng);
            x = std::max(0, std::min(matrix_width - 1, x));
        }
    }
}

void Scenes::WeatherScene::renderSunRays(rgb_matrix::FrameCanvas *canvas, const WeatherData &data)
{
    if (!enable_sun_rays->get() || !enable_animations->get() || !data.is_day)
        return;

    sun_ray_rotation += 0.01f * animation_speed_multiplier->get();
    if (sun_ray_rotation > 2 * M_PI)
        sun_ray_rotation -= 2 * M_PI;

    int code = data.weatherCode;
    if (code == 0 || code == 1 || code == 2 || code == 3)
    {
        int center_x = matrix_width / 2;
        int center_y = matrix_height / 4;

        for (int ray = 0; ray < 12; ray++)
        {
            float angle = sun_ray_rotation + ray * M_PI / 6;

            for (int len = 8; len < 35; len++)
            {
                float distance_factor = static_cast<float>(len - 8) / 27.0f;
                int ray_width = 1 + static_cast<int>(distance_factor * 3);

                for (int w = -ray_width / 2; w <= ray_width / 2; w++)
                {
                    float spread_angle = angle + (w * 0.02f);
                    int x = center_x + static_cast<int>(cos(spread_angle) * len);
                    int y = center_y + static_cast<int>(sin(spread_angle) * len);

                    if (x >= 0 && x < matrix_width && y >= 0 && y < matrix_height)
                    {
                        uint8_t existing_r, existing_g, existing_b;
                        canvas->GetPixel(x, y, &existing_r, &existing_g, &existing_b);

                        float base_intensity = 1.0f - distance_factor;
                        float width_intensity = 1.0f - std::abs(w) / static_cast<float>(ray_width / 2 + 1);
                        float final_intensity = base_intensity * width_intensity * 0.3f;

                        uint8_t sun_r = 255;
                        uint8_t sun_g = 220;
                        uint8_t sun_b = 100 + static_cast<uint8_t>(50 * distance_factor);

                        SetPixelAlpha(canvas, x, y, sun_r, sun_g, sun_b, final_intensity);
                    }
                }
            }
        }
    }
}

void Scenes::WeatherScene::renderFogMist(rgb_matrix::FrameCanvas *canvas, const WeatherData &data)
{
    if (!enable_animations->get())
        return;

    int code = data.weatherCode;
    bool is_fog =
        code == 10 ||
        (code >= 11 && code <= 12) ||
        (code >= 40 && code <= 49);

    if (is_fog)
    {
        std::uniform_int_distribution<int> num_dist(7, 9);
        const int NUM_PATCHES = num_dist(rng);

        if (!fog_initialized || last_fog_matrix_width != matrix_width || last_fog_matrix_height != matrix_height) {
            fog_initialized = false;
            fog_patches.clear();
            std::uniform_real_distribution<float> r_dist(18.0f, 40.0f);
            std::uniform_real_distribution<float> d_dist(0.25f, 0.55f);
            std::uniform_int_distribution<int> xy_dist(0, matrix_width - 1);
            std::uniform_int_distribution<int> yh_dist(0, matrix_height - 1);
            std::uniform_real_distribution<float> vx_dist(0.08f, 0.18f);
            std::uniform_int_distribution<int> sign_dist(0, 1);
            std::uniform_real_distribution<float> phase_dist(0.0f, 10.0f);
            for (int i = 0; i < NUM_PATCHES; ++i) {
                float r = r_dist(rng);
                float d = d_dist(rng);
                float x = xy_dist(rng);
                float y = yh_dist(rng);
                float vx = vx_dist(rng) * (sign_dist(rng) ? 1 : -1);
                float vy = 0.03f * (sign_dist(rng) ? 1 : -1);
                float phase = phase_dist(rng);
                fog_patches.push_back({x, y, vx, vy, r, d, phase});
            }
            fog_initialized = true;
            last_fog_matrix_width = matrix_width;
            last_fog_matrix_height = matrix_height;
        }

        fog_time += 0.05f * animation_speed_multiplier->get();
        for (auto &patch : fog_patches) {
            patch.x += patch.vx * (0.7f + 0.3f * sin(fog_time + patch.phase));
            patch.y += patch.vy * (0.7f + 0.3f * cos(fog_time + patch.phase * 1.2f));
            patch.phase += 0.01f + 0.01f * animation_speed_multiplier->get();
            if (patch.x < -patch.radius) patch.x = matrix_width + patch.radius;
            if (patch.x > matrix_width + patch.radius) patch.x = -patch.radius;
            if (patch.y < -patch.radius) patch.y = matrix_height + patch.radius;
            if (patch.y > matrix_height + patch.radius) patch.y = -patch.radius;
        }

        for (const auto &patch : fog_patches) {
            float edge_fade = 0.7f + 0.3f * sin(fog_time + patch.phase * 0.8f);
            for (int dx = -patch.radius; dx <= patch.radius; ++dx) {
                for (int dy = -patch.radius; dy <= patch.radius; ++dy) {
                    int x = static_cast<int>(patch.x + dx);
                    int y = static_cast<int>(patch.y + dy);
                    if (x >= 0 && x < matrix_width && y >= 0 && y < matrix_height) {
                        float dist = sqrtf(dx * dx + dy * dy);
                        if (dist <= patch.radius) {
                            float local_alpha = patch.density * (1.0f - dist / patch.radius) * edge_fade;
                            local_alpha *= 0.95f + 0.05f * sin(fog_time + patch.phase);
                            if (local_alpha > 0.05f) {
                                uint8_t fog_gray = 160 + static_cast<uint8_t>(30 * patch.density);
                                SetPixelAlpha(canvas, x, y, fog_gray, fog_gray, fog_gray + 10, std::min(local_alpha, 0.35f));
                            }
                        }
                    }
                }
            }
        }
        for (int y = 0; y < matrix_height; ++y) {
            float grad_alpha = 0.08f * (1.0f - y / (float)matrix_height);
            for (int x = 0; x < matrix_width; ++x) {
                SetPixelAlpha(canvas, x, y, 180, 180, 190, grad_alpha);
            }
        }
    } else {
        fog_initialized = false;
    }
}

void Scenes::WeatherScene::renderRainbowEffect(rgb_matrix::FrameCanvas *canvas, const WeatherData &data)
{
    if (!enable_rainbow->get() || !enable_animations->get())
        return;

    int code = data.weatherCode;
    bool is_rain =
        (code >= 50 && code <= 69) ||
        (code >= 80 && code <= 84) ||
        (code == 91 || code == 92);
    if (is_rain && data.is_day)
    {
        rainbow_time += 0.008f;

        int center_x = matrix_width / 2;
        int center_y = matrix_height + 10;
        int num_bands = 5;
        float base_radius = matrix_height * 0.6f;
        float band_width = 2.0f;

        for (int band = 0; band < num_bands; band++)
        {
            float radius = base_radius + band * band_width;
            float thickness = 2.0f;
            float edge_fade = 1.0f - (band / (float)num_bands) * 0.3f;
            float band_intensity = 0.85f + 0.10f * sin(rainbow_time + band);
            float intensity = band_intensity * edge_fade;

            for (int x = 0; x < matrix_width; x++)
            {
                float dx = x - center_x;
                float inside = radius * radius - dx * dx;
                if (inside < 0) continue;
                float y_arc = center_y - sqrtf(inside);
                for (float t = -thickness / 2.0f; t <= thickness / 2.0f; t += 0.5f)
                {
                    int y = static_cast<int>(y_arc + t);
                    if (y >= 0 && y < matrix_height)
                    {
                        float hue = ((float)x / matrix_width) * 360.0f;
                        hue += rainbow_time * 30.0f;
                        hue = std::fmod(hue, 360.0f);
                        float s = 1.0f;
                        float v = intensity;
                        uint8_t r, g, b;
                        color::hsv_to_rgb(hue, s, v, r, g, b);
                        canvas->SetPixel(x, y, r, g, b);
                    }
                }
            }
        }
    }
}

void Scenes::WeatherScene::renderAurora(rgb_matrix::FrameCanvas *canvas)
{
    if (!enable_aurora->get() || !enable_animations->get())
        return;

    if (data.temperature != "N/A" && !data.is_day && data.temperature.find("-") != std::string::npos)
    {
        aurora_continuous_time += 0.02f * animation_speed_multiplier->get();

        for (int x = 0; x < matrix_width; x++)
        {
            for (int y = 0; y < matrix_height; y++)
            {
                float wave1 = sin(aurora_continuous_time + x * 0.15f + y * 0.08f);
                float wave2 = sin(aurora_continuous_time * 1.2f + x * 0.12f - y * 0.05f);
                float wave3 = sin(aurora_continuous_time * 0.8f + x * 0.18f + y * 0.1f);

                float base_intensity = (wave1 + wave2 + wave3) / 3.0f * 0.2f + 0.25f;
                float vertical_fade = 1.0f - (static_cast<float>(y) / matrix_height) * 0.6f;
                float intensity = base_intensity * vertical_fade;

                if (intensity > 0.1f)
                {
                    float alpha = (intensity - 0.1f) * 0.8f;
                    float color_shift = sin(aurora_continuous_time * 0.5f + x * 0.1f) * 0.3f;
                    uint8_t r = static_cast<uint8_t>(std::clamp((intensity + color_shift * 0.5f) * 80, 0.0f, 255.0f));
                    uint8_t g = static_cast<uint8_t>(std::clamp(intensity * 180 + color_shift * 40, 0.0f, 255.0f));
                    uint8_t b = static_cast<uint8_t>(std::clamp(intensity * 120 - color_shift * 30, 0.0f, 255.0f));

                    SetPixelAlpha(canvas, x, y, r, g, b, alpha);
                }
            }
        }
    }
}

void Scenes::WeatherScene::drawWeatherBorder(rgb_matrix::FrameCanvas *canvas, const RGB &color, int brightness_mod) const
{
    for (int i = 0; i < BORDER_THICKNESS; i++)
    {
        for (int x = BORDER_PADDING; x < matrix_width - BORDER_PADDING; x++)
        {
            canvas->SetPixel(x, BORDER_PADDING + i,
                                        std::min(255, color.r + brightness_mod),
                                        std::min(255, color.g + brightness_mod),
                                        std::min(255, color.b + brightness_mod));

            canvas->SetPixel(x, matrix_height - BORDER_PADDING - i - 1,
                                        std::min(255, color.r + brightness_mod),
                                        std::min(255, color.g + brightness_mod),
                                        std::min(255, color.b + brightness_mod));
        }

        for (int y = BORDER_PADDING; y < matrix_height - BORDER_PADDING; y++)
        {
            canvas->SetPixel(BORDER_PADDING + i, y,
                                        std::min(255, color.r + brightness_mod),
                                        std::min(255, color.g + brightness_mod),
                                        std::min(255, color.b + brightness_mod));

            canvas->SetPixel(matrix_width - BORDER_PADDING - i - 1, y,
                                        std::min(255, color.r + brightness_mod),
                                        std::min(255, color.g + brightness_mod),
                                        std::min(255, color.b + brightness_mod));
        }
    }
}

void Scenes::WeatherScene::drawPrecipitationIndicator(rgb_matrix::FrameCanvas *canvas, float probability, int x,
                                                       int y) const
{
    if (probability <= 0.05f)
    {
        return;
    }

    constexpr int max_size = 5;
    const int size = std::max(2, static_cast<int>(probability * max_size));

    const uint8_t intensity = std::min(255, static_cast<int>(150 + probability * 105));

    for (int i = 0; i < size; i++)
    {
        int width = std::max(1, i / 2);
        for (int j = -width; j <= width; j++)
        {
            canvas->SetPixel(x + j, y + i,
                                       100, 150, intensity);
        }
    }
}

RGB Scenes::WeatherScene::interpolateColor(const RGB &start, const RGB &end, float progress)
{
    progress = std::max(0.0f, std::min(1.0f, progress));
    return {
        static_cast<uint8_t>(start.r + (end.r - start.r) * progress),
        static_cast<uint8_t>(start.g + (end.g - start.g) * progress),
        static_cast<uint8_t>(start.b + (end.b - start.b) * progress)};
}

void Scenes::WeatherScene::applyBackgroundEffects(rgb_matrix::FrameCanvas *canvas, const RGB &base_color)
{
    for (int y = 0; y < matrix_height; y++)
    {
        float gradient_factor = 1.0f - (float)y / matrix_height * GRADIENT_INTENSITY;

        for (int x = 0; x < matrix_width; x++)
        {
            float x_variation = 1.0f + std::sin(x * 0.1f) * 0.05f;

            int pulse = (animation_frame < 30) ? animation_frame : 60 - animation_frame;
            float pulse_factor = 1.0f + (pulse / 600.0f);

            uint8_t r = std::min(255.0f, base_color.r * gradient_factor * x_variation * pulse_factor);
            uint8_t g = std::min(255.0f, base_color.g * gradient_factor * x_variation * pulse_factor);
            uint8_t b = std::min(255.0f, base_color.b * gradient_factor * x_variation * pulse_factor);

            canvas->SetPixel(x, y, r, g, b);
        }
    }

    if (!data.is_day)
    {
        if (stars.empty())
        {
            resetStars();
        }

        for (int i = 0; i < stars.size(); i++)
        {
            const auto &coords = stars[i];
            const int x = std::get<0>(coords);
            const int y = std::get<1>(coords);

            const int brightness = 150 + (std::sin(0.1f * animation_frame + i) + 1) * 50;
            canvas->SetPixel(x, y, brightness, brightness, brightness);
        }

        if (enable_animations->get())
        {
            tryCreateShootingStar();
            updateShootingStars();
            renderShootingStars(canvas);
        }
    }
}

RGB Scenes::WeatherScene::getThemeColor(const ColorTheme theme, const WeatherData &data)
{
    switch (theme)
    {
    case ColorTheme::AUTO:
        return data.color;

    case ColorTheme::BLUE:
        return data.is_day ? RGB{30, 100, 200} : RGB{10, 30, 80};

    case ColorTheme::GREEN:
        return data.is_day ? RGB{40, 160, 70} : RGB{10, 60, 30};

    case ColorTheme::PURPLE:
        return data.is_day ? RGB{130, 60, 180} : RGB{50, 20, 80};

    case ColorTheme::ORANGE:
        return data.is_day ? RGB{220, 120, 40} : RGB{100, 50, 20};

    case ColorTheme::GRAYSCALE:
        return data.is_day ? RGB{180, 180, 180} : RGB{50, 50, 50};

    default:
        return data.color;
    }
}
