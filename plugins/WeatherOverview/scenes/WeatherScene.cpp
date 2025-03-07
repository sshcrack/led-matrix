#include <spdlog/spdlog.h>
#include "WeatherScene.h"
#include "../Constants.h"
#include "shared/utils/canvas_image.h"
#include "shared/utils/image_fetch.h"

namespace fs = std::filesystem;

const int MAIN_ICON_SIZE = 42;
const int FORECAST_ICON_SIZE = 16;
const int MAX_SCROLL = 20;
const int SCROLL_PAUSE = 40;
const int ANIMATION_INTERVAL = 100; // milliseconds

// Maximum number of particles in different animation modes
const int MAX_PARTICLES = 50;

// Color transition speed
const float COLOR_TRANSITION_SPEED = 0.05;

// Add these new constants at the top with the other constants
const int RAIN_SPEED_FACTOR = 2;
const int SNOW_SPEED_FACTOR = 1;
const float GRADIENT_INTENSITY = 0.7f;
const int BORDER_THICKNESS = 1;
const int BORDER_PADDING = 2;

// Instead of a global should_render flag, track update state inside the scene
std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> Scenes::WeatherSceneWrapper::create() {
    return {
        new WeatherScene(), [](Scenes::Scene *scene) {
            delete scene;
        }
    };
}

string Scenes::WeatherScene::get_name() const {
    return "weather";
}

void Scenes::WeatherScene::renderCurrentWeather(const RGBMatrixBase *matrix, const WeatherData &data) {
    // Draw the main weather icon
    if (images.has_value()) {
        SetImageTransparent(offscreen_canvas, 2, 12,
                            images->currentIcon, data.color.r, data.color.g, data.color.b);
    }

    // Draw temperature in large font
    constexpr int temp_x = MAIN_ICON_SIZE + 6;
    constexpr int temp_y = 20;
    DrawText(offscreen_canvas, HEADER_FONT, temp_x, temp_y,
             {255, 255, 255}, data.temperature.c_str());

    // Draw weather description with scroll effect if needed
    const std::string desc = data.description;
    const int desc_width = BODY_FONT.CharacterWidth('A') * desc.length();
    const int available_width = matrix->width() - temp_x;

    const int desc_y = temp_y + 14;

    if (desc_width > available_width) {
        // Handle text scrolling for long descriptions
        const int start_pos = temp_x - scroll_position;
        DrawText(offscreen_canvas, BODY_FONT, start_pos, desc_y,
                 {220, 220, 255}, desc.c_str());

        // Update scroll position for next frame
        if (scroll_pause_counter > 0) {
            scroll_pause_counter--;
        } else {
            scroll_position += scroll_direction;
            if (scroll_position >= desc_width - available_width + 5) {
                scroll_direction = -1;
                scroll_pause_counter = SCROLL_PAUSE;
            } else if (scroll_position <= 0) {
                scroll_direction = 1;
                scroll_pause_counter = SCROLL_PAUSE;
            }
        }
    } else {
        DrawText(offscreen_canvas, BODY_FONT, temp_x, desc_y,
                 {220, 220, 255}, desc.c_str());
    }

    // Draw additional weather info
    constexpr int add_info_y = desc_y + 10;
    const std::string humidity_info = "Humidity: " + data.humidity;
    DrawText(offscreen_canvas, SMALL_FONT, temp_x, add_info_y,
             {200, 200, 255}, humidity_info.c_str());

    const std::string wind_info = "Wind: " + data.wind_speed;
    DrawText(offscreen_canvas, SMALL_FONT, temp_x, add_info_y + 7,
             {200, 200, 255}, wind_info.c_str());
}

