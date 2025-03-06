#include "WeatherVisualizerScene.h"
#include <spdlog/spdlog.h>
#include <cmath>
#include <random>
#include "../Constants.h"
#include "shared/utils/utils.h"
#include <chrono>
#include <thread>
#include <ctime>
#include <iostream>

// Update interval for fetching new weather data in milliseconds
constexpr uint64_t UPDATE_INTERVAL = 5 * 60 * 1000; // 5 minutes
constexpr uint64_t TIME_CHECK_INTERVAL = 60 * 1000; // Check time every minute

WeatherVisualizerScene::WeatherVisualizerScene() {
    weather_parser = std::make_unique<WeatherParser>();
}

void WeatherVisualizerScene::register_properties() {
    // Register configurable properties
    add_property(MAKE_PROPERTY("target_fps", int, target_fps));
    add_property(MAKE_PROPERTY("time_override", bool, time_override_enabled));
}

void WeatherVisualizerScene::initialize(RGBMatrixBase *matrix, FrameCanvas *l_offscreen_canvas) {
    Scene::initialize(matrix, l_offscreen_canvas);

    // Initialize animation state
    frame_counter = 0;
    wind_offset = 0;
    sun_ray_length = 3;
    ray_growing = true;
    lightning_active = false;
    lightning_timer = 0;

    // Initialize frame limiting
    last_frame_time = GetTimeInMillis();
    frame_interval_ms = 1000 / target_fps;

    // Check the current time
    update_time_of_day();

    // Initialize weather effects with matrix dimensions
    spdlog::debug("Initializing weather with {} and {}", matrix_width, matrix_height);
    weather_effects.initialize(matrix_width, matrix_height);

    // Initialize star field if we're at night
    if (is_night_time && !star_field.is_initialized()) {
        star_field.initialize(matrix_width, matrix_height, animation_speed_factor);
    }

    // Attempt to get weather data immediately upon activation
    auto weather_result = weather_parser->get_data();
    if (weather_result) {
        current_weather = weather_result.value();
        has_error = false;

        // Initialize particles based on weather code
        if (current_weather.weatherCode >= 200 && current_weather.weatherCode < 600) {
            // Rain conditions
            initialize_particles(30, false);
        } else if (current_weather.weatherCode >= 600 && current_weather.weatherCode < 700) {
            // Snow conditions
            initialize_particles(15, true);
        }

        // Adjust frame rate for current weather and time of day
        adjust_frame_rate_for_weather();
    } else {
        has_error = true;
        error_message = weather_result.error();
        spdlog::error("Weather visualization error: {}", error_message);
    }

    last_update = GetTimeInMillis();
}

void WeatherVisualizerScene::after_render_stop(RGBMatrixBase *matrix) {
    Scene::after_render_stop(matrix);
    particles.clear();
}

bool WeatherVisualizerScene::is_night_time_now() const {
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm local_tm = *std::localtime(&now_time);

    // Consider 7 AM to 7 PM as day time, rest as night time
    return local_tm.tm_hour < 7 || local_tm.tm_hour >= 19;
}

void WeatherVisualizerScene::update_time_of_day() {
    bool was_night = is_night_time;

    // If we're using weather API's day/night data
    if (!time_override_enabled && current_weather.icon_url.find("night") != std::string::npos) {
        is_night_time = true;
    }
    // Otherwise use local system time
    else {
        is_night_time = is_night_time_now();
    }

    // Initialize stars if it's night and we haven't done so yet
    if (is_night_time && !star_field.is_initialized()) {
        star_field.initialize(matrix_width, matrix_height, animation_speed_factor);
    } else if (!is_night_time && was_night) {
        // Clear stars during transition to day
        star_field.clear();
    }

    // Adjust brightness based on time - darker at night
    brightness_factor = is_night_time ? 0.7f : 1.0f;

    spdlog::debug("Time of day updated: {} - Brightness: {:.1f}",
                  is_night_time ? "Night" : "Day", brightness_factor);
}

