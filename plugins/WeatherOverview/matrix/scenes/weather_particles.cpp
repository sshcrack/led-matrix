#include "WeatherScene.h"
#include "../Constants.h"
#include <cmath>
#include <shared/matrix/utils/utils.h>

void Scenes::WeatherScene::initializeParticles()
{
    particles.resize(MAX_PARTICLES);
    for (auto &p : particles)
    {
        p.active = false;
    }
    active_particles = 0;
}

void Scenes::WeatherScene::resetStars()
{
    this->stars.clear();

    std::uniform_int_distribution<int> x_dist(0, matrix_width);
    std::uniform_int_distribution<int> y_dist(0, matrix_height);
    for (int i = 0; i < 20; i++)
    {
        stars.emplace_back(x_dist(rng), y_dist(rng));
    }
}

void Scenes::WeatherScene::tryCreateShootingStar()
{
    if (data.is_day)
    {
        return;
    }

    int active_count = 0;
    for (const auto &star : shooting_stars)
    {
        if (star.active)
            active_count++;
    }

    if (active_count >= MAX_SHOOTING_STARS)
    {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_shooting_star_time).count();

    if (elapsed < MIN_MS_BETWEEN_STARS || animation_frame <= shooting_star_frame_threshold->get())
    {
        return;
    }

    const int chance = shooting_star_chance->get();
    if (chance <= 0)
        return;

    if (std::uniform_int_distribution<int>(0, 99)(rng) < chance)
    {
        if (shooting_stars.size() < MAX_SHOOTING_STARS)
        {
            shooting_stars.resize(shooting_stars.size() + 1);
        }

        for (auto &star : shooting_stars)
        {
            if (!star.active)
            {
                bool from_top = std::uniform_int_distribution<int>(0, 1)(rng);
                bool from_left = std::uniform_int_distribution<int>(0, 1)(rng);

                if (from_top)
                {
                    star.x = std::uniform_int_distribution<int>(0, matrix_width - 1)(rng);
                    star.y = 0;
                }
                else
                {
                    star.x = from_left ? 0 : matrix_width - 1;
                    star.y = std::uniform_int_distribution<int>(0, matrix_height / 2 - 1)(rng);
                }

                std::uniform_real_distribution<float> sdist(SHOOTING_STAR_SPEED_MIN, SHOOTING_STAR_SPEED_MAX);
                float speed = sdist(rng);

                star.dx = from_left ? speed : -speed;
                star.dy = speed;

                std::uniform_real_distribution<float> tail_dist(3.0f, 8.0f);
                std::uniform_real_distribution<float> bright_dist(150.0f, 255.0f);
                star.tail_length = tail_dist(rng);
                star.brightness = bright_dist(rng);
                star.active = true;

                last_shooting_star_time = std::chrono::steady_clock::now();
                break;
            }
        }
    }
}

void Scenes::WeatherScene::updateShootingStars()
{
    for (auto &star : shooting_stars)
    {
        if (star.active)
        {
            star.x += star.dx;
            star.y += star.dy;

            if (star.x < -star.tail_length || star.x >= matrix_width + star.tail_length ||
                star.y < -star.tail_length || star.y >= matrix_height + star.tail_length)
            {
                star.active = false;
            }
        }
    }
}

void Scenes::WeatherScene::renderShootingStars(rgb_matrix::FrameCanvas *canvas)
{
    for (const auto &star : shooting_stars)
    {
        if (star.active)
        {
            for (float i = 0; i <= star.tail_length; i++)
            {
                float tail_x = star.x - (star.dx * i / star.dy);
                float tail_y = star.y - i;

                float brightness_factor = 1.0f - (i / star.tail_length);
                uint8_t b = static_cast<uint8_t>(star.brightness * brightness_factor);

                int px = static_cast<int>(tail_x);
                int py = static_cast<int>(tail_y);
                if (px >= 0 && px < matrix_width && py >= 0 && py < matrix_height)
                {
                    SetPixelAlpha(canvas, px, py, 255, 255, 255, ((float)b / 255.0f));
                }
            }
        }
    }
}