void Scenes::WeatherScene::renderForecast(const RGBMatrixBase *matrix, const WeatherData &data) const {
    int base_offset_x = 5;

    // Draw forecast title
    DrawText(offscreen_canvas, SMALL_FONT, base_offset_x, 65,
             {255, 255, 255}, "3-Day Forecast:");

    // Draw forecast data
    if (data.forecast.size() >= 3 && images.has_value()) {
        const int forecast_width = matrix->width() / 3;

        for (int i = 0; i < std::min(size_t(3), data.forecast.size()); i++) {
            const auto &day = data.forecast[i];
            const int base_x = i * forecast_width + base_offset_x;

            // Draw day name
            DrawText(offscreen_canvas, SMALL_FONT, base_x + 2, 78,
                     {255, 255, 255}, day.day_name.c_str());

            // Draw forecast icon
            if (i < images->forecastIcons.size()) {
                SetImageTransparent(offscreen_canvas, base_x + (forecast_width - FORECAST_ICON_SIZE) / 2 - 7, 79,
                                    images->forecastIcons[i], data.color.r, data.color.g, data.color.b);
            }

            // Draw min/max temperature
            std::string temp = day.temperature_min + "/" + day.temperature_max;
            const int temp_width = SMALL_FONT.CharacterWidth('A') * temp.length();
            DrawText(offscreen_canvas, SMALL_FONT, base_x + (forecast_width - temp_width) / 2, 100,
                     {220, 220, 255}, temp.c_str());

            // Draw precipitation indicator if probability is significant
            if (day.precipitation_chance > 0.1f) {
                // Position the indicator next to the temperature
                const int indicator_x = base_x + forecast_width - 8;
                const int indicator_y = 95;

                // Draw the precipitation indicator
                drawPrecipitationIndicator(matrix, day.precipitation_chance, indicator_x, indicator_y);

                // Draw the probability percentage
                std::string prob = std::to_string(static_cast<int>(day.precipitation_chance * 100)) + "%";
                DrawText(offscreen_canvas, SMALL_FONT, base_x + 10, 110,
                         {150, 200, 255}, prob.c_str());
            }
        }
    }
}

void Scenes::WeatherScene::initializeParticles() {
    particles.resize(MAX_PARTICLES);
    for (auto &p: particles) {
        p.active = false;
    }
    active_particles = 0;
}

void Scenes::WeatherScene::updateAnimationState(const WeatherData &data) {
    // Determine if we should show precipitation animations based on weather code
    // Weather codes typically follow these patterns:
    // 2xx: Thunderstorm
    // 3xx: Drizzle
    // 5xx: Rain
    // 6xx: Snow
    // 7xx: Atmosphere (fog, mist)
    // 800: Clear
    // 80x: Clouds

    has_precipitation = false;

    if (!enable_animations->get()) {
        return;
    }

    int code = data.weatherCode;
    if ((code >= 200 && code < 700) || data.precipitation > 0.1f) {
        has_precipitation = true;

        // If we don't have particles initialized yet
        if (particles.empty()) {
            initializeParticles();
        }

        // Update existing particles
        updateParticles(data);
    }
}

void Scenes::WeatherScene::updateParticles(const WeatherData &data) {
    int code = data.weatherCode;
    bool is_snow = (code >= 600 && code < 700);
    int intensity = animation_intensity->get();

    // Calculate how many particles should be active based on precipitation and intensity
    int target_particles = std::min(MAX_PARTICLES,
                                    static_cast<int>(MAX_PARTICLES * data.precipitation * intensity / 10.0f));

    // Update existing particles
    for (auto &p: particles) {
        if (p.active) {
            // Move particle down
            p.y += p.speed;

            // Add some horizontal drift for snow
            if (is_snow) {
                p.x += std::sin(p.y * 0.1f) * 0.3f;
            }

            // If particle is out of bounds, deactivate it
            if (p.y >= 64) {
                p.active = false;
                active_particles--;
            }
        }
    }

    // Activate new particles if needed
    if (active_particles < target_particles) {
        for (auto &p: particles) {
            if (!p.active) {
                p.active = true;
                p.x = static_cast<float>(rand() % 64);
                p.y = 0;
                p.size = is_snow ? 1.0f + (rand() % 2) : 1.0f;
                p.opacity = 150 + (rand() % 105);
                p.speed = is_snow
                              ? 0.2f + (rand() % 10) / 10.0f * SNOW_SPEED_FACTOR
                              : 0.5f + (rand() % 15) / 10.0f * RAIN_SPEED_FACTOR;
                active_particles++;

                if (active_particles >= target_particles) {
                    break;
                }
            }
        }
    }
}