RGB WeatherVisualizerScene::apply_night_tint(const RGB &color) const {
    if (!is_night_time) {
        return color;
    }

    // Apply night time tint and brightness reduction
    return RGB{
        static_cast<uint8_t>(std::min(255, static_cast<int>(color.r * brightness_factor * 0.8f))),
        static_cast<uint8_t>(std::min(255, static_cast<int>(color.g * brightness_factor * 0.7f))),
        static_cast<uint8_t>(std::min(255, static_cast<int>(color.b * brightness_factor * 0.9f)))
    };
}

void WeatherVisualizerScene::adjust_frame_rate_for_weather() {
    int code = current_weather.weatherCode;

    // Base target FPS on weather type
    // More dynamic weather needs higher FPS, static conditions can use lower FPS
    if (code >= 200 && code < 300) {
        // Thunderstorm - high FPS for lightning effects
        target_fps = 40;
        animation_speed_factor = 1.2f;
    } else if (code >= 300 && code < 600) {
        // Rain - medium-high FPS
        target_fps = 35;
        animation_speed_factor = 1.1f;
    } else if (code >= 600 && code < 700) {
        // Snow - needs medium FPS but slower animations
        target_fps = 25;
        animation_speed_factor = 0.7f; // Slower for gentle snow
    } else if (code >= 700 && code < 800) {
        // Fog/mist - slow moving, low FPS is fine
        target_fps = 20;
        animation_speed_factor = 0.5f;
    } else if (code == 800) {
        // Clear sky - depends on time of day
        if (is_night_time) {
            // Night clear - twinkling stars need medium FPS
            target_fps = 30; // Higher for smoother star twinkling
            animation_speed_factor = 0.8f;
        } else {
            // Day clear - sun rays need medium-low FPS
            target_fps = 20;
            animation_speed_factor = 0.9f;
        }
    } else if (code > 800) {
        // Clouds - slow moving, low FPS is fine
        target_fps = 15;
        animation_speed_factor = 0.8f;
    } else {
        // Default - medium FPS
        target_fps = 25;
        animation_speed_factor = 1.0f;
    }

    // Adjust for night time - generally slower animations at night
    // except for star twinkling which needs to be smooth
    if (is_night_time && code != 200 && code != 800) {
        target_fps = std::max(10, target_fps - 5);
        animation_speed_factor *= 0.8f;
    }

    // Update star field animation speed
    star_field.set_speed_factor(animation_speed_factor);

    // Update the frame interval based on target FPS
    frame_interval_ms = 1000 / target_fps;
    spdlog::debug("Adjusted frame rate: FPS={}, speed factor={:.1f}, time={}",
                  target_fps, animation_speed_factor, is_night_time ? "night" : "day");
}

void WeatherVisualizerScene::limit_frame_rate() {
    uint64_t current_time = GetTimeInMillis();
    uint64_t elapsed = current_time - last_frame_time;

    if (elapsed < frame_interval_ms) {
        // Sleep for the remaining time to achieve desired frame rate
        uint64_t sleep_time = frame_interval_ms - elapsed;
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
        current_time = GetTimeInMillis();
    }

    // Calculate actual delta time for smooth animation regardless of frame rate
    float delta_time = (current_time - last_frame_time) / 1000.0f; // Convert to seconds

    // Update stars with actual elapsed time for consistent animation speed
    if (is_night_time && star_field.is_initialized()) {
        star_field.update(delta_time);
    }

    // Update timing variables
    last_frame_time = current_time;
}

