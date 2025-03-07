#pragma once

#include "led-matrix.h"
#include "../elements/WeatherElements.h"
#include "../WeatherParser.h"
#include <vector>
#include <random>

namespace WeatherRenderer {
    // Sky color calculation
    RGB get_sky_color(bool is_day, int weather_code);
    
    // Initialization methods
    void initialize_stars(std::vector<WeatherElements::Star>& stars, int count, 
                          int matrix_width, int matrix_height, std::mt19937& rng);
                          
    void initialize_raindrops(std::vector<WeatherElements::Raindrop>& raindrops, int count,
                             int matrix_width, int matrix_height, std::mt19937& rng, float animation_speed);
                             
    void initialize_snowflakes(std::vector<WeatherElements::Snowflake>& snowflakes, int count,
                              int matrix_width, int matrix_height, std::mt19937& rng, float animation_speed);
                              
    void initialize_clouds(std::vector<WeatherElements::Cloud>& clouds, int count,
                          int matrix_width, int matrix_height, std::mt19937& rng, float animation_speed);

    // Weather rendering methods
    void render_clear_day(rgb_matrix::Canvas* canvas, int matrix_width, int matrix_height, float elapsed_time);
    
    void render_clear_night(rgb_matrix::Canvas* canvas, 
                           std::vector<WeatherElements::Star>& stars,
                           int matrix_width, int matrix_height, float elapsed_time);
                           
    void render_cloudy(rgb_matrix::Canvas* canvas, 
                      std::vector<WeatherElements::Cloud>& clouds,
                      int matrix_width, int matrix_height,
                      bool is_day, int cloud_coverage, float elapsed_time);
                      
    void render_rain(rgb_matrix::Canvas* canvas, 
                    std::vector<WeatherElements::Cloud>& clouds,
                    std::vector<WeatherElements::Raindrop>& raindrops,
                    int matrix_width, int matrix_height, int intensity);
                    
    void render_snow(rgb_matrix::Canvas* canvas, 
                    std::vector<WeatherElements::Cloud>& clouds,
                    std::vector<WeatherElements::Snowflake>& snowflakes,
                    int matrix_width, int matrix_height, int intensity);
                    
    void render_thunderstorm(rgb_matrix::Canvas* canvas, 
                            std::vector<WeatherElements::Cloud>& clouds,
                            std::vector<WeatherElements::Raindrop>& raindrops,
                            std::vector<WeatherElements::LightningBolt>& lightning_bolts,
                            int matrix_width, int matrix_height,
                            float elapsed_time, std::chrono::time_point<std::chrono::steady_clock> start_time,
                            std::mt19937& rng);
                            
    void render_fog(rgb_matrix::Canvas* canvas,
                   std::vector<WeatherElements::Cloud>& clouds,
                   int matrix_width, int matrix_height);

    // Helper rendering methods
    void render_moon(rgb_matrix::Canvas* canvas, int matrix_width, int matrix_height);
    void render_sun(rgb_matrix::Canvas* canvas, int matrix_width, int matrix_height, float elapsed_time);
}
