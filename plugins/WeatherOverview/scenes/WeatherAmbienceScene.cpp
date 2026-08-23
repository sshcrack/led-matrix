#include "WeatherAmbienceScene.h"

#include "shared/matrix/utils/utils.h"
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>

namespace Scenes {
namespace {
constexpr float Pi = 3.14159265358979323846f;

bool is_rain_code(int code) {
    return (code >= 51 && code <= 67) || (code >= 80 && code <= 82);
}
bool is_snow_code(int code) {
    return (code >= 71 && code <= 77) || (code >= 85 && code <= 86);
}
bool is_fog_code(int code) { return code == 45 || code == 48; }
bool is_storm_code(int code) { return code >= 95 && code <= 99; }
bool is_cloudy_code(int code) { return code >= 1 && code <= 3; }

void alpha_pixel(rgb_matrix::FrameCanvas *canvas, int x, int y,
                 uint8_t r, uint8_t g, uint8_t b, float alpha) {
    if (x < 0 || y < 0 || x >= canvas->width() || y >= canvas->height()) return;
    SetPixelAlpha(canvas, x, y, r, g, b, std::clamp(alpha, 0.0f, 1.0f));
}
}

WeatherAmbienceScene::WeatherAmbienceScene() {
    set_target_fps(30);
}

std::unique_ptr<Scenes::Scene> WeatherAmbienceSceneWrapper::create() {
    return std::make_unique<WeatherAmbienceScene>();
}

void WeatherAmbienceScene::register_properties() {
    location_lat_->label("Latitude").group("Location");
    location_lon_->label("Longitude").group("Location");
    mode_->label("Weather mode").description("Auto follows live weather; the other modes are visual overrides.").group("Atmosphere");
    intensity_->label("Intensity").description("Density and visual strength of the weather effect.").group("Atmosphere");
    animation_speed_->label("Animation speed").group("Motion").step(0.05);
    particle_limit_->label("Particle limit").description("Upper CPU budget for rain/snow particles. Adaptive quality may lower this automatically.").group("Performance");
    wind_response_->label("Wind response").description("How strongly live wind speed pushes clouds and precipitation.").group("Motion").step(0.05);
    refresh_minutes_->label("Weather refresh").description("Minutes between live weather refreshes.").group("Location");
    lightning_enabled_->label("Lightning").group("Atmosphere");

    add_property(location_lat_); add_property(location_lon_); add_property(mode_);
    add_property(intensity_); add_property(animation_speed_); add_property(particle_limit_);
    add_property(wind_response_); add_property(refresh_minutes_); add_property(lightning_enabled_);
}

WeatherAmbienceMode WeatherAmbienceScene::effective_mode() const {
    const auto selected = mode_->get().get();
    if (selected != WeatherAmbienceMode::AUTO) return selected;
    const int code = weather_.weatherCode;
    if (is_storm_code(code)) return WeatherAmbienceMode::STORM;
    if (is_snow_code(code)) return WeatherAmbienceMode::SNOW;
    if (is_rain_code(code)) return WeatherAmbienceMode::RAIN;
    if (is_fog_code(code)) return WeatherAmbienceMode::FOG;
    if (is_cloudy_code(code) || weather_.cloud_cover > 0.45f) return WeatherAmbienceMode::CLOUDY;
    return WeatherAmbienceMode::CLEAR;
}

bool WeatherAmbienceScene::refresh_weather() {
    const auto now = std::chrono::steady_clock::now();
    const auto interval = std::chrono::minutes(std::max(5, refresh_minutes_->get()));
    if (have_weather_ && now - last_fetch_ < interval) return true;
    const auto result = parser_.get_data(location_lat_->get(), location_lon_->get());
    if (!result) {
        spdlog::warn("Weather ambience refresh failed: {}", result.error());
        return have_weather_;
    }
    weather_ = result.value();
    have_weather_ = true;
    last_fetch_ = now;
    return true;
}

void WeatherAmbienceScene::initialize_clouds() {
    if (clouds_initialized_) return;
    std::uniform_real_distribution<float> x(-25.0f, static_cast<float>(matrix_width));
    std::uniform_real_distribution<float> y(5.0f, matrix_height * 0.48f);
    std::uniform_real_distribution<float> scale(0.65f, 1.45f);
    std::uniform_real_distribution<float> phase(0.0f, 2.0f * Pi);
    for (auto &cloud : clouds_) cloud = {x(rng_), y(rng_), scale(rng_), phase(rng_)};
    clouds_initialized_ = true;
}

void WeatherAmbienceScene::spawn_particle(WeatherAmbienceMode mode) {
    if (mode != WeatherAmbienceMode::RAIN && mode != WeatherAmbienceMode::SNOW && mode != WeatherAmbienceMode::STORM) return;
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    WeatherParticle particle;
    particle.x = unit(rng_) * matrix_width;
    particle.y = -2.0f - unit(rng_) * matrix_height * 0.18f;
    const float wind = std::clamp(weather_.wind_speed_value / 35.0f, 0.0f, 1.5f) * wind_response_->get();
    particle.phase = unit(rng_) * 2.0f * Pi;
    particle.maxLife = particle.life = 2.8f + unit(rng_) * 4.5f;
    if (mode == WeatherAmbienceMode::SNOW) {
        particle.kind = ParticleKind::Snow;
        particle.vx = (unit(rng_) - 0.5f) * 4.0f + wind * 4.0f;
        particle.vy = 9.0f + unit(rng_) * 13.0f;
        particle.size = 1.0f + unit(rng_) * 1.8f;
        particle.alpha = 0.58f + unit(rng_) * 0.38f;
    } else {
        particle.kind = ParticleKind::Rain;
        particle.vx = wind * 12.0f + (unit(rng_) - 0.5f) * 2.5f;
        particle.vy = 48.0f + unit(rng_) * 52.0f;
        particle.size = 1.0f;
        particle.alpha = 0.42f + unit(rng_) * 0.45f;
    }
    particles_.try_push(std::move(particle));
}

void WeatherAmbienceScene::update_particles(float dt, WeatherAmbienceMode mode) {
    const float quality = render_quality_scale();
    const int limit = std::max(35, static_cast<int>(std::lround(
        particle_limit_->get() * (0.40f + 0.60f * quality))));
    particles_.set_limit(static_cast<size_t>(limit));
    if (mode != WeatherAmbienceMode::RAIN && mode != WeatherAmbienceMode::SNOW && mode != WeatherAmbienceMode::STORM) {
        particles_.clear();
        return;
    }

    const float weather_strength = mode == WeatherAmbienceMode::STORM ? 1.0f
        : std::clamp(0.35f + weather_.precipitation * 0.22f, 0.35f, 1.0f);
    const int target = std::min(limit, static_cast<int>(std::lround(
        limit * weather_strength * (0.30f + intensity_->get() * 0.07f))));
    const float speed = animation_speed_->get();
    while (static_cast<int>(particles_.size()) < target) spawn_particle(mode);

    for (auto &particle : particles_) {
        if (particle.kind == ParticleKind::Snow) {
            const float sway = std::sin(time_ * 1.4f + particle.phase) * 5.5f;
            Particles::integrate(particle, dt * speed, sway, 1.3f);
            particle.rotation += dt * speed * 0.8f;
        } else {
            Particles::integrate(particle, dt * speed, 0.0f, 8.0f);
        }
    }
    particles_.erase_if([&](const WeatherParticle &particle) {
        return particle.life <= 0.0f || particle.y > matrix_height + 8.0f || particle.x > matrix_width + 12.0f;
    });
}

void WeatherAmbienceScene::render_background(rgb_matrix::FrameCanvas *canvas, WeatherAmbienceMode mode) const {
    const bool day = weather_.is_day;
    for (int y = 0; y < matrix_height; ++y) {
        const float t = static_cast<float>(y) / std::max(1, matrix_height - 1);
        float dim = 1.0f;
        if (mode == WeatherAmbienceMode::RAIN) dim = 0.62f;
        else if (mode == WeatherAmbienceMode::STORM) dim = 0.40f;
        else if (mode == WeatherAmbienceMode::FOG) dim = 0.76f;
        else if (mode == WeatherAmbienceMode::CLOUDY) dim = 0.72f;

        const float top_r = day ? 18.0f : 2.0f;
        const float top_g = day ? 72.0f : 5.0f;
        const float top_b = day ? 132.0f : 22.0f;
        const float bottom_r = day ? 82.0f : 8.0f;
        const float bottom_g = day ? 142.0f : 16.0f;
        const float bottom_b = day ? 190.0f : 48.0f;
        const uint8_t r = static_cast<uint8_t>(std::clamp((top_r + (bottom_r - top_r) * t) * dim, 0.0f, 255.0f));
        const uint8_t g = static_cast<uint8_t>(std::clamp((top_g + (bottom_g - top_g) * t) * dim, 0.0f, 255.0f));
        const uint8_t b = static_cast<uint8_t>(std::clamp((top_b + (bottom_b - top_b) * t) * dim, 0.0f, 255.0f));
        for (int x = 0; x < matrix_width; ++x) canvas->SetPixel(x, y, r, g, b);
    }

    if (mode == WeatherAmbienceMode::CLEAR) {
        const float cx = weather_.is_day ? matrix_width * 0.76f : matrix_width * 0.70f;
        const float cy = matrix_height * 0.24f;
        const float radius = weather_.is_day ? 10.0f : 7.0f;
        for (int y = std::max(0, static_cast<int>(cy - radius - 3)); y <= std::min(matrix_height - 1, static_cast<int>(cy + radius + 3)); ++y) {
            for (int x = std::max(0, static_cast<int>(cx - radius - 3)); x <= std::min(matrix_width - 1, static_cast<int>(cx + radius + 3)); ++x) {
                const float d = std::hypot(x - cx, y - cy);
                if (d > radius + 3.0f) continue;
                const float alpha = d <= radius ? 0.90f : (radius + 3.0f - d) / 3.0f * 0.28f;
                if (weather_.is_day) alpha_pixel(canvas, x, y, 255, 220, 105, alpha);
                else alpha_pixel(canvas, x, y, 210, 225, 255, alpha);
            }
        }
    }
}

void WeatherAmbienceScene::render_clouds(rgb_matrix::FrameCanvas *canvas, WeatherAmbienceMode mode, float dt) {
    if (mode == WeatherAmbienceMode::CLEAR && weather_.cloud_cover < 0.18f) return;
    initialize_clouds();
    const float cloudiness = mode == WeatherAmbienceMode::STORM ? 1.0f
        : mode == WeatherAmbienceMode::RAIN ? 0.86f
        : std::clamp(std::max(weather_.cloud_cover, 0.35f), 0.0f, 1.0f);
    const int count = std::clamp(2 + static_cast<int>(std::round(cloudiness * 3.0f)), 2, static_cast<int>(clouds_.size()));
    const float wind = (1.5f + weather_.wind_speed_value * 0.10f * wind_response_->get()) * animation_speed_->get();
    for (int i = 0; i < count; ++i) {
        auto &cloud = clouds_[static_cast<size_t>(i)];
        cloud.x += wind * dt * (0.45f + cloud.scale * 0.25f);
        if (cloud.x > matrix_width + 28.0f * cloud.scale) cloud.x = -30.0f * cloud.scale;
        const int rx = std::max(5, static_cast<int>(13.0f * cloud.scale));
        const int ry = std::max(3, static_cast<int>(5.5f * cloud.scale));
        const int center_y = static_cast<int>(cloud.y + std::sin(time_ * 0.35f + cloud.phase) * 1.5f);
        const int center_x = static_cast<int>(cloud.x);
        const uint8_t base = mode == WeatherAmbienceMode::STORM ? 86 : static_cast<uint8_t>(145 + 55 * (1.0f - cloudiness));
        for (int y = center_y - ry; y <= center_y + ry; ++y) {
            for (int x = center_x - rx; x <= center_x + rx; ++x) {
                const float nx = static_cast<float>(x - center_x) / rx;
                const float ny = static_cast<float>(y - center_y) / ry;
                const float d = nx * nx + ny * ny;
                if (d >= 1.0f) continue;
                alpha_pixel(canvas, x, y, base, base + 5, std::min(255, base + 18),
                            (1.0f - d) * (0.24f + cloudiness * 0.34f));
            }
        }
    }
}

void WeatherAmbienceScene::render_particles(rgb_matrix::FrameCanvas *canvas) const {
    for (const auto &particle : particles_) {
        const int x = static_cast<int>(std::lround(particle.x));
        const int y = static_cast<int>(std::lround(particle.y));
        const float life = Particles::life_ratio(particle);
        if (particle.kind == ParticleKind::Snow) {
            alpha_pixel(canvas, x, y, 238, 247, 255, particle.alpha * life);
            if (particle.size > 1.8f) {
                alpha_pixel(canvas, x + 1, y, 225, 240, 255, particle.alpha * life * 0.45f);
                alpha_pixel(canvas, x, y + 1, 225, 240, 255, particle.alpha * life * 0.45f);
            }
        } else {
            for (int tail = 0; tail < 4; ++tail)
                alpha_pixel(canvas, x, y - tail, 115, 190, 255, particle.alpha * life * (1.0f - tail * 0.20f));
        }
    }
}

void WeatherAmbienceScene::render_fog(rgb_matrix::FrameCanvas *canvas, float) const {
    const int bands = 5 + intensity_->get() / 2;
    for (int band = 0; band < bands; ++band) {
        const float center = (band + 0.5f) * matrix_height / bands + std::sin(time_ * 0.22f + band * 1.7f) * 7.0f;
        const float half = 8.0f + band * 1.5f;
        for (int y = std::max(0, static_cast<int>(center - half)); y <= std::min(matrix_height - 1, static_cast<int>(center + half)); ++y) {
            const float fade = 1.0f - std::abs(y - center) / half;
            const int offset = static_cast<int>(std::sin(time_ * 0.30f + band * 2.1f) * 12.0f);
            for (int x = 0; x < matrix_width; x += 2) {
                const float wave = 0.72f + 0.28f * std::sin((x + offset) * 0.055f + band);
                alpha_pixel(canvas, x, y, 205, 220, 225, fade * wave * 0.055f * intensity_->get());
                alpha_pixel(canvas, x + 1, y, 205, 220, 225, fade * wave * 0.055f * intensity_->get());
            }
        }
    }
}

void WeatherAmbienceScene::render_lightning(rgb_matrix::FrameCanvas *canvas, WeatherAmbienceMode mode, float dt) {
    lightning_ = std::max(0.0f, lightning_ - dt * 6.5f);
    lightning_cooldown_ = std::max(0.0f, lightning_cooldown_ - dt);
    if (mode == WeatherAmbienceMode::STORM && lightning_enabled_->get() && lightning_cooldown_ <= 0.0f) {
        const float chance_per_second = 0.07f + intensity_->get() * 0.018f;
        std::uniform_real_distribution<float> unit(0.0f, 1.0f);
        if (unit(rng_) < chance_per_second * dt) {
            lightning_ = 1.0f;
            lightning_cooldown_ = 1.5f + unit(rng_) * 4.0f;
        }
    }
    if (lightning_ <= 0.0f) return;
    for (int y = 0; y < matrix_height; ++y)
        for (int x = 0; x < matrix_width; ++x)
            alpha_pixel(canvas, x, y, 215, 225, 255, lightning_ * 0.58f);
}

bool WeatherAmbienceScene::render(rgb_matrix::FrameCanvas *canvas) {
    if (!refresh_weather()) return false;
    const float dt = std::clamp(static_cast<float>(frame_context().delta_seconds), 0.0f, 0.08f);
    time_ += dt * animation_speed_->get();
    const auto mode = effective_mode();

    update_particles(dt, mode);
    render_background(canvas, mode);
    render_clouds(canvas, mode, dt);
    if (mode == WeatherAmbienceMode::FOG) render_fog(canvas, dt);
    render_particles(canvas);
    render_lightning(canvas, mode, dt);
    wait_until_next_frame();
    return true;
}

} // namespace Scenes