void WeatherVisualizerScene::initialize_particles(int count, bool is_snow) {
    particles.clear();
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> x_dist(0, matrix_width - 1);
    std::uniform_int_distribution<> y_dist(-15, matrix_height - 1);
    std::uniform_int_distribution<> speed_dist(is_snow ? 1 : 2, is_snow ? 2 : 3);
    std::uniform_int_distribution<> drift_dist(-1, 1);

    for (int i = 0; i < count; ++i) {
        WeatherParticle particle;
        particle.x = x_dist(gen);
        particle.y = y_dist(gen);
        particle.speed = speed_dist(gen);
        particle.drift = is_snow ? drift_dist(gen) : 0; // Only snow drifts

        particles.push_back(particle);
    }
}

void WeatherVisualizerScene::update_particles(bool is_snow) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> x_dist(0, matrix_width - 1);

    for (auto &p: particles) {
        // Apply animation speed factor to make movement weather-appropriate
        float speed_adjustment = is_snow ? animation_speed_factor * 0.8f : animation_speed_factor;

        // Move particles down with adjusted speed
        if (frame_counter % std::max(1, static_cast<int>(2 / speed_adjustment)) == 0) {
            p.y += p.speed;

            // Handle horizontal drift for snow
            if (is_snow && frame_counter % std::max(1, static_cast<int>(6 / speed_adjustment)) == 0) {
                p.x += p.drift;

                // Keep within bounds
                if (p.x < 0) p.x = 0;
                if (p.x >= matrix_width) p.x = matrix_width - 1;
            }
        }

        // Reset particles that go off screen
        if (p.y >= matrix_height) {
            p.y = -2;
            p.x = x_dist(gen);
        }
    }
}

RGB WeatherVisualizerScene::get_color_for_weather_code() {
    // Return appropriate background color based on weather code
    int code = current_weather.weatherCode;

    RGB color;

    // Thunderstorm
    if (code >= 200 && code < 300) color = {40, 40, 60};

        // Rain
    else if (code >= 300 && code < 600) color = {60, 80, 120};

        // Snow
    else if (code >= 600 && code < 700) color = {200, 200, 220};

        // Fog/mist
    else if (code >= 700 && code < 800) color = {150, 150, 150};

        // Clear sky
    else if (code == 800) {
        if (is_night_time) {
            color = {10, 10, 40}; // Night
        } else {
            color = {100, 150, 255}; // Day
        }
    }

    // Clouds
    else if (code > 800) color = {120, 120, 140};

        // Default
    else color = {0, 0, 0};

    // Apply night time tinting if it's night
    return apply_night_tint(color);
}

