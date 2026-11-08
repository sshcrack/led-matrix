#pragma once

// A calm window onto the sky outside: the real time of day (sun, moon and
// stars), clouds, rain, snow, fog and storms at the matrix's location, from
// the shared environment system (Environment::Service). Every aspect can be
// pinned with the scene's Environment properties.

#include "shared/matrix/Scene.h"
#include "shared/matrix/environment/look.h"
#include "shared/matrix/environment/paint.h"
#include "shared/matrix/wrappers.h"

#include <vector>

namespace Scenes {

class WeatherAmbienceScene final : public Scene {
    Environment::SceneEnvironment environment_{
        {Environment::TimeOfDay::Day, Environment::Weather::PartlyCloudy, Environment::Season::Summer}};
    Environment::Precipitation precip_;
    Environment::Lightning lightning_;
    std::vector<float> px_;
    float time_ = 0.0f;

    PropertyPointer<int> intensity_ = MAKE_PROPERTY_MINMAX("intensity", int, 6, 1, 10);
    PropertyPointer<float> animation_speed_ = MAKE_PROPERTY_MINMAX("animation_speed", float, 1.0f, 0.2f, 3.0f);
    PropertyPointer<int> particle_limit_ = MAKE_PROPERTY_MINMAX("particle_limit", int, 260, 40, 600);
    PropertyPointer<bool> lightning_enabled_ = MAKE_PROPERTY("lightning", bool, true);

public:
    WeatherAmbienceScene();
    void initialize(int width, int height) override;
    bool render(rgb_matrix::FrameCanvas *canvas) override;
    void register_properties() override;
    void set_random_seed(uint32_t seed) override;
    [[nodiscard]] bool supports_virtual_time() const override { return true; }
    [[nodiscard]] SceneDescriptor get_descriptor() const override;
    std::string get_name() const override { return "weather_ambience"; }
    std::string get_category() const override { return "Weather"; }
    tmillis_t get_default_duration() override { return 45000; }
    int get_default_weight() override { return 5; }
};

class WeatherAmbienceSceneWrapper final : public Plugins::SceneWrapper {
public:
    std::unique_ptr<Scenes::Scene> create() override;
};

} // namespace Scenes
