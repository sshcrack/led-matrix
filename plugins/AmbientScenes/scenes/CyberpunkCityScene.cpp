#include "CyberpunkCityScene.h"
#include <cmath>
#include <algorithm>
#include <chrono>
#include <random>

namespace AmbientScenes {

CyberpunkCityScene::CyberpunkCityScene()
    : Scene(), lightning_timer(0), lightning_duration(0), lightning_active(false) {
    // Initialize random number generator with a time-based seed
    std::random_device rd;
    rng = std::mt19937(rd());
}

void CyberpunkCityScene::initialize(rgb_matrix::RGBMatrixBase *matrix, rgb_matrix::FrameCanvas *l_offscreen_canvas) {
    Scene::initialize(matrix, l_offscreen_canvas);
    
    // Initialize scene elements
    initialize_buildings();
    initialize_vehicles();
    initialize_raindrops();
}

void CyberpunkCityScene::initialize_buildings() {
    buildings.clear();
    
    // Create a skyline of buildings with varying heights and widths
    std::uniform_int_distribution<int> width_dist(3, 8);
    std::uniform_int_distribution<int> height_dist(5, 24);
    std::uniform_int_distribution<int> billboard_dist(0, 100);
    
    int x = 0;
    while (x < offscreen_canvas->width()) {
        Building building;
        building.x = x;
        building.width = width_dist(rng);
        building.height = height_dist(rng);
        
        // Determine building outline color based on color scheme
        get_color_scheme(building.outline_r, building.outline_g, building.outline_b, 
                        buildings.size() % 3); // Vary colors slightly
        
        // Determine if building has a billboard
        int billboard_chance = billboard_frequency->get() * 30 + 10; // 10%, 40%, 70%
        building.has_billboard = billboard_dist(rng) < billboard_chance;
        
        if (building.has_billboard) {
            // Place billboard somewhere on the upper half of the building
            std::uniform_int_distribution<int> billboard_y_dist(
                offscreen_canvas->height() - building.height, 
                offscreen_canvas->height() - building.height / 2
            );
            building.billboard_y = billboard_y_dist(rng);
            building.billboard_state = 0;
            building.billboard_timer = 0;
        }
        
        buildings.push_back(building);
        x += building.width + 1; // Gap between buildings
    }
}

void CyberpunkCityScene::initialize_vehicles() {
    vehicles.clear();
    
    // Create initial vehicles
    int num_vehicles = 2 + traffic_density->get() * 2; // 2, 4, or 6 vehicles
    
    std::uniform_real_distribution<float> y_dist(2.0f, offscreen_canvas->height() * 0.4f);
    std::uniform_real_distribution<float> speed_dist(0.1f, 0.5f);
    std::uniform_int_distribution<int> size_dist(1, 3);
    std::uniform_int_distribution<int> dir_dist(0, 1);
    
    for (int i = 0; i < num_vehicles; i++) {
        Vehicle vehicle;
        vehicle.y = y_dist(rng);
        vehicle.speed = speed_dist(rng);
        vehicle.size = size_dist(rng);
        vehicle.direction_right = dir_dist(rng) == 1;
        
        if (vehicle.direction_right) {
            vehicle.x = -vehicle.size;
        } else {
            vehicle.x = offscreen_canvas->width() + vehicle.size;
        }
        
        // Set vehicle color based on color scheme
        get_color_scheme(vehicle.r, vehicle.g, vehicle.b, i % 3 + 3); // Different variation
        
        vehicles.push_back(vehicle);
    }
}

void CyberpunkCityScene::initialize_raindrops() {
    raindrops.clear();
    
    if (rain_intensity->get() == 0) {
        return; // No rain
    }
    
    // Create initial raindrops
    int num_raindrops = rain_intensity->get() * 15; // 15, 30, or 45 raindrops
    
    std::uniform_real_distribution<float> x_dist(0, offscreen_canvas->width() - 1);
    std::uniform_real_distribution<float> y_dist(0, offscreen_canvas->height() - 1);
    std::uniform_real_distribution<float> speed_dist(0.2f, 0.8f);
    std::uniform_int_distribution<int> brightness_dist(50, 150);
    std::uniform_int_distribution<int> length_dist(1, 3);
    
    for (int i = 0; i < num_raindrops; i++) {
        Raindrop drop;
        drop.x = x_dist(rng);
        drop.y = y_dist(rng);
        drop.speed = speed_dist(rng);
        drop.brightness = brightness_dist(rng);
        drop.length = length_dist(rng);
        
        raindrops.push_back(drop);
    }
}

bool CyberpunkCityScene::render(rgb_matrix::RGBMatrixBase *matrix) {
    // Set background color based on time of day
    uint8_t bg_r, bg_g, bg_b;
    switch (time_of_day->get()) {
        case 0: // Day
            bg_r = 50; bg_g = 70; bg_b = 100;
            break;
        case 1: // Twilight
            bg_r = 30; bg_g = 30; bg_b = 50;
            break;
        case 2: // Night
        default:
            bg_r = 5; bg_g = 5; bg_b = 15;
            break;
    }
    
    // Apply lightning effect to background if active
    if (lightning_active) {
        float intensity = static_cast<float>(lightning_duration - lightning_timer) / lightning_duration;
        bg_r = std::min(255, bg_r + static_cast<int>(200 * intensity));
        bg_g = std::min(255, bg_g + static_cast<int>(200 * intensity));
        bg_b = std::min(255, bg_b + static_cast<int>(200 * intensity));
    }
    
    offscreen_canvas->Fill(bg_r, bg_g, bg_b);
    
    // Update and draw scene elements
    update_buildings();
    update_vehicles();
    update_raindrops();
    update_lightning();
    
    draw_buildings(matrix);
    draw_vehicles(matrix);
    draw_raindrops(matrix);
    
    // Swap and display
    offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas, 1);
    return true;
}

