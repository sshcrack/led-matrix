#pragma once

#include <string>

// To get longitude / latitude of a city
// https://geocoding-api.open-meteo.com/v1/search?language=en&format=json&name=(Name of city)&count=(something)

extern std::string LOCATION_LAT;
extern std::string LOCATION_LON;
extern WeatherParser *parser;

static std::string HEADER_FONT_FILE = "./rpi-rgb-led-matrix/fonts/7x13.bdf";
static std::string BODY_FONT_FILE = "./rpi-rgb-led-matrix/fonts/5x8.bdf";

extern rgb_matrix::Font HEADER_FONT;
extern rgb_matrix::Font BODY_FONT;

static std::string get_api_url() {
    return "https://api.open-meteo.com/v1/forecast?latitude=" + LOCATION_LAT + "&longitude=" + LOCATION_LON +
           "&current=temperature_2m,is_day,rain,weather_code,cloud_cover&daily=weather_code&timezone=Europe%2FBerlin";
}

enum SkyColor {
    DAY_CLEAR = 0x3aa1d5,
    DAY_CLOUDS = 0x7c8c9a,
    NIGHT_CLEAR = 0x262b49,
    NIGHT_CLOUDS = 0x2d2e34
};

