#pragma once

#include <expected>
#include <string>
#include <optional>
#include "shared/utils/utils.h"

struct RGB {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

struct WeatherData {
    RGB color{};
    std::string icon_url;
    std::string description;
    int weatherCode;
};

static long CACHE_INVALIDATION = 1000 * 60 * 15;

class WeatherParser {
private:
    tmillis_t last_fetch = 0;
    std::optional<WeatherData> cached_data;
    bool changed = false;
public:
    static std::expected<std::string, std::string> fetch_api();
    bool has_changed();
    void unmark_changed();

    static std::expected<WeatherData, std::string> parse_weather_data(const std::string &str_data);

    std::expected<WeatherData, std::string> get_data();
};
