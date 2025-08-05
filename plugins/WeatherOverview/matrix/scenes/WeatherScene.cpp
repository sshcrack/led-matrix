#include "spdlog/spdlog.h"
#include "WeatherScene.h"
#include "../Constants.h"
#include "picosha2.h"
#include "shared/matrix/utils/canvas_image.h"
#include "shared/matrix/utils/image_fetch.h"

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

// Constants for shooting stars
const int MAX_SHOOTING_STARS = 3;
const int MIN_MS_BETWEEN_STARS = 5 * 1000; // 5 seconds
const float SHOOTING_STAR_SPEED_MIN = 1.5f;
const float SHOOTING_STAR_SPEED_MAX = 3.0f;

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

static void pre_process_image(Magick::Image *img) {
    const int w = img->columns() * 0.9f;
    const int h = img->rows() * 0.9f;

    const int x = (img->columns() - w) / 2;
    const int y = (img->rows() - h) / 2;

    img->crop(Magick::Geometry(w, h, x, y));
}

void Scenes::WeatherScene::renderCurrentWeather(const RGBMatrixBase *matrix, const WeatherData &data) {
    // Draw the main weather icon
    if (images.has_value()) {
        SetImageTransparent(offscreen_canvas, 2, 12,
                            images->currentIcon);
    }

    // Draw temperature in large font
    constexpr int temp_x = MAIN_ICON_SIZE + 6;
    constexpr int temp_y = 20;
    DrawText(offscreen_canvas, HEADER_FONT, temp_x, temp_y,
             {255, 255, 255}, data.temperature.c_str());

    // Draw weather description with scroll effect if needed
    const std::string desc = data.description;
    const int desc_y = temp_y + 14;

    DrawText(offscreen_canvas, BODY_FONT, temp_x, desc_y,
             {220, 220, 255}, desc.c_str());

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
                                    images->forecastIcons[i]);
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
    if (!enable_animations->get()) {
        return;
    }

    // Render enhanced particle effects if enabled
    if (enable_enhanced_particles->get()) {
        renderEnhancedParticles(matrix, data);
    } else {
        // Original particle rendering
        if (!has_precipitation) {
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
    
    // Add new beautiful weather effects
    renderClouds(matrix, data);
    renderLightning(matrix);
    renderSunRays(matrix, data);
    renderFogMist(matrix, data);
    renderRainbowEffect(matrix, data);
    renderAurora(matrix);
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
            const int brightness = 150 + (std::sin(0.1f * animation_frame + i) + 1) * 50;
            offscreen_canvas->SetPixel(x, y, brightness, brightness, brightness);
        }
        
        // Update and render shooting stars
        if (enable_animations->get()) {
            tryCreateShootingStar();
            updateShootingStars();
            renderShootingStars();
        }
    }
}

void Scenes::WeatherScene::tryCreateShootingStar() {
    // Only try to create a shooting star if we're at night and not too many stars are active
    if (data.is_day) {
        return;
    }
    
    // Count active shooting stars
    int active_count = 0;
    for (const auto& star : shooting_stars) {
        if (star.active) active_count++;
    }
    
    // Don't create more than the max number of shooting stars
    if (active_count >= MAX_SHOOTING_STARS) {
        return;
    }
    
    // Only try to create a shooting star if enough frames have passed since the last one
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_shooting_star_time).count();


    if (elapsed < MIN_MS_BETWEEN_STARS || animation_frame % get_target_fps() <= shooting_star_frame_threshold->get()) {
        return;
    }
    
    // Random chance based on user setting (percentage chance per frame check)
    const int chance = shooting_star_chance->get();
    if (chance <= 0) return;
    
    if (rand() % 100 < chance) {
        // Create a new shooting star
        if (shooting_stars.size() < MAX_SHOOTING_STARS) {
            shooting_stars.resize(shooting_stars.size() + 1);
        }
        
        // Find an inactive shooting star slot
        for (auto& star : shooting_stars) {
            if (!star.active) {
                // Determine direction (diagonal across the screen)
                bool from_top = (rand() % 2 == 0);
                bool from_left = (rand() % 2 == 0);

                // Set starting position
                if (from_top) {
                    star.x = rand() % matrix_width;
                    star.y = 0;
                } else {
                    star.x = from_left ? 0 : matrix_width - 1;
                    star.y = rand() % (matrix_height / 2); // Start in top half
                }

                // Set direction and speed
                float speed = SHOOTING_STAR_SPEED_MIN +
                              (static_cast<float>(rand()) / RAND_MAX) *
                              (SHOOTING_STAR_SPEED_MAX - SHOOTING_STAR_SPEED_MIN);

                star.dx = from_left ? speed : -speed;
                star.dy = speed;

                // Set other properties
                star.tail_length = 3.0f + (static_cast<float>(rand()) / RAND_MAX) * 5.0f;
                star.brightness = 150.0f + (static_cast<float>(rand()) / RAND_MAX) * 105.0f;
                star.active = true;
                
                last_shooting_star_time = std::chrono::steady_clock::now();
                break;
            }
        }
    }
}

