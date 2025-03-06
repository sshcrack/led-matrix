#pragma once

#include "led-matrix.h"
#include <vector>

using rgb_matrix::Canvas;

// Structure to represent a star in the night sky
struct Star {
    int x;
    int y;           // Position
    float brightness; // Current brightness
    float max_brightness; // Maximum brightness (varies by star)
    float twinkle_speed; // How quickly the star twinkles
    float phase;        // Current phase in the twinkle cycle
    uint8_t color_type; // 0=white, 1=blue, 2=yellow, 3=red
    bool is_bright;     // If true, may have a glow effect
};

class StarField {
private:
    std::vector<Star> stars;
    bool initialized = false;
    int matrix_width = 0;
    int matrix_height = 0;
    float animation_speed_factor = 1.0f;
    
public:
    StarField() = default;
    
    // Initialize the star field
    void initialize(int width, int height, float speed_factor = 1.0f);
    
    // Clear the star field
    void clear();
    
    // Update star animation based on delta time
    void update(float delta_time);
    
    // Render stars to canvas
    void render(Canvas *canvas);
    
    // Check if stars are initialized
    bool is_initialized() const { return initialized; }
    
    // Update animation speed factor
    void set_speed_factor(float speed_factor) { animation_speed_factor = speed_factor; }
};