void CyberpunkCityScene::update_buildings() {
    // Update billboard animations
    for (auto& building : buildings) {
        if (building.has_billboard) {
            building.billboard_timer++;
            
            // Change billboard state periodically
            if (building.billboard_timer > 20) {
                building.billboard_state = (building.billboard_state + 1) % 4;
                building.billboard_timer = 0;
            }
        }
    }
}

void CyberpunkCityScene::update_vehicles() {
    for (auto& vehicle : vehicles) {
        // Move vehicles
        if (vehicle.direction_right) {
            vehicle.x += vehicle.speed;
            if (vehicle.x > offscreen_canvas->width() + vehicle.size) {
                // Reset to left side when it goes off-screen
                vehicle.x = -vehicle.size;
                
                // Randomize y position and speed for variety
                std::uniform_real_distribution<float> y_dist(2.0f, offscreen_canvas->height() * 0.4f);
                std::uniform_real_distribution<float> speed_dist(0.1f, 0.5f);
                vehicle.y = y_dist(rng);
                vehicle.speed = speed_dist(rng);
            }
        } else {
            vehicle.x -= vehicle.speed;
            if (vehicle.x < -vehicle.size) {
                // Reset to right side when it goes off-screen
                vehicle.x = offscreen_canvas->width() + vehicle.size;
                
                // Randomize y position and speed for variety
                std::uniform_real_distribution<float> y_dist(2.0f, offscreen_canvas->height() * 0.4f);
                std::uniform_real_distribution<float> speed_dist(0.1f, 0.5f);
                vehicle.y = y_dist(rng);
                vehicle.speed = speed_dist(rng);
            }
        }
    }
}

void CyberpunkCityScene::update_raindrops() {
    if (rain_intensity->get() == 0) {
        raindrops.clear();
        return;
    }
    
    for (auto& drop : raindrops) {
        // Move raindrops down and slightly to the right (diagonal rain)
        drop.y += drop.speed;
        drop.x += drop.speed * 0.3f;
        
        // Reset raindrops that go off-screen
        if (drop.y >= offscreen_canvas->height() || drop.x >= offscreen_canvas->width()) {
            std::uniform_real_distribution<float> x_dist(0, offscreen_canvas->width() * 0.8f);
            drop.x = x_dist(rng);
            drop.y = 0;
            
            // Randomize speed and brightness for variety
            std::uniform_real_distribution<float> speed_dist(0.2f, 0.8f);
            std::uniform_int_distribution<int> brightness_dist(50, 150);
            drop.speed = speed_dist(rng);
            drop.brightness = brightness_dist(rng);
        }
    }
    
    // Add new raindrops if needed
    int target_raindrops = rain_intensity->get() * 15;
    if (raindrops.size() < target_raindrops) {
        std::uniform_real_distribution<float> x_dist(0, offscreen_canvas->width() - 1);
        std::uniform_real_distribution<float> speed_dist(0.2f, 0.8f);
        std::uniform_int_distribution<int> brightness_dist(50, 150);
        std::uniform_int_distribution<int> length_dist(1, 3);
        
        Raindrop drop;
        drop.x = x_dist(rng);
        drop.y = 0;
        drop.speed = speed_dist(rng);
        drop.brightness = brightness_dist(rng);
        drop.length = length_dist(rng);
        
        raindrops.push_back(drop);
    }
}

