#pragma once

#include "Scene.h"
#include "../WeatherParser.h"
#include "led-matrix.h"
#include <memory>
#include <wrappers.h>
#include <chrono>
#include "WeatherEffects.h"
#include "StarField.h"

using rgb_matrix::Canvas;
using rgb_matrix::Font;
using Scenes::Scene;

class WeatherVisualizerScene final : public Scene {
private:
    std::unique_ptr<WeatherParser> weather_parser;
    WeatherData current_weather;
    uint64_t last_update = 0;
    bool has_error = false;
    std::string error_message;

    // Animation state variables
    int frame_counter = 0;
    std::vector<WeatherParticle> particles; // For rain/snow particles
    StarField star_field;
    int lightning_timer = 0; // Timer for lightning flashes
    bool lightning_active = false;
    int wind_offset = 0; // For cloud movements
    int sun_ray_length = 3; // For sun ray animation
    bool ray_growing = true;

    // Frame limiting variables
    uint64_t last_frame_time = 0;
    int target_fps = 30;     // Default target FPS
    int frame_interval_ms = 1000 / 30; // Default frame interval in ms (33ms ≈ 30fps)

    // Weather-specific animation speeds
    float animation_speed_factor = 1.0f; // Default speed factor
    
    // Time-of-day awareness
    bool is_night_time = false;
    bool time_override_enabled = false;
    
    // Lighting and mood adjustments
    float brightness_factor = 1.0f; // Used to dim visuals at night
    RGB night_tint = {20, 20, 50}; // Bluish tint for night time

    // Weather visualization helpers
    WeatherEffects weather_effects;
    
    // Helper methods
    void initialize_particles(int count, bool is_snow);
    void update_particles(bool is_snow);
    void render_based_on_weather_code(Canvas *canvas);
    RGB get_color_for_weather_code();
    
    // Frame rate and time awareness control
    void adjust_frame_rate_for_weather();
    void limit_frame_rate();
    void update_time_of_day();
    bool is_night_time_now() const;
    RGB apply_night_tint(const RGB &color) const;

public:
    WeatherVisualizerScene();
    ~WeatherVisualizerScene() override = default;
    
    // Required Scene overrides
    [[nodiscard]] std::string get_name() const override { return "weather_visualizer"; }
    void register_properties() override;
    bool render(rgb_matrix::RGBMatrixBase *matrix) override;


    void initialize(RGBMatrixBase *matrix, FrameCanvas *l_offscreen_canvas) override;
    void after_render_stop(RGBMatrixBase *matrix) override;
};

class WeatherVisualizerWrapper final : public Plugins::SceneWrapper {
public:
    std::unique_ptr<Scenes::Scene, void(*)(Scenes::Scene *)> create() override {
        return {
            new WeatherVisualizerScene(), [](Scenes::Scene *scene) {
                delete scene;
            }
        };
    }
};