void Scenes::WeatherScene::renderAnimations(const RGBMatrixBase *matrix, const WeatherData &data) {
    if (!has_precipitation || !enable_animations->get()) {
        return;
    }

    const int code = data.weatherCode;
    const bool is_snow = (code >= 600 && code < 700);

    // Render active particles
    for (const auto &p: particles) {
        if (p.active) {
            if (is_snow) {
                // Snow particles are white dots
                offscreen_canvas->SetPixel(static_cast<int>(p.x), static_cast<int>(p.y),
                                           p.opacity, p.opacity, p.opacity);

                // For larger snow particles, draw a small cluster
                if (p.size > 1.5f) {
                    offscreen_canvas->SetPixel(static_cast<int>(p.x) + 1, static_cast<int>(p.y),
                                               p.opacity * 0.7f, p.opacity * 0.7f, p.opacity * 0.7f);
                    offscreen_canvas->SetPixel(static_cast<int>(p.x), static_cast<int>(p.y) + 1,
                                               p.opacity * 0.7f, p.opacity * 0.7f, p.opacity * 0.7f);
                }
            } else {
                // Rain particles are blue-white streaks
                for (int i = 0; i < 2; i++) {
                    offscreen_canvas->SetPixel(static_cast<int>(p.x), static_cast<int>(p.y) - i,
                                               200, 220, p.opacity);
                }
            }
        }
    }
}

RGB Scenes::WeatherScene::interpolateColor(const RGB &start, const RGB &end, float progress) {
    progress = std::max(0.0f, std::min(1.0f, progress));
    return {
        static_cast<uint8_t>(start.r + (end.r - start.r) * progress),
        static_cast<uint8_t>(start.g + (end.g - start.g) * progress),
        static_cast<uint8_t>(start.b + (end.b - start.b) * progress)
    };
}

void Scenes::WeatherScene::applyBackgroundEffects(const RGBMatrixBase *matrix, const RGB &base_color) {
    // Create a gradient background
    for (int y = 0; y < matrix->height(); y++) {
        // Calculate gradient factor (darker at bottom, lighter at top)
        float gradient_factor = 1.0f - (float) y / matrix->height() * GRADIENT_INTENSITY;

        for (int x = 0; x < matrix->width(); x++) {
            // Add some subtle horizontal variation
            float x_variation = 1.0f + std::sin(x * 0.1f) * 0.05f;

            // Apply pulse animation
            int pulse = (animation_frame < 30) ? animation_frame : 60 - animation_frame;
            float pulse_factor = 1.0f + (pulse / 300.0f);

            // Calculate final color
            uint8_t r = std::min(255.0f, base_color.r * gradient_factor * x_variation * pulse_factor);
            uint8_t g = std::min(255.0f, base_color.g * gradient_factor * x_variation * pulse_factor);
            uint8_t b = std::min(255.0f, base_color.b * gradient_factor * x_variation * pulse_factor);

            offscreen_canvas->SetPixel(x, y, r, g, b);
        }
    }

    // Add subtle star-like dots for night scenes
    if (!data.is_day) {
        if (stars.empty()) {
            resetStars();
        }

        for (int i = 0; i < stars.size(); i++) {
            const auto &coords = stars[i];
            const int x = std::get<0>(coords);
            const int y = std::get<1>(coords);

            // Make stars twinkle
            const int brightness = 150 + (std::sin(animation_frame * 0.1f + i) + 1) * 50;
            offscreen_canvas->SetPixel(x, y, brightness, brightness, brightness);
        }
    }
}

void Scenes::WeatherScene::drawWeatherBorder(const RGBMatrixBase *matrix, const RGB &color, int brightness_mod) const {
    // Draw a subtle border around the display
    for (int i = 0; i < BORDER_THICKNESS; i++) {
        // Top and bottom borders
        for (int x = BORDER_PADDING; x < matrix->width() - BORDER_PADDING; x++) {
            offscreen_canvas->SetPixel(x, BORDER_PADDING + i,
                                       std::min(255, color.r + brightness_mod),
                                       std::min(255, color.g + brightness_mod),
                                       std::min(255, color.b + brightness_mod));

            offscreen_canvas->SetPixel(x, matrix->height() - BORDER_PADDING - i - 1,
                                       std::min(255, color.r + brightness_mod),
                                       std::min(255, color.g + brightness_mod),
                                       std::min(255, color.b + brightness_mod));
        }

        // Left and right borders
        for (int y = BORDER_PADDING; y < matrix->height() - BORDER_PADDING; y++) {
            offscreen_canvas->SetPixel(BORDER_PADDING + i, y,
                                       std::min(255, color.r + brightness_mod),
                                       std::min(255, color.g + brightness_mod),
                                       std::min(255, color.b + brightness_mod));

            offscreen_canvas->SetPixel(matrix->width() - BORDER_PADDING - i - 1, y,
                                       std::min(255, color.r + brightness_mod),
                                       std::min(255, color.g + brightness_mod),
                                       std::min(255, color.b + brightness_mod));
        }
    }
}

