#pragma once

#include <string>
#include <filesystem>
#include "shared/matrix/utils/consts.h"

#include "led-matrix.h"

// Fonts
extern rgb_matrix::Font HEADER_FONT;
extern rgb_matrix::Font BODY_FONT;
extern rgb_matrix::Font SMALL_FONT;
extern rgb_matrix::Font TINY_FONT;  // New tiny font for additional details

// Sky colors for different conditions
namespace SkyColor {
    constexpr int NIGHT_CLEAR = 0x102851;
    constexpr int NIGHT_CLOUDS = 0x0A192E;
    constexpr int DAY_CLEAR = 0x6D9EEB;
    constexpr int DAY_CLOUDS = 0x6D8ED0;
    
    // Additional sky colors for different weather conditions
    constexpr int SUNSET = 0xE65F5C;  // Sunset/sunrise color
    constexpr int RAIN = 0x536878;    // Rainy day color
    constexpr int FOG = 0x708090;     // Foggy day color
    constexpr int SNOW = 0xB0E0E6;    // Snowy day color
}

// Animation constants
namespace AnimationConstants {
    constexpr int RAIN_DROP_MAX = 30;  // Maximum number of raindrops
    constexpr int SNOW_FLAKE_MAX = 20; // Maximum number of snowflakes
    constexpr int FRAME_RATE = 20;     // Frames per second for animations
    constexpr int ANIMATION_INTERVAL = 1000 / FRAME_RATE; // milliseconds between frames
}

const static std::filesystem::path weather_dir = Constants::root_dir / "weather";