void CyberpunkCityScene::update_lightning() {
    if (!enable_lightning->get()) {
        lightning_active = false;
        return;
    }
    
    if (lightning_active) {
        // Update active lightning
        lightning_timer++;
        if (lightning_timer >= lightning_duration) {
            lightning_active = false;
        }
    } else {
        // Random chance to start lightning
        std::uniform_int_distribution<int> lightning_chance(0, 500);
        if (lightning_chance(rng) == 0) {
            lightning_active = true;
            lightning_timer = 0;
            
            // Random duration between 3-10 frames
            std::uniform_int_distribution<int> duration_dist(3, 10);
            lightning_duration = duration_dist(rng);
        }
    }
}

void CyberpunkCityScene::draw_buildings(rgb_matrix::RGBMatrixBase *matrix) {
    int ground_y = offscreen_canvas->height() - 1;
    
    for (const auto& building : buildings) {
        int building_top = ground_y - building.height;
        
        // Draw building outline
        for (int y = building_top; y <= ground_y; y++) {
            // Left and right edges
            offscreen_canvas->SetPixel(building.x, y, 
                                     building.outline_r, building.outline_g, building.outline_b);
            offscreen_canvas->SetPixel(building.x + building.width - 1, y, 
                                     building.outline_r, building.outline_g, building.outline_b);
        }
        
        // Top edge
        for (int x = building.x; x < building.x + building.width; x++) {
            offscreen_canvas->SetPixel(x, building_top, 
                                     building.outline_r, building.outline_g, building.outline_b);
        }
        
        // Draw windows (random pattern)
        std::uniform_int_distribution<int> window_dist(0, 100);
        for (int x = building.x + 1; x < building.x + building.width - 1; x++) {
            for (int y = building_top + 1; y < ground_y; y++) {
                // Higher chance of windows in night mode
                int window_chance = time_of_day->get() == 2 ? 30 : 15;
                
                if (window_dist(rng) < window_chance) {
                    // Window color based on time of day
                    if (time_of_day->get() == 0) { // Day
                        offscreen_canvas->SetPixel(x, y, 200, 200, 150); // Yellowish
                    } else if (time_of_day->get() == 1) { // Twilight
                        offscreen_canvas->SetPixel(x, y, 255, 200, 100); // Orange
                    } else { // Night
                        offscreen_canvas->SetPixel(x, y, 255, 255, 150); // Bright yellow
                    }
                }
            }
        }
        
        // Draw billboard if present
        if (building.has_billboard && building.width >= 5) {
            int billboard_width = std::min(building.width - 2, 6);
            int billboard_height = 2;
            int billboard_x = building.x + (building.width - billboard_width) / 2;
            
            // Billboard background
            for (int x = billboard_x; x < billboard_x + billboard_width; x++) {
                for (int y = building.billboard_y; y < building.billboard_y + billboard_height; y++) {
                    // Base color
                    uint8_t r = 0, g = 0, b = 0;
                    get_color_scheme(r, g, b, building.billboard_state + 6);
                    
                    // Animate billboard by varying brightness
                    float brightness = 0.5f + 0.5f * sin(building.billboard_timer * 0.3f);
                    r = static_cast<uint8_t>(r * brightness);
                    g = static_cast<uint8_t>(g * brightness);
                    b = static_cast<uint8_t>(b * brightness);
                    
                    offscreen_canvas->SetPixel(x, y, r, g, b);
                }
            }
        }
    }
}

