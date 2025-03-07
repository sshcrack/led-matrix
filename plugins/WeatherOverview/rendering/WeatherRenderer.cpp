#include "WeatherRenderer.h"
#include "../Constants.h"
#include <cmath>
#include <algorithm>

namespace WeatherRenderer {
    RGB get_sky_color(bool is_day, int weather_code) {
        uint32_t color;

        // Determine base color from weather and time
        if (is_day) {
            // Day time colors
            if (weather_code <= 1) {
                color = SkyColor::DAY_CLEAR; // Clear sky
            } else if (weather_code <= 3) {
                color = SkyColor::DAY_CLOUDS; // Partly cloudy
            } else if (weather_code >= 45 && weather_code <= 48) {
                color = 0x8598B0; // Foggy - grayish blue
            } else if ((weather_code >= 51 && weather_code <= 67) ||
                       (weather_code >= 80 && weather_code <= 82)) {
                color = 0x546A8C; // Rainy - darker blue
            } else if (weather_code >= 71 && weather_code <= 77 ||
                       weather_code >= 85 && weather_code <= 86) {
                color = 0x7080A5; // Snowy - light grayish blue
            } else if (weather_code >= 95) {
                color = 0x303F5A; // Thunderstorm - dark blue/gray
            } else {
                color = SkyColor::DAY_CLOUDS; // Default
            }
        } else {
            // Night time colors
            if (weather_code <= 1) {
                color = SkyColor::NIGHT_CLEAR; // Clear night
            } else if (weather_code <= 3) {
                color = SkyColor::NIGHT_CLOUDS; // Partly cloudy night
            } else if (weather_code >= 45 && weather_code <= 48) {
                color = 0x0A1524; // Foggy night - dark blue
            } else if ((weather_code >= 51 && weather_code <= 67) ||
                       (weather_code >= 80 && weather_code <= 82)) {
                color = 0x070E18; // Rainy night - darker blue
            } else if (weather_code >= 71 && weather_code <= 77 ||
                       weather_code >= 85 && weather_code <= 86) {
                color = 0x091524; // Snowy night - dark blue
            } else if (weather_code >= 95) {
                color = 0x050a10; // Thunderstorm night - very dark blue
            } else {
                color = SkyColor::NIGHT_CLOUDS; // Default night
            }
        }

        return {
            static_cast<uint8_t>((color >> 16) & 0xFF),
            static_cast<uint8_t>((color >> 8) & 0xFF),
            static_cast<uint8_t>(color & 0xFF)
        };
    }

    void initialize_stars(std::vector<WeatherElements::Star>& stars, int count, 
                          int matrix_width, int matrix_height, std::mt19937& rng) {
        std::uniform_int_distribution<> x_dist(0, matrix_width - 1);
        std::uniform_int_distribution<> y_dist(0, matrix_height / 2); // Stars mostly in upper half
        std::uniform_int_distribution<> brightness_dist(100, 255);
        std::uniform_real_distribution<> speed_dist(0.2f, 1.0f);
        std::uniform_real_distribution<> phase_dist(0.0f, 6.28f);

        stars.resize(count);
        for (auto &star: stars) {
            star.x = x_dist(rng);
            star.y = y_dist(rng);
            star.brightness = brightness_dist(rng);
            star.twinkle_speed = speed_dist(rng);
            star.twinkle_phase = phase_dist(rng);
        }
    }

    void initialize_raindrops(std::vector<WeatherElements::Raindrop>& raindrops, int count,
                             int matrix_width, int matrix_height, std::mt19937& rng, float animation_speed) {
        std::uniform_int_distribution<> x_dist(0, matrix_width - 1);
        std::uniform_int_distribution<> y_dist(-20, matrix_height - 1);
        std::uniform_real_distribution<> speed_dist(0.8f, 2.5f);
        std::uniform_int_distribution<> length_dist(2, 5);
        std::uniform_int_distribution<> alpha_dist(120, 200);

        raindrops.resize(count);
        for (auto &drop: raindrops) {
            drop.x = x_dist(rng);
            drop.y = y_dist(rng);
            drop.speed = speed_dist(rng) * animation_speed;
            drop.length = length_dist(rng);
            drop.alpha = alpha_dist(rng);
        }
    }