void WeatherVisualizerScene::render_based_on_weather_code(Canvas *canvas) {
    int code = current_weather.weatherCode;

    // Update the wind offset for cloud movement
    weather_effects.update_wind(frame_counter, animation_speed_factor);
    int current_wind_offset = weather_effects.get_wind_offset();

    // Render based on weather code
    // Thunderstorm: 200-299
    if (code >= 200 && code < 300) {
        // Update rain particles
        update_particles(false);

        // Adjust lightning frequency based on animation speed
        int lightning_interval = std::max(10, static_cast<int>(40 / animation_speed_factor));

        // Occasionally create new lightning
        if (frame_counter % lightning_interval == 0 && !lightning_active) {
            lightning_active = true;
            lightning_timer = 5 + (frame_counter % 6); // Random duration between 5-10 frames
        }

        // Process active lightning
        if (lightning_active) {
            // Decrease the flash counter
            lightning_timer--;
            if (lightning_timer <= 0) {
                lightning_active = false;
            }
        }

        // Render the thunderstorm with lightning effect
        weather_effects.draw_lightning(canvas, lightning_active, current_wind_offset, is_night_time, particles);
    }
    // Drizzle: 300-399
    else if (code >= 300 && code < 400) {
        update_particles(false);
        weather_effects.draw_clear_sky(canvas, is_night_time);
        if (is_night_time) star_field.render(canvas);
        weather_effects.draw_clouds(canvas, current_wind_offset, false, is_night_time);
        weather_effects.draw_rain(canvas, particles);
    }
    // Rain: 400-599
    else if (code >= 400 && code < 600) {
        update_particles(false);
        weather_effects.draw_clear_sky(canvas, is_night_time);
        if (is_night_time) star_field.render(canvas);
        weather_effects.draw_clouds(canvas, current_wind_offset, true, is_night_time);
        weather_effects.draw_rain(canvas, particles);
    }
    // Snow: 600-699
    else if (code >= 600 && code < 700) {
        update_particles(true);
        weather_effects.draw_clear_sky(canvas, is_night_time);
        if (is_night_time) star_field.render(canvas);
        weather_effects.draw_clouds(canvas, current_wind_offset, false, is_night_time);
        weather_effects.draw_snow(canvas, particles);
    }
    // Fog, mist: 700-799
    else if (code >= 700 && code < 800) {
        weather_effects.draw_clear_sky(canvas, is_night_time);
        if (is_night_time) star_field.render(canvas);
        weather_effects.draw_fog(canvas, frame_counter, is_night_time);
    }
    // Clear sky: 800
    else if (code == 800) {
        weather_effects.draw_clear_sky(canvas, is_night_time);
        if (is_night_time) {
            star_field.render(canvas);
            weather_effects.draw_moon(canvas);
        } else {
            weather_effects.draw_sun(canvas, frame_counter, animation_speed_factor);
        }
    }
    // Clouds: 801-899
    else if (code > 800) {
        weather_effects.draw_clear_sky(canvas, is_night_time);
        if (is_night_time) star_field.render(canvas);
        weather_effects.draw_clouds(canvas, current_wind_offset, code >= 803, is_night_time);
    }
    // Unknown
    else {
        // Default animation for unknown weather code
        weather_effects.draw_clear_sky(canvas, is_night_time);
        if (is_night_time) star_field.render(canvas);
    }
}

bool WeatherVisualizerScene::render(rgb_matrix::RGBMatrixBase *matrix) {
    // Initialize if needed
    if (!initialized) {
        std::cout << "Activate" << std::flush;
        initialized = true;
        return false;
    }

    std::cout << "W " << matrix_width << " " << matrix_height << std::flush;

    uint64_t now = GetTimeInMillis();
    return true;

    // Check if it's time to update weather data
    if (now - last_update >= UPDATE_INTERVAL) {
        auto weather_result = weather_parser->get_data();
        if (weather_result) {
            current_weather = weather_result.value();
            has_error = false;

            // Check time of day again with fresh data
            update_time_of_day();

            // Reinitialize particles based on new weather condition
            if (current_weather.weatherCode >= 200 && current_weather.weatherCode < 600) {
                // Rain conditions
                initialize_particles(30, false);
            } else if (current_weather.weatherCode >= 600 && current_weather.weatherCode < 700) {
                // Snow conditions
                initialize_particles(15, true);
            }

            // Adjust frame rate for the new weather conditions and time of day
            adjust_frame_rate_for_weather();
        } else {
            has_error = true;
            error_message = weather_result.error();
            spdlog::error("Weather visualization error: {}", error_message);
        }
        last_update = now;
    }
    // Check time of day on a regular basis
    else if (now % TIME_CHECK_INTERVAL < 1000) {
        // Check approximately once per minute
        update_time_of_day();
        adjust_frame_rate_for_weather();
    }

    // Clear canvas
    offscreen_canvas->Fill(0, 0, 0);

    if (has_error) {
        // Display error message
        rgb_matrix::DrawText(offscreen_canvas, BODY_FONT, 2, 15, rgb_matrix::Color(255, 0, 0),
                             nullptr, "Weather Error", 0);
    } else {
        // Render the weather visualization
        render_based_on_weather_code(offscreen_canvas);
    }

    offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas);

    // Increment frame counter for animations
    frame_counter++;

    // Apply frame rate limiting - this also updates the star twinkle animation
    limit_frame_rate();

    return true; // Return true to indicate that the scene should continue rendering
}
