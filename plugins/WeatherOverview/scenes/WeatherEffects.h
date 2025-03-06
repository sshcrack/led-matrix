#pragma once

#include "led-matrix.h"
#include <vector>

using rgb_matrix::Canvas;

// Simple structure to represent a weather particle (rain/snow)
struct WeatherParticle {
    int x;
    int y;
    int speed;
    int drift; // For horizontal movement (snow)
};

class WeatherEffects {
private:
    int matrix_width = 0;
    int matrix_height = 0;
    int wind_offset = 0;
    int sun_ray_length = 3;
    bool ray_growing = true;
    
public:
    WeatherEffects() = default;
    
    void initialize(int width, int height);
    
    // Weather visualization methods
    void draw_rain(Canvas *canvas, const std::vector<WeatherParticle>& particles);
    void draw_snow(Canvas *canvas, const std::vector<WeatherParticle>& particles);
    void draw_clouds(Canvas *canvas, int offset, bool dark = false, bool is_night = false);
    void draw_sun(Canvas *canvas, int frame_counter, float speed_factor);
    void draw_moon(Canvas *canvas, bool with_glow = true);
    void draw_lightning(Canvas *canvas, bool active, int wind_offset, bool is_night, 
                        const std::vector<WeatherParticle>& particles);
    void draw_fog(Canvas *canvas, int frame_counter, bool is_night = false);
    void draw_clear_sky(Canvas *canvas, bool night = false);
    
    // Animation control
    void update_wind(int frame_counter, float speed_factor);
    
    // Getters for animation state
    int get_wind_offset() const { return wind_offset; }
};