    void initialize_snowflakes(std::vector<WeatherElements::Snowflake>& snowflakes, int count,
                              int matrix_width, int matrix_height, std::mt19937& rng, float animation_speed) {
        std::uniform_int_distribution<> x_dist(0, matrix_width - 1);
        std::uniform_int_distribution<> y_dist(-20, matrix_height - 1);
        std::uniform_real_distribution<> speed_dist(0.2f, 0.8f);
        std::uniform_real_distribution<> drift_dist(-0.3f, 0.3f);
        std::uniform_int_distribution<> size_dist(1, 2);
        std::uniform_real_distribution<> angle_dist(0.0f, 6.28f);
        std::uniform_real_distribution<> rotation_dist(-0.1f, 0.1f);

        snowflakes.resize(count);
        for (auto &flake: snowflakes) {
            flake.x = x_dist(rng);
            flake.y = y_dist(rng);
            flake.speed = speed_dist(rng) * animation_speed;
            flake.drift = drift_dist(rng) * animation_speed;
            flake.size = size_dist(rng);
            flake.angle = angle_dist(rng);
            flake.rotation_speed = rotation_dist(rng) * animation_speed;
        }
    }

    void initialize_clouds(std::vector<WeatherElements::Cloud>& clouds, int count,
                          int matrix_width, int matrix_height, std::mt19937& rng, float animation_speed) {
        std::uniform_int_distribution<> x_dist(-10, matrix_width + 10);
        std::uniform_int_distribution<> y_dist(2, matrix_height / 2);
        std::uniform_real_distribution<> speed_dist(-0.05f, 0.05f);
        std::uniform_int_distribution<> width_dist(8, 16);
        std::uniform_int_distribution<> height_dist(4, 7);
        std::uniform_int_distribution<> opacity_dist(170, 230);

        clouds.resize(count);
        for (auto &cloud: clouds) {
            cloud.x = x_dist(rng);
            cloud.y = y_dist(rng);
            cloud.speed = speed_dist(rng) * animation_speed;
            cloud.width = width_dist(rng);
            cloud.height = height_dist(rng);
            cloud.opacity = opacity_dist(rng);
        }
    }

    // Rendering functions for different weather conditions
    void render_sun(rgb_matrix::Canvas *canvas, int matrix_width, int matrix_height, float elapsed_time) {
        int sun_x = matrix_width * 3 / 4;
        int sun_y = matrix_height / 4;
        int radius = 4;
        int ray_length = 3;  // Fixed ray length for all sun beams

        // Draw the sun
        for (int dx = -radius; dx <= radius; dx++) {
            for (int dy = -radius; dy <= radius; dy++) {
                float distance = sqrt(dx * dx + dy * dy);
                if (distance <= radius) {
                    int x = sun_x + dx;
                    int y = sun_y + dy;
                    if (x >= 0 && x < matrix_width && y >= 0 && y < matrix_height) {
                        int brightness = 255 * (1.0f - distance / radius * 0.5f);
                        // Sun core is yellow-orange
                        canvas->SetPixel(x, y, brightness, brightness * 0.8, brightness * 0.2);
                    }
                }
            }
        }

        // Draw sun rays with consistent yellow color
        float time_offset = elapsed_time * 0.5f;
        for (int i = 0; i < 8; i++) {
            float angle = i * M_PI / 4.0f + time_offset;
            int x = sun_x + cos(angle) * (radius + 1);
            int y = sun_y + sin(angle) * (radius + 1);

            for (int j = 0; j < ray_length; j++) {
                int ray_x = x + cos(angle) * j;
                int ray_y = y + sin(angle) * j;
                if (ray_x >= 0 && ray_x < matrix_width && ray_y >= 0 && ray_y < matrix_height) {
                    // Consistent yellow color for rays with slight fade at the edges
                    int brightness = 255 * (1.0f - static_cast<float>(j) / ray_length);
                    canvas->SetPixel(ray_x, ray_y, brightness, brightness, brightness * 0.4);
                }
            }
        }
    }

    void render_moon(rgb_matrix::Canvas *canvas, int matrix_width, int matrix_height) {
        int moon_x = matrix_width * 3 / 4;
        int moon_y = matrix_height / 4;
        int radius = 3;

        // Draw the moon
        for (int dx = -radius; dx <= radius; dx++) {
            for (int dy = -radius; dy <= radius; dy++) {
                float distance = sqrt(dx * dx + dy * dy);
                if (distance <= radius) {
                    // Crescent moon effect
                    float crescent = (dx + radius * 0.5f) / (radius * 2.0f);
                    if (crescent > 0.5f) {
                        int x = moon_x + dx;
                        int y = moon_y + dy;
                        if (x >= 0 && x < matrix_width && y >= 0 && y < matrix_height) {
                            int brightness = 200 * (1.0f - distance / radius * 0.7f);
                            canvas->SetPixel(x, y, brightness, brightness, brightness);
                        }
                    }
                }
            }
        }
    }

