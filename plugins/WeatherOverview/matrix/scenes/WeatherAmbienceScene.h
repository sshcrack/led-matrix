#pragma once

#include "../WeatherParser.h"
#include "shared/matrix/Scene.h"
#include "shared/matrix/particles.h"
#include "shared/matrix/wrappers.h"

#include <array>
#include <chrono>
#include <random>

namespace Scenes {

enum class WeatherAmbienceMode {
    AUTO = 0,
    CLEAR,
    CLOUDY,
    RAIN,
    SNOW,
    FOG,
    STORM,
};

class WeatherAmbienceScene final : public Scene {
    enum class ParticleKind { Rain, Snow };
    struct WeatherParticle : Particles::KinematicParticle {
        ParticleKind kind = ParticleKind::Rain;
        float alpha = 1.0f;
        float phase = 0.0f;
    };

    struct Cloud {
        float x = 0.0f;
        float y = 0.0f;
        float scale = 1.0f;
        float phase = 0.0f;
    };

    WeatherParser parser_;
    WeatherData weather_{};
    bool have_weather_ = false;
    std::chrono::steady_clock::time_point last_fetch_{};
    std::mt19937 rng_{std::random_device{}()};
    Particles::ParticlePool<WeatherParticle> particles_{420};
    std::array<Cloud, 5> clouds_{};
    bool clouds_initialized_ = false;
    float time_ = 0.0f;
    float lightning_ = 0.0f;
    float lightning_cooldown_ = 0.0f;

    PropertyPointer<std::string> location_lat_ = MAKE_PROPERTY("location_lat", std::string, "52.5200");
    PropertyPointer<std::string> location_lon_ = MAKE_PROPERTY("location_lon", std::string, "13.4050");
    PropertyPointer<Plugins::EnumProperty<WeatherAmbienceMode>> mode_ =
        MAKE_ENUM_PROPERTY("mode", WeatherAmbienceMode, WeatherAmbienceMode::AUTO);
    PropertyPointer<int> intensity_ = MAKE_PROPERTY_MINMAX("intensity", int, 6, 1, 10);
    PropertyPointer<float> animation_speed_ = MAKE_PROPERTY_MINMAX("animation_speed", float, 1.0f, 0.2f, 3.0f);
    PropertyPointer<int> particle_limit_ = MAKE_PROPERTY_MINMAX("particle_limit", int, 260, 40, 600);
    PropertyPointer<float> wind_response_ = MAKE_PROPERTY_MINMAX("wind_response", float, 1.0f, 0.0f, 2.0f);
    PropertyPointer<int> refresh_minutes_ = MAKE_PROPERTY_MINMAX("refresh_minutes", int, 15, 15, 60);
    PropertyPointer<bool> lightning_enabled_ = MAKE_PROPERTY("lightning", bool, true);

    [[nodiscard]] WeatherAmbienceMode effective_mode() const;
    bool refresh_weather();
    void initialize_clouds();
    void update_particles(float dt, WeatherAmbienceMode mode);
    void spawn_particle(WeatherAmbienceMode mode);
    void render_background(rgb_matrix::FrameCanvas *canvas, WeatherAmbienceMode mode) const;
    void render_clouds(rgb_matrix::FrameCanvas *canvas, WeatherAmbienceMode mode, float dt);
    void render_particles(rgb_matrix::FrameCanvas *canvas) const;
    void render_fog(rgb_matrix::FrameCanvas *canvas, float dt) const;
    void render_lightning(rgb_matrix::FrameCanvas *canvas, WeatherAmbienceMode mode, float dt);

public:
    WeatherAmbienceScene();
    bool render(rgb_matrix::FrameCanvas *canvas) override;
    void register_properties() override;
    std::string get_name() const override { return "weather_ambience"; }
    std::string get_category() const override { return "Weather"; }
    tmillis_t get_default_duration() override { return 45000; }
    int get_default_weight() override { return 5; }
    SceneCapabilities get_capabilities() const override {
        auto caps = Scene::get_capabilities();
        caps.requires_network = true;
        return caps;
    }
};

class WeatherAmbienceSceneWrapper final : public Plugins::SceneWrapper {
public:
    std::unique_ptr<Scenes::Scene> create() override;
};

} // namespace Scenes
