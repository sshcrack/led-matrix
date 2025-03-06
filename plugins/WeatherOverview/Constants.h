#pragma once

#include <string>
#include <shared/utils/consts.h>
#include "WeatherParser.h"

// To get longitude / latitude of a city
// https://geocoding-api.open-meteo.com/v1/search?language=en&format=json&name=(Name of city)&count=(something)

extern std::string LOCATION_LAT;
extern std::string LOCATION_LON;
extern WeatherParser *parser;

extern rgb_matrix::Font HEADER_FONT;
extern rgb_matrix::Font BODY_FONT;
extern rgb_matrix::Font SMALL_FONT;

static std::string get_api_url() {
    return "https://api.open-meteo.com/v1/forecast?latitude=" + LOCATION_LAT + "&longitude=" + LOCATION_LON +
           "&current=temperature_2m,is_day,rain,weather_code,cloud_cover,relative_humidity_2m,wind_speed_10m" +
           "&daily=weather_code,temperature_2m_max,temperature_2m_min&forecast_days=5" +
           "&timezone=Europe%2FBerlin";
}

const static std::filesystem::path weather_dir = Constants::root_dir / "weather";
enum SkyColor {
    DAY_CLEAR = 0x3aa1d5,
    DAY_CLOUDS = 0x7c8c9a,
    NIGHT_CLEAR = 0x262b49,
    NIGHT_CLOUDS = 0x2d2e34
};