    void render_clear_day(rgb_matrix::Canvas *canvas, int matrix_width, int matrix_height, float elapsed_time) {
        render_sun(canvas, matrix_width, matrix_height, elapsed_time);
    }

    void render_clear_night(rgb_matrix::Canvas *canvas, 
                           std::vector<WeatherElements::Star>& stars,
                           int matrix_width, int matrix_height, float elapsed_time) {
        // Render stars
        for (auto &star: stars) {
            float phase = star.twinkle_phase + elapsed_time * star.twinkle_speed;
            uint8_t current_brightness = star.brightness * (0.7f + 0.3f * sin(phase));
            canvas->SetPixel(star.x, star.y, current_brightness, current_brightness, current_brightness);
        }

        render_moon(canvas, matrix_width, matrix_height);
    }

    void render_cloudy(rgb_matrix::Canvas *canvas, 
                      std::vector<WeatherElements::Cloud>& clouds,
                      int matrix_width, int matrix_height,
                      bool is_day, int cloud_coverage, float elapsed_time) {
        // Update and render clouds
        for (auto &cloud: clouds) {
            cloud.x += cloud.speed;
            if (cloud.x < -cloud.width)
                cloud.x = matrix_width + cloud.width;
            if (cloud.x > matrix_width + cloud.width)
                cloud.x = -cloud.width;

            // Draw cloud as a rounded shape
            for (int dx = 0; dx < cloud.width; dx++) {
                for (int dy = 0; dy < cloud.height; dy++) {
                    // Create a cloud-like shape
                    float distance = sqrt(pow(dx - cloud.width / 2, 2) + pow(dy - cloud.height / 2, 2));
                    if (distance < cloud.width / 2) {
                        int x = cloud.x + dx;
                        int y = cloud.y + dy;
                        if (x >= 0 && x < matrix_width && y >= 0 && y < matrix_height) {
                            int alpha = cloud.opacity * (1.0f - distance / (cloud.width / 2));
                            canvas->SetPixel(x, y, 255, 255, 255);
                        }
                    }
                }
            }
        }

        // Render sun or moon behind clouds if not too cloudy
        if (cloud_coverage < 5) {
            if (is_day) {
                render_sun(canvas, matrix_width, matrix_height, elapsed_time);
            } else {
                render_moon(canvas, matrix_width, matrix_height);
            }
        }
    }

    void render_rain(rgb_matrix::Canvas *canvas, 
                    std::vector<WeatherElements::Cloud>& clouds,
                    std::vector<WeatherElements::Raindrop>& raindrops,
                    int matrix_width, int matrix_height, int intensity) {
        // First render clouds
        render_cloudy(canvas, clouds, matrix_width, matrix_height, false, 8, 0);

        // Update and render raindrops
        for (auto &drop: raindrops) {
            drop.y += drop.speed;
            if (drop.y > matrix_height) {
                drop.y = -5;
                std::uniform_int_distribution<> x_dist(0, matrix_width - 1);
                std::mt19937 temp_rng; // Create a temporary RNG for this operation
                drop.x = x_dist(temp_rng);
            }

            // Draw raindrop as a line
            for (int i = 0; i < drop.length; i++) {
                int y = drop.y - i;
                if (y >= 0 && y < matrix_height) {
                    int alpha = drop.alpha * (1.0f - static_cast<float>(i) / drop.length);
                    int color_value = 200 + alpha / 4;
                    canvas->SetPixel(drop.x, y, color_value / 2, color_value / 2, color_value);
                }
            }
        }
    }