void Scenes::WeatherScene::drawPrecipitationIndicator(const RGBMatrixBase *matrix, float probability, int x,
                                                      int y) const {
    if (probability <= 0.05f) {
        return; // Don't show indicator for very low probability
    }

    // Draw a small droplet icon with size based on probability
    const int max_size = 5;
    int size = std::max(2, static_cast<int>(probability * max_size));

    // Blue color with intensity based on probability
    uint8_t intensity = std::min(255, static_cast<int>(150 + probability * 105));

    // Draw droplet shape
    for (int i = 0; i < size; i++) {
        int width = std::max(1, i / 2);
        for (int j = -width; j <= width; j++) {
            offscreen_canvas->SetPixel(x + j, y + i,
                                       100, 150, intensity);
        }
    }
}

void Scenes::WeatherScene::renderSunriseSunset(const RGBMatrixBase *matrix, const WeatherData &data) const {
    if (data.sunrise.empty() || data.sunset.empty() || !show_sunrise_sunset->get()) {
        return;
    }

    // Draw sun icon for sunrise
    const int icon_size = 5;
    const int base_x = 10;
    const int base_y = 55;

    // Draw sunrise icon (simple sun)
    for (int i = -1; i <= 1; i++) {
        for (int j = -1; j <= 1; j++) {
            if (i == 0 || j == 0) {
                offscreen_canvas->SetPixel(base_x + i, base_y + j, 255, 200, 50);
            }
        }
    }
    offscreen_canvas->SetPixel(base_x, base_y, 255, 220, 100);

    // Draw sunrise time
    std::string sunrise_text = "↑ " + data.sunrise;
    DrawText(offscreen_canvas, SMALL_FONT, base_x + icon_size + 2, base_y + 2,
             {255, 220, 100}, sunrise_text.c_str());

    // Draw sunset icon (simple sun with down arrow)
    const int sunset_x = matrix->width() / 2 + 20;
    for (int i = -1; i <= 1; i++) {
        for (int j = -1; j <= 1; j++) {
            if (i == 0 || j == 0) {
                offscreen_canvas->SetPixel(sunset_x + i, base_y + j, 255, 150, 50);
            }
        }
    }
    offscreen_canvas->SetPixel(sunset_x, base_y, 255, 180, 80);

    // Draw sunset time
    std::string sunset_text = "↓ " + data.sunset;
    DrawText(offscreen_canvas, SMALL_FONT, sunset_x + icon_size + 2, base_y + 2,
             {255, 180, 80}, sunset_text.c_str());
}

void Scenes::WeatherScene::renderClock(const RGBMatrixBase *matrix) const {
    const time_t timestamp = time(NULL);
    const tm datetime = *localtime(&timestamp);

    char output[50];
    strftime(output, 50, "%H:%M", &datetime);

    rgb_matrix::DrawText(offscreen_canvas, BODY_FONT, 98, 11, {255, 255, 255}, output);
}

void Scenes::WeatherScene::resetStars() {
    this->stars.clear();

    for (int i = 0; i < 20; i++) {
        int x = rand() % (matrix_width + 1);
        int y = rand() % (matrix_height + 1);

        stars.push_back(std::make_pair(x, y));
    }
}