void Scenes::WeatherScene::updateEnhancedParticles(const WeatherData &data)
{
    if (particles.empty()) {
        initializeParticles();
    }

    const int code = data.weatherCode;
    bool is_snow =
        (code >= 70 && code <= 79) ||
        (code >= 85 && code <= 88) ||
        (code == 93 || code == 94);

    bool is_rain =
        (code >= 50 && code <= 69) ||
        (code >= 80 && code <= 84) ||
        (code == 91 || code == 92);

    has_precipitation = is_snow || is_rain;

    if (!has_precipitation || !enable_animations->get())
    {
        active_particles = 0;
        for (auto &p : particles)
        {
            p.active = false;
        }
        return;
    }

    int target_particles = std::min(MAX_PARTICLES, particle_density->get() * 5);
    float speed_mult = animation_speed_multiplier->get();

    for (auto &p : particles)
    {
        if (p.active)
        {
            p.wind_factor += 0.1f;
            float wind_offset = sin(p.wind_factor) * 0.3f;

            p.x += wind_offset;
            p.y += p.speed * speed_mult;
            p.life_time += 1.0f / static_cast<float>(std::max(1, get_target_fps()));

            if (is_snow)
            {
                p.rotation += 0.05f;
            }

            if (p.life_time > 2.0f)
            {
                p.opacity *= 0.98f;
            }

            if (p.y > matrix_height || p.x < -5 || p.x > matrix_width + 5 || p.opacity < 10)
            {
                p.active = false;
                active_particles--;
            }
        }
    }

    if (active_particles < target_particles)
    {
        for (auto &p : particles)
        {
            if (!p.active)
            {
                p.active = true;
                p.x = std::uniform_int_distribution<int>(-5, matrix_width + 4)(rng);
                p.y = -5;
                p.life_time = 0;
                p.wind_factor = std::uniform_int_distribution<int>(0, 359)(rng) * 0.0174f;
                p.rotation = 0;

                if (is_snow)
                {
                    std::uniform_int_distribution<int> sz(1, 3);
                    p.size = sz(rng);
                    p.speed = std::uniform_int_distribution<int>(1, 8)(rng) / 10.0f + 0.1f;
                    p.opacity = std::uniform_int_distribution<int>(180, 254)(rng);
                    p.r = std::uniform_int_distribution<int>(240, 254)(rng);
                    p.g = std::uniform_int_distribution<int>(240, 254)(rng);
                    p.b = std::uniform_int_distribution<int>(250, 254)(rng);
                }
                else if (is_rain)
                {
                    p.size = 1.0f;
                    p.speed = std::uniform_int_distribution<int>(8, 27)(rng) / 10.0f;
                    p.opacity = std::uniform_int_distribution<int>(150, 254)(rng);
                    p.r = std::uniform_int_distribution<int>(150, 199)(rng);
                    p.g = std::uniform_int_distribution<int>(180, 229)(rng);
                    p.b = std::uniform_int_distribution<int>(200, 254)(rng);
                }

                active_particles++;
                if (active_particles >= target_particles)
                {
                    break;
                }
            }
        }
    }
}

void Scenes::WeatherScene::renderEnhancedParticles(rgb_matrix::FrameCanvas *canvas, const WeatherData &data)
{
    if (!has_precipitation || !enable_animations->get())
    {
        return;
    }

    const int code = data.weatherCode;
    bool is_snow =
        (code >= 70 && code <= 79) ||
        (code >= 85 && code <= 88) ||
        (code == 93 || code == 94);

    for (const auto &p : particles)
    {
        if (p.active)
        {
            int px = static_cast<int>(p.x);
            int py = static_cast<int>(p.y);

            if (px >= 0 && px < matrix_width && py >= 0 && py < matrix_height)
            {
                uint8_t max = 255;
                if (is_snow)
                {
                    float alpha = p.opacity / 255.0f;

                    SetPixelAlpha(canvas, px, py, std::min(max, p.r), std::min(max, p.g), std::min(max, p.b), alpha);

                    if (p.size > 1.5f)
                    {
                        float cos_r = cos(p.rotation);
                        float sin_r = sin(p.rotation);

                        for (int i = -1; i <= 1; i++)
                        {
                            for (int j = -1; j <= 1; j++)
                            {
                                if (i == 0 && j == 0)
                                    continue;

                                int rx = px + static_cast<int>(i * cos_r - j * sin_r);
                                int ry = py + static_cast<int>(i * sin_r + j * cos_r);

                                if (rx >= 0 && rx < matrix_width && ry >= 0 && ry < matrix_height)
                                {
                                    float sub_alpha = alpha * 0.6f;
                                    SetPixelAlpha(canvas, rx, ry, std::min(max, p.r), std::min(max, p.g), std::min(max, p.b), sub_alpha);
                                }
                            }
                        }
                    }
                }
                else
                {
                    float alpha = p.opacity / 255.0f;

                    for (int i = 0; i < 3; i++)
                    {
                        int ry = py - i;
                        if (ry >= 0 && ry < matrix_height)
                        {
                            float streak_alpha = alpha * (1.0f - i * (1.0f / 3.0f));
                            SetPixelAlpha(canvas, px, ry, std::min(max, p.r), std::min(max, p.g), std::min(max, p.b), streak_alpha);
                        }
                    }
                }
            }
        }
    }
}