    void render_snow(rgb_matrix::Canvas *canvas, 
                    std::vector<WeatherElements::Cloud>& clouds,
                    std::vector<WeatherElements::Snowflake>& snowflakes,
                    int matrix_width, int matrix_height, int intensity) {
        // First render clouds
        render_cloudy(canvas, clouds, matrix_width, matrix_height, false, 8, 0);

        // Update and render snowflakes
        for (auto &flake: snowflakes) {
            flake.y += flake.speed;
            flake.x += flake.drift;
            flake.angle += flake.rotation_speed;

            if (flake.y > matrix_height) {
                flake.y = -5;
                std::uniform_int_distribution<> x_dist(0, matrix_width - 1);
                std::mt19937 temp_rng; // Create a temporary RNG for this operation
                flake.x = x_dist(temp_rng);
            }

            if (flake.x < 0)
                flake.x += matrix_width;

            // Draw snowflake
            int x = static_cast<int>(flake.x);
            int y = static_cast<int>(flake.y);

            if (flake.size == 1) {
                // Small snowflake (single pixel)
                if (x >= 0 && x < matrix_width && y >= 0 && y < matrix_height) {
                    canvas->SetPixel(x, y, 220, 220, 255);
                }
            } else {
                // Larger snowflake (cross pattern)
                if (y >= 0 && y < matrix_height && x >= 0 && x < matrix_width) {
                    canvas->SetPixel(x, y, 220, 220, 255);
                }
                if (y + 1 >= 0 && y + 1 < matrix_height && x >= 0 && x < matrix_width) {
                    canvas->SetPixel(x, y + 1, 200, 200, 235);
                }
                if (y - 1 >= 0 && y - 1 < matrix_height && x >= 0 && x < matrix_width) {
                    canvas->SetPixel(x, y - 1, 200, 200, 235);
                }
                if (y >= 0 && y < matrix_height && x + 1 >= 0 && x + 1 < matrix_width) {
                    canvas->SetPixel(x + 1, y, 200, 200, 235);
                }
                if (y >= 0 && y < matrix_height && x - 1 >= 0 && x - 1 < matrix_width) {
                    canvas->SetPixel(x - 1, y, 200, 200, 235);
                }
            }
        }
    }

    void render_thunderstorm(rgb_matrix::Canvas *canvas, 
                            std::vector<WeatherElements::Cloud>& clouds,
                            std::vector<WeatherElements::Raindrop>& raindrops,
                            std::vector<WeatherElements::LightningBolt>& lightning_bolts,
                            int matrix_width, int matrix_height,
                            float elapsed_time, std::chrono::time_point<std::chrono::steady_clock> start_time,
                            std::mt19937& rng) {
        // First render rain
        render_rain(canvas, clouds, raindrops, matrix_width, matrix_height, 80);

        // Update and render lightning
        auto now = std::chrono::steady_clock::now();
        auto time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();

        std::uniform_int_distribution<> lightning_chance(0, 1000);
        if (lightning_chance(rng) < 5) {
            // Small chance of lightning on any frame
            for (auto &bolt: lightning_bolts) {
                if (!bolt.active) {
                    bolt.active = true;
                    bolt.start_time = time_ms;
                    std::uniform_int_distribution<> x_dist(0, matrix_width - 1);
                    bolt.x = x_dist(rng);
                    break;
                }
            }
        }

        // Draw active lightning bolts
        for (auto &bolt: lightning_bolts) {
            if (bolt.active) {
                int elapsed = time_ms - bolt.start_time;
                if (elapsed > bolt.duration) {
                    bolt.active = false;
                    continue;
                }

                // Calculate brightness based on time (start bright, then fade)
                int brightness = (elapsed < 100)
                                    ? 255
                                    : 255 * (1.0f - (elapsed - 100) / static_cast<float>(bolt.duration - 100));

                // Draw zigzag lightning bolt
                int x = bolt.x;
                for (int y = 0; y < matrix_height; y += 3) {
                    std::uniform_int_distribution<> zigzag(-1, 1);
                    x += zigzag(rng);
                    if (x < 0)
                        x = 0;
                    if (x >= matrix_width)
                        x = matrix_width - 1;

                    canvas->SetPixel(x, y, brightness, brightness, brightness);
                    if (x > 0)
                        canvas->SetPixel(x - 1, y, brightness / 2, brightness / 2, brightness / 2);
                    if (x < matrix_width - 1)
                        canvas->SetPixel(x + 1, y, brightness / 2, brightness / 2, brightness / 2);
                }
            }
        }
    }

    void render_fog(rgb_matrix::Canvas *canvas,
                   std::vector<WeatherElements::Cloud>& clouds,
                   int matrix_width, int matrix_height) {
        // Render clouds low to the ground
        for (auto &cloud: clouds) {
            cloud.x += cloud.speed;
            if (cloud.x < -cloud.width)
                cloud.x = matrix_width + cloud.width;
            if (cloud.x > matrix_width + cloud.width)
                cloud.x = -cloud.width;

            // Draw fog as horizontal lines with varying opacity
            for (int dy = 0; dy < cloud.height; dy++) {
                int y = cloud.y + dy;
                if (y >= 0 && y < matrix_height) {
                    for (int dx = 0; dx < cloud.width; dx++) {
                        int x = (static_cast<int>(cloud.x) + dx) % matrix_width;
                        if (x >= 0) {
                            int opacity = cloud.opacity * (1.0f - fabs(dy - cloud.height / 2) / (cloud.height / 2));
                            canvas->SetPixel(x, y, 200, 200, 220);
                        }
                    }
                }
            }
        }
    }
}