void CyberpunkCityScene::draw_vehicles(rgb_matrix::RGBMatrixBase *matrix) {
    for (const auto& vehicle : vehicles) {
        int x = static_cast<int>(vehicle.x);
        int y = static_cast<int>(vehicle.y);
        
        // Draw vehicle body
        for (int i = 0; i < vehicle.size; i++) {
            offscreen_canvas->SetPixel(x + i, y, vehicle.r, vehicle.g, vehicle.b);
        }
        
        // Draw lights (front and back)
        if (vehicle.direction_right) {
            // Front light (white/yellow)
            if (x + vehicle.size < offscreen_canvas->width()) {
                offscreen_canvas->SetPixel(x + vehicle.size, y, 255, 255, 200);
            }
            // Back light (red)
            if (x >= 0) {
                offscreen_canvas->SetPixel(x, y, 255, 50, 50);
            }
        } else {
            // Front light (white/yellow)
            if (x >= 0) {
                offscreen_canvas->SetPixel(x, y, 255, 255, 200);
            }
            // Back light (red)
            if (x + vehicle.size - 1 < offscreen_canvas->width()) {
                offscreen_canvas->SetPixel(x + vehicle.size - 1, y, 255, 50, 50);
            }
        }
    }
}

void CyberpunkCityScene::draw_raindrops(rgb_matrix::RGBMatrixBase *matrix) {
    for (const auto& drop : raindrops) {
        int x = static_cast<int>(drop.x);
        int y = static_cast<int>(drop.y);
        
        // Skip if off-screen
        if (x < 0 || x >= offscreen_canvas->width() || y < 0 || y >= offscreen_canvas->height()) {
            continue;
        }
        
        // Draw raindrop with trail
        for (int i = 0; i < drop.length; i++) {
            int trail_y = y - i;
            if (trail_y >= 0) {
                // Fade brightness for trail
                int brightness = drop.brightness * (1.0f - static_cast<float>(i) / drop.length);
                offscreen_canvas->SetPixel(x, trail_y, brightness, brightness, brightness);
            }
        }
    }
}

void CyberpunkCityScene::get_color_scheme(uint8_t& r, uint8_t& g, uint8_t& b, int variation) {
    // Base colors for different schemes
    switch (color_scheme->get()) {
        case 0: // Classic cyberpunk (pink/blue/purple)
            switch (variation % 6) {
                case 0: r = 255; g = 50; b = 150; break; // Pink
                case 1: r = 0; g = 200; b = 255; break;  // Cyan
                case 2: r = 150; g = 50; b = 255; break; // Purple
                case 3: r = 255; g = 100; b = 200; break; // Light pink
                case 4: r = 50; g = 150; b = 255; break;  // Blue
                case 5: r = 200; g = 50; b = 200; break;  // Magenta
            }
            break;
            
        case 1: // Blue dominant
            switch (variation % 3) {
                case 0: r = 0; g = 100; b = 255; break;  // Blue
                case 1: r = 0; g = 200; b = 255; break;  // Cyan
                case 2: r = 50; g = 50; b = 200; break;  // Dark blue
            }
            break;
            
        case 2: // Red dominant
            switch (variation % 3) {
                case 0: r = 255; g = 50; b = 50; break;  // Red
                case 1: r = 255; g = 100; b = 0; break;  // Orange
                case 2: r = 200; g = 0; b = 50; break;   // Dark red
            }
            break;
            
        case 3: // Green dominant
            switch (variation % 3) {
                case 0: r = 0; g = 255; b = 100; break;  // Green
                case 1: r = 100; g = 255; b = 0; break;  // Lime
                case 2: r = 0; g = 200; b = 100; break;  // Dark green
            }
            break;
    }
}

std::string CyberpunkCityScene::get_name() const {
    return "cyberpunk_city";
}

void CyberpunkCityScene::register_properties() {
    add_property(time_of_day);
    add_property(rain_intensity);
    add_property(traffic_density);
    add_property(enable_lightning);
    add_property(color_scheme);
    add_property(billboard_frequency);
}

std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> CyberpunkCitySceneWrapper::create() {
    return {new CyberpunkCityScene(), [](Scenes::Scene *scene) {
        delete (CyberpunkCityScene *) scene;
    }};
}

} 