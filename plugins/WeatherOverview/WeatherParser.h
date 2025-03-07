#pragma once

#include <expected>
#include <string>
#include <optional>
#include <vector>
#include "shared/utils/utils.h"

struct RGB {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

struct ForecastDay {
    std::string day_name;
    std::string icon_url;
    std::string temperature_min;
    std::string temperature_max;
    int weatherCode;
};

struct WeatherData {
    RGB color{0, 0, 0};  // Default to black
    std::string icon_url;
    std::string description{"No data"};
    std::string temperature{"N/A"};
    std::string humidity{"N/A"};
    std::string wind_speed{"N/A"};
    int weatherCode{0};
    bool is_day;
    std::vector<ForecastDay> forecast;
};

static long CACHE_INVALIDATION = 1000 * 60 * 15;

class WeatherParser {
private:
    tmillis_t last_fetch = 0;
    std::optional<WeatherData> cached_data;
    bool changed = false;
public:
    static std::expected<std::string, std::string> fetch_api();
    bool has_changed() const;
    void unmark_changed();

    static std::expected<WeatherData, std::string> parse_weather_data(const std::string &str_data);

    std::expected<WeatherData, std::string> get_data();
};
