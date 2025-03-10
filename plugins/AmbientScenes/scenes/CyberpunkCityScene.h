#pragma once

#include "Scene.h"
#include "plugin/main.h"
#include <vector>
#include <random>

namespace AmbientScenes {

// Structure to represent a building in the skyline
struct Building {
    int x;
    int width;
    int height;
    uint8_t outline_r;
    uint8_t outline_g;
    uint8_t outline_b;
    bool has_billboard;
    int billboard_y;
    int billboard_state;
    int billboard_timer;
};

// Structure to represent a flying vehicle
struct Vehicle {
    float x;
    float y;
    float speed;
    int size;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    bool direction_right;
};

// Structure to represent a raindrop
struct Raindrop {
    float x;
    float y;
    float speed;
    int brightness;
    int length;
};

class CyberpunkCityScene : public Scenes::Scene {
private:
    // Scene properties
    PropertyPointer<uint8_t> time_of_day = MAKE_PROPERTY("time_of_day", uint8_t, 1); // 0=day, 1=twilight, 2=night
    PropertyPointer<uint8_t> rain_intensity = MAKE_PROPERTY("rain_intensity", uint8_t, 2); // 0=none, 1=light, 2=medium, 3=heavy
    PropertyPointer<uint8_t> traffic_density = MAKE_PROPERTY("traffic_density", uint8_t, 1); // 0=low, 1=medium, 2=high
    PropertyPointer<bool> enable_lightning = MAKE_PROPERTY("enable_lightning", bool, true);
    PropertyPointer<uint8_t> color_scheme = MAKE_PROPERTY("color_scheme", uint8_t, 0); // 0=classic, 1=blue, 2=red, 3=green
    PropertyPointer<uint8_t> billboard_frequency = MAKE_PROPERTY("billboard_frequency", uint8_t, 1); // 0=low, 1=medium, 2=high
    
    // Scene elements
    std::vector<Building> buildings;
    std::vector<Vehicle> vehicles;
    std::vector<Raindrop> raindrops;
    
    // Lightning effect
    int lightning_timer;
    int lightning_duration;
    bool lightning_active;
    
    // Random number generator
    std::mt19937 rng;
    
    // Helper methods
    void initialize_buildings();
    void initialize_vehicles();
    void initialize_raindrops();
    void update_buildings();
    void update_vehicles();
    void update_raindrops();
    void update_lightning();
    void draw_buildings(rgb_matrix::RGBMatrixBase *matrix);
    void draw_vehicles(rgb_matrix::RGBMatrixBase *matrix);
    void draw_raindrops(rgb_matrix::RGBMatrixBase *matrix);
    void draw_lightning(rgb_matrix::RGBMatrixBase *matrix);
    void get_color_scheme(uint8_t& r, uint8_t& g, uint8_t& b, int variation = 0);
    
public:
    explicit CyberpunkCityScene();
    ~CyberpunkCityScene() override = default;
    
    bool render(rgb_matrix::RGBMatrixBase *matrix) override;
    void initialize(rgb_matrix::RGBMatrixBase *matrix, rgb_matrix::FrameCanvas *l_offscreen_canvas) override;
    [[nodiscard]] std::string get_name() const override;
    void register_properties() override;
    
    using Scene::Scene;
};

class CyberpunkCitySceneWrapper : public Plugins::SceneWrapper {
    std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create();
};

} 