void Scenes::WeatherScene::updateShootingStars() {
    for (auto& star : shooting_stars) {
        if (star.active) {
            // Move the shooting star
            star.x += star.dx;
            star.y += star.dy;
            
            // Check if the shooting star has left the screen
            if (star.x < -star.tail_length || star.x >= matrix_width + star.tail_length ||
                star.y < -star.tail_length || star.y >= matrix_height + star.tail_length) {
                star.active = false;
            }
        }
    }
}

void Scenes::WeatherScene::renderShootingStars() {
    for (const auto& star : shooting_stars) {
        if (star.active) {
            // Draw the shooting star as a line with a gradient
            for (float i = 0; i <= star.tail_length; i++) {
                float tail_x = star.x - (star.dx * i / star.dy);
                float tail_y = star.y - i;
                
                // Calculate brightness based on position in tail
                float brightness_factor = 1.0f - (i / star.tail_length);
                uint8_t b = static_cast<uint8_t>(star.brightness * brightness_factor);
                
                // Draw pixel if it's on screen
                int px = static_cast<int>(tail_x);
                int py = static_cast<int>(tail_y);
                if (px >= 0 && px < matrix_width && py >= 0 && py < matrix_height) {
                    offscreen_canvas->SetPixel(px, py, b, b, b);
                }
            }
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
    constexpr int max_size = 5;
    const int size = std::max(2, static_cast<int>(probability * max_size));

    // Blue color with intensity based on probability
    const uint8_t intensity = std::min(255, static_cast<int>(150 + probability * 105));

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
    constexpr int icon_size = 5;
    constexpr int base_x = 10;
    constexpr int base_y = 55;

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
    const time_t timestamp = time(nullptr);
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

        stars.emplace_back(x, y);
    }
}

RGB Scenes::WeatherScene::getThemeColor(const ColorTheme theme, const WeatherData &data) {
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

    // Get the appropriate color based on theme setting
    RGB theme_color = getThemeColor(color_theme->get().get(), data);

    // Check if we need to reload images
    if (parser.has_changed() || !images.has_value()) {
        const auto weather_dir_path = fs::path(weather_dir);
        if (!exists(weather_dir_path)) {
            fs::create_directory(weather_dir);
        }

        std::string hash;
        picosha2::hash256_hex_string(data.icon_url, hash);

        // Download and process current weather icon
        string file_path = weather_dir_path / ("weather_icon_" + hash + ".png");
        fs::path processed_img = to_processed_path(file_path);

        if (!fs::exists(processed_img) && !data.icon_url.empty()) {
            try_remove(file_path);
            auto res = utils::download_image(data.icon_url, file_path);
            if (!res) {
                spdlog::warn("Could not download main image {}", res.error());
            }
        }

        bool contain_img = true;
        auto res = LoadImageAndScale(
            file_path,
            MAIN_ICON_SIZE, MAIN_ICON_SIZE,
            true, true,
            contain_img, true,
            pre_process_image
        );

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
                std::string forecast_hash;
                picosha2::hash256_hex_string(forecast_day.icon_url, forecast_hash);


                string forecast_file = weather_dir_path / ("forecast_icon" + forecast_hash + ".png");
                fs::path forecast_processed = to_processed_path(forecast_file);

                if (!fs::exists(forecast_processed)) {
                    try_remove(forecast_file);
                    auto dl_res = utils::download_image(forecast_day.icon_url, forecast_file);
                    if (!dl_res) {
                        spdlog::warn("Could not download forecast image {}", dl_res.error());
                        continue;
                    }
                }

                auto f_res = LoadImageAndScale(forecast_file,
                                               FORECAST_ICON_SIZE, FORECAST_ICON_SIZE,
                                               true, true,
                                               contain_img, true,
                                               pre_process_image
                );

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

    animation_frame = (animation_frame + 1) % get_target_fps(); // 60 frames for subtle animations

    // Update animation state periodically
    if (has_precipitation) {
        updateParticles(data);
    }

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

    wait_until_next_frame();
    return true; // Always return true to keep the scene running
}

void Scenes::WeatherScene::after_render_stop(RGBMatrixBase *matrix) {
    if (reset_stars_on_exit->get()) {
        resetStars();
        // Clear any active shooting stars
        for (auto& star : shooting_stars) {
            star.active = false;
        }
    }
    Scene::after_render_stop(matrix);
}

// Enhanced Animation Methods

void Scenes::WeatherScene::updateEnhancedParticles(const WeatherData &data) {
    const int code = data.weatherCode;
    const bool is_snow = (code >= 600 && code < 700);
    const bool is_rain = (code >= 200 && code < 600);

    has_precipitation = is_snow || is_rain;

    if (!has_precipitation || !enable_animations->get()) {
        active_particles = 0;
        for (auto &p: particles) {
            p.active = false;
        }
        return;
    }

    int target_particles = std::min(MAX_PARTICLES, particle_density->get() * 5);
    float speed_mult = animation_speed_multiplier->get();

    // Update existing particles with enhanced physics
    for (auto &p: particles) {
        if (p.active) {
            // Wind effect - create swaying motion
            p.wind_factor += 0.1f;
            float wind_offset = sin(p.wind_factor) * 0.3f;
            
            p.x += wind_offset;
            p.y += p.speed * speed_mult;
            p.life_time += 0.016f; // Assuming 60 FPS
            
            // Snow rotation
            if (is_snow) {
                p.rotation += 0.05f;
            }
            
            // Fade effect for older particles
            if (p.life_time > 2.0f) {
                p.opacity *= 0.98f;
            }
            
            // Remove particles that are off screen or too faded
            if (p.y > matrix_height || p.x < -5 || p.x > matrix_width + 5 || p.opacity < 10) {
                p.active = false;
                active_particles--;
            }
        }
    }

    // Activate new particles with enhanced properties
    if (active_particles < target_particles) {
        for (auto &p: particles) {
            if (!p.active) {
                p.active = true;
                p.x = static_cast<float>(rand() % (matrix_width + 10) - 5);
                p.y = -5;
                p.life_time = 0;
                p.wind_factor = static_cast<float>(rand() % 360) * 0.0174f; // Random starting phase
                p.rotation = 0;
                
                if (is_snow) {
                    p.size = 1.0f + (rand() % 3);
                    p.speed = 0.1f + (rand() % 8) / 10.0f;
                    p.opacity = 180 + (rand() % 75);
                    // Varied snow colors for more realism
                    p.r = 240 + (rand() % 15);
                    p.g = 240 + (rand() % 15);
                    p.b = 250 + (rand() % 5);
                } else if (is_rain) {
                    p.size = 1.0f;
                    p.speed = 0.8f + (rand() % 20) / 10.0f;
                    p.opacity = 150 + (rand() % 105);
                    // Rain colors with slight blue tint
                    p.r = 150 + (rand() % 50);
                    p.g = 180 + (rand() % 50);
                    p.b = 200 + (rand() % 55);
                }
                
                active_particles++;
                if (active_particles >= target_particles) {
                    break;
                }
            }
        }
    }
}

void Scenes::WeatherScene::renderEnhancedParticles(const RGBMatrixBase *matrix, const WeatherData &data) {
    if (!has_precipitation || !enable_animations->get()) {
        return;
    }

    const int code = data.weatherCode;
    const bool is_snow = (code >= 600 && code < 700);

    // Render active particles with enhanced visuals
    for (const auto &p: particles) {
        if (p.active) {
            int px = static_cast<int>(p.x);
            int py = static_cast<int>(p.y);
            
            if (px >= 0 && px < matrix_width && py >= 0 && py < matrix_height) {
                if (is_snow) {
                    // Enhanced snow rendering with rotation and clustering
                    uint8_t alpha = static_cast<uint8_t>(p.opacity);
                    offscreen_canvas->SetPixel(px, py, 
                                             std::min(255, (int)(p.r * alpha / 255)),
                                             std::min(255, (int)(p.g * alpha / 255)),
                                             std::min(255, (int)(p.b * alpha / 255)));

                    // For larger snow particles, draw a cluster with rotation
                    if (p.size > 1.5f) {
                        float cos_r = cos(p.rotation);
                        float sin_r = sin(p.rotation);
                        
                        // Draw rotated snowflake pattern
                        for (int i = -1; i <= 1; i++) {
                            for (int j = -1; j <= 1; j++) {
                                if (i == 0 && j == 0) continue;
                                
                                int rx = px + static_cast<int>(i * cos_r - j * sin_r);
                                int ry = py + static_cast<int>(i * sin_r + j * cos_r);
                                
                                if (rx >= 0 && rx < matrix_width && ry >= 0 && ry < matrix_height) {
                                    uint8_t sub_alpha = static_cast<uint8_t>(alpha * 0.6f);
                                    offscreen_canvas->SetPixel(rx, ry,
                                                             std::min(255, (int)(p.r * sub_alpha / 255)),
                                                             std::min(255, (int)(p.g * sub_alpha / 255)),
                                                             std::min(255, (int)(p.b * sub_alpha / 255)));
                                }
                            }
                        }
                    }
                } else {
                    // Enhanced rain rendering with streaks and splash effects
                    uint8_t alpha = static_cast<uint8_t>(p.opacity);
                    
                    // Draw rain streak
                    for (int i = 0; i < 3; i++) {
                        int ry = py - i;
                        if (ry >= 0 && ry < matrix_height) {
                            float streak_alpha = alpha * (1.0f - i * 0.3f);
                            offscreen_canvas->SetPixel(px, ry,
                                                     std::min(255, (int)(p.r * streak_alpha / 255)),
                                                     std::min(255, (int)(p.g * streak_alpha / 255)),
                                                     std::min(255, (int)(p.b * streak_alpha / 255)));
                        }
                    }
                }
            }
        }
    }
}

void Scenes::WeatherScene::renderClouds(const RGBMatrixBase *matrix, const WeatherData &data) {
    if (!enable_animations->get()) return;
    
    // Initialize cloud layers if empty
    if (cloud_layers.empty()) {
        for (int i = 0; i < 3; i++) {
            cloud_layers.push_back({static_cast<float>(rand() % matrix_width), 
                                   static_cast<float>(10 + rand() % 20)});
        }
    }
    
    // Update cloud animation
    cloud_phase += 0.02f * animation_speed_multiplier->get();
    
    // Render moving clouds for cloudy weather
    int code = data.weatherCode;
    if (code >= 800 && code < 900) { // Cloudy conditions
        for (size_t i = 0; i < cloud_layers.size(); i++) {
            auto& cloud = cloud_layers[i];
            
            // Move clouds slowly across screen
            cloud.first += 0.1f * animation_speed_multiplier->get();
            if (cloud.first > matrix_width + 20) {
                cloud.first = -20;
                cloud.second = 10 + rand() % 20;
            }
            
            // Draw soft cloud shape
            float cloud_intensity = 0.3f + 0.2f * sin(cloud_phase + i);
            int cloud_width = 15 + i * 3;
            int cloud_height = 6 + i;
            
            for (int x = 0; x < cloud_width; x++) {
                for (int y = 0; y < cloud_height; y++) {
                    int px = static_cast<int>(cloud.first) + x - cloud_width/2;
                    int py = static_cast<int>(cloud.second) + y - cloud_height/2;
                    
                    if (px >= 0 && px < matrix_width && py >= 0 && py < matrix_height) {
                        // Distance from cloud center for soft edges
                        float dist = sqrt(pow(x - cloud_width/2.0f, 2) + pow(y - cloud_height/2.0f, 2));
                        float edge_factor = std::max(0.0f, 1.0f - dist / (cloud_width/2.0f));
                        
                        uint8_t cloud_color = static_cast<uint8_t>(100 * cloud_intensity * edge_factor);
                        offscreen_canvas->SetPixel(px, py, cloud_color, cloud_color, cloud_color + 20);
                    }
                }
            }
        }
    }
}

void Scenes::WeatherScene::renderLightning(const RGBMatrixBase *matrix) {
    if (!enable_lightning->get() || !enable_animations->get()) return;
    
    // Trigger lightning for thunderstorm conditions
    if (lightning_timer > 0) {
        lightning_timer--;
        
        if (lightning_active && lightning_timer > 180) {
            // Bright flash
            for (int y = 0; y < matrix_height; y++) {
                for (int x = 0; x < matrix_width; x++) {
                    offscreen_canvas->SetPixel(x, y, 255, 255, 200);
                }
            }
        } else if (lightning_active && lightning_timer > 170) {
            // Draw lightning bolt
            for (const auto& point : lightning_points) {
                if (point.first >= 0 && point.first < matrix_width && 
                    point.second >= 0 && point.second < matrix_height) {
                    offscreen_canvas->SetPixel(point.first, point.second, 200, 200, 255);
                }
            }
        }
        
        if (lightning_timer <= 0) {
            lightning_active = false;
        }
    }
    
    // Randomly trigger new lightning
    if (!lightning_active && (rand() % 1800) == 0) { // About every 30 seconds at 60fps
        lightning_active = true;
        lightning_timer = 200;
        
        // Generate lightning bolt path
        lightning_points.clear();
        int x = rand() % matrix_width;
        int y = 0;
        
        while (y < matrix_height) {
            lightning_points.push_back({x, y});
            y += 1 + rand() % 3;
            x += (rand() % 3) - 1; // -1, 0, or 1
            x = std::max(0, std::min(matrix_width - 1, x));
        }
    }
}

void Scenes::WeatherScene::renderSunRays(const RGBMatrixBase *matrix, const WeatherData &data) {
    if (!enable_sun_rays->get() || !enable_animations->get() || !data.is_day) return;
    
    // Update sun ray rotation
    sun_ray_rotation += 0.01f * animation_speed_multiplier->get();
    if (sun_ray_rotation > 2 * M_PI) sun_ray_rotation -= 2 * M_PI;
    
    // Draw rotating sun rays for clear/sunny weather
    int code = data.weatherCode;
    if (code == 800) { // Clear sky
        int center_x = matrix_width / 2;
        int center_y = matrix_height / 4;
        
        for (int ray = 0; ray < 8; ray++) {
            float angle = sun_ray_rotation + ray * M_PI / 4;
            
            for (int len = 10; len < 25; len++) {
                int x = center_x + static_cast<int>(cos(angle) * len);
                int y = center_y + static_cast<int>(sin(angle) * len);
                
                if (x >= 0 && x < matrix_width && y >= 0 && y < matrix_height) {
                    uint8_t intensity = static_cast<uint8_t>(100 * (1.0f - (len - 10) / 15.0f));
                    offscreen_canvas->SetPixel(x, y, 255, 200, intensity);
                }
            }
        }
    }
}

void Scenes::WeatherScene::renderFogMist(const RGBMatrixBase *matrix, const WeatherData &data) {
    if (!enable_animations->get()) return;
    
    int code = data.weatherCode;
    if (code >= 700 && code < 800) { // Fog/mist conditions
        // Initialize fog grid if empty
        if (fog_grid.empty()) {
            fog_grid.resize(matrix_width);
            for (auto& col : fog_grid) {
                col.resize(matrix_height);
                for (auto& cell : col) {
                    cell = static_cast<float>(rand() % 100) / 100.0f;
                }
            }
        }
        
        // Update fog animation
        float fog_time = animation_frame * 0.05f * animation_speed_multiplier->get();
        
        for (int x = 0; x < matrix_width; x++) {
            for (int y = 0; y < matrix_height; y++) {
                // Create rolling fog effect
                float fog_density = fog_grid[x][y];
                fog_density += 0.1f * sin(fog_time + x * 0.1f + y * 0.2f);
                fog_density = std::max(0.0f, std::min(1.0f, fog_density));
                
                uint8_t fog_color = static_cast<uint8_t>(50 + fog_density * 100);
                offscreen_canvas->SetPixel(x, y, fog_color, fog_color, fog_color + 20);
            }
        }
    }
}

void Scenes::WeatherScene::renderRainbowEffect(const RGBMatrixBase *matrix, const WeatherData &data) {
    if (!enable_rainbow->get() || !enable_animations->get()) return;
    
    // Show rainbow after rain
    int code = data.weatherCode;
    if ((code >= 500 && code < 600) && data.is_day) { // Light rain during day
        float rainbow_time = animation_frame * 0.02f;
        
        // Draw subtle rainbow arc
        int center_x = matrix_width / 2;
        int center_y = matrix_height;
        int radius = matrix_height * 0.8f;
        
        for (float angle = M_PI * 0.2f; angle < M_PI * 0.8f; angle += 0.1f) {
            int x = center_x + static_cast<int>(cos(angle) * radius);
            int y = center_y - static_cast<int>(sin(angle) * radius);
            
            if (x >= 0 && x < matrix_width && y >= 0 && y < matrix_height) {
                // Calculate rainbow color based on angle
                float hue = (angle - M_PI * 0.2f) / (M_PI * 0.6f) * 360.0f;
                float intensity = 0.3f + 0.2f * sin(rainbow_time);
                
                uint8_t r, g, b;
                // Simple HSV to RGB conversion for rainbow
                float c = intensity;
                float x_val = c * (1 - abs(fmod(hue / 60.0f, 2) - 1));
                float m = 0;
                
                if (hue < 60) { r = c * 255; g = x_val * 255; b = 0; }
                else if (hue < 120) { r = x_val * 255; g = c * 255; b = 0; }
                else if (hue < 180) { r = 0; g = c * 255; b = x_val * 255; }
                else if (hue < 240) { r = 0; g = x_val * 255; b = c * 255; }
                else if (hue < 300) { r = x_val * 255; g = 0; b = c * 255; }
                else { r = c * 255; g = 0; b = x_val * 255; }
                
                offscreen_canvas->SetPixel(x, y, r, g, b);
            }
        }
    }
}

void Scenes::WeatherScene::renderAurora(const RGBMatrixBase *matrix) {
    if (!enable_aurora->get() || !enable_animations->get()) return;
    
    // Render aurora effect for very cold temperatures or at night
    if (!data.is_day && data.temperature.find("-") != std::string::npos) {
        float aurora_time = animation_frame * 0.03f * animation_speed_multiplier->get();
        
        for (int x = 0; x < matrix_width; x++) {
            for (int y = 0; y < matrix_height / 2; y++) {
                // Create flowing aurora waves
                float wave1 = sin(aurora_time + x * 0.2f + y * 0.1f);
                float wave2 = sin(aurora_time * 1.3f + x * 0.15f);
                float intensity = (wave1 + wave2) * 0.3f + 0.4f;
                intensity *= (1.0f - static_cast<float>(y) / matrix_height);
                
                if (intensity > 0.2f) {
                    // Green-blue aurora colors
                    uint8_t r = static_cast<uint8_t>(intensity * 50);
                    uint8_t g = static_cast<uint8_t>(intensity * 200);
                    uint8_t b = static_cast<uint8_t>(intensity * 150);
                    
                    offscreen_canvas->SetPixel(x, y, r, g, b);
                }
            }
        }
    }
}