RGB Scenes::WeatherScene::getThemeColor(ColorTheme theme, const WeatherData &data) const {
    switch (theme) {
        case ColorTheme::AUTO:
            return data.color; // Use the default color from weather data

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

bool Scenes::WeatherScene::render(RGBMatrixBase *matrix) {
    auto data_res = parser.get_data(location_lat->get(), location_lon->get());
    if (!data_res) {
        spdlog::warn("Could not get weather data: {}", data_res.error());
        // Instead of returning false, show an error message and continue
        offscreen_canvas->Clear();
        DrawText(offscreen_canvas, BODY_FONT, 2, BODY_FONT.baseline() + 5,
                 {255, 100, 100}, "Weather data error");
        DrawText(offscreen_canvas, SMALL_FONT, 2, BODY_FONT.baseline() + 15,
                 {200, 200, 200}, data_res.error().c_str());
        offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas, 1);
        SleepMillis(1000);
        return false; // Continue running despite error
    }

    data = data_res.value(); // Store data for animations
    bool should_update_display = false;

    // Get the appropriate color based on theme setting
    RGB theme_color = getThemeColor(static_cast<ColorTheme>(color_theme->get()), data);

    // Check if we need to reload images
    if (parser.has_changed() || !images.has_value()) {
        should_update_display = true;
        const auto weather_dir_path = fs::path(weather_dir);
        if (!exists(weather_dir_path)) {
            fs::create_directory(weather_dir);
        }

        // Download and process current weather icon
        string file_path = weather_dir_path / ("weather_icon" + std::to_string(data.weatherCode) + ".png");
        fs::path processed_img = to_processed_path(file_path);

        if (!fs::exists(processed_img) && !data.icon_url.empty()) {
            try_remove(file_path);
            auto res = utils::download_image(data.icon_url, file_path);
            if (!res) {
                spdlog::warn("Could not download main image {}", res.error());
            }
        }

        bool contain_img = true;
        auto res = LoadImageAndScale(file_path, MAIN_ICON_SIZE, MAIN_ICON_SIZE, true, true, contain_img, true);

        Images img;

        if (res.has_value()) {
            auto arr = std::move(res.value());
            img.currentIcon = arr.front();
            try_remove(file_path);
        } else {
            spdlog::error("Error loading main image: {}", res.error());
            // Don't return false here, continue with empty icon
        }

        // Process forecast icons
        for (const auto &forecast_day: data.forecast) {
            if (!forecast_day.icon_url.empty()) {
                string forecast_file = weather_dir_path / ("forecast_icon" +
                                                           std::to_string(forecast_day.weatherCode) + ".png");
                fs::path forecast_processed = to_processed_path(forecast_file);

                if (!fs::exists(forecast_processed)) {
                    try_remove(forecast_file);
                    auto dl_res = utils::download_image(forecast_day.icon_url, forecast_file);
                    if (!dl_res) {
                        spdlog::warn("Could not download forecast image {}", dl_res.error());
                        continue;
                    }
                }

                auto f_res = LoadImageAndScale(forecast_file, FORECAST_ICON_SIZE, FORECAST_ICON_SIZE,
                                               true, true, contain_img, true);

                if (f_res) {
                    img.forecastIcons.push_back(f_res.value().at(0));
                } else {
                    spdlog::warn("Could not load forecast image: {}", f_res.error());
                }

                try_remove(forecast_file);
            }
        }

        parser.unmark_changed();
        images = img;

        // Initialize animation state when data changes
        updateAnimationState(data);
    }

    // Check if animation frame needs to be updated
    tmillis_t current_time = GetTimeInMillis();
    if (current_time - last_animation_time > ANIMATION_INTERVAL) {
        animation_frame = (animation_frame + 1) % 60; // 60 frames for subtle animations
        last_animation_time = current_time;
        should_update_display = true;

        // Update animation state periodically
        if (has_precipitation) {
            updateParticles(data);
        }
    }

    // Always update on scroll position changes
    if (scroll_position > 0 || scroll_direction > 0) {
        should_update_display = true;
    }

    // Update the display if needed
    if (should_update_display) {
        offscreen_canvas->Clear();

        // Apply beautiful background with gradient if enabled
        if (gradient_background->get()) {
            applyBackgroundEffects(matrix, theme_color);
        } else {
            // Simple background fill
            offscreen_canvas->Fill(theme_color.r, theme_color.g, theme_color.b);
        }

        // Draw a subtle border if enabled
        if (show_border->get()) {
            drawWeatherBorder(matrix, theme_color, 40);
        }

        if (enable_clock->get())
            renderClock(matrix);

        // Render all components
        renderCurrentWeather(matrix, data);
        renderSunriseSunset(matrix, data);
        renderForecast(matrix, data);

        // Render weather animations (rain, snow, etc.)
        if (enable_animations->get()) {
            renderAnimations(matrix, data);
        }

        offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas, 1);
    }

    return true; // Always return true to keep the scene running
}

void Scenes::WeatherScene::after_render_stop(RGBMatrixBase *matrix) {
    // Reset scroll position when stopping render
    scroll_position = 0;
    scroll_direction = 1;
    scroll_pause_counter = 0;
    resetStars();
    Scene::after_render_stop(matrix);
}
