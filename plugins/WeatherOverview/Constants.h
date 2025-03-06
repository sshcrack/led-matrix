#pragma once

#include <string>
#include <shared/utils/consts.h>

#include "WeatherParser.h"


// Global weather API settings
extern std::string LOCATION_LAT;
extern std::string LOCATION_LON;

// Shared parser instance
extern WeatherParser* parser;

// Fonts
extern rgb_matrix::Font HEADER_FONT;
extern rgb_matrix::Font BODY_FONT;
extern rgb_matrix::Font SMALL_FONT;

// Sky colors for different conditions
namespace SkyColor {
    constexpr int NIGHT_CLEAR = 0x102851;
    constexpr int NIGHT_CLOUDS = 0x0A192E;
    constexpr int DAY_CLEAR = 0x6D9EEB;
    constexpr int DAY_CLOUDS = 0x6D8ED0;
}

// Weather API URL helper
static std::string get_api_url() {
    return "https://api.open-meteo.com/v1/forecast?latitude=" + LOCATION_LAT + 
           "&longitude=" + LOCATION_LON + 
           "&current=temperature_2m,relative_humidity_2m,is_day,precipitation,weather_code,cloud_cover,wind_speed_10m" +
           "&daily=weather_code,temperature_2m_max,temperature_2m_min&timezone=auto";
}

// Weather icons are defined in the icons/weather_icons.h file
const static std::filesystem::path weather_dir = Constants::root_dir / "weather";
