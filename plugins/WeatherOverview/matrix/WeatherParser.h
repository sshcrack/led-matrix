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

// New structure for air quality data
struct AirQualityData {
    int index;               // AQI value
    std::string category;    // Good, Moderate, etc.
    RGB color;               // Indicator color
};

struct ForecastDay {
    std::string day_name;
    std::string icon_url;
    std::string temperature_min;
    std::string temperature_max;
    int weatherCode;
    float precipitation_chance; // New field for precipitation probability
};

struct WeatherData {
    RGB color{0, 0, 0};  // Default to black
    std::string icon_url;
    std::string description{"No data"};
    std::string temperature{"N/A"};
    std::string humidity{"N/A"};
    std::string wind_speed{"N/A"};
    std::string last_updated_time{"N/A"}; // New field for display timestamp
    std::string sunrise{"N/A"};           // Sunrise time
    std::string sunset{"N/A"};            // Sunset time
    float precipitation{0.0f};            // Current precipitation value
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
    static std::expected<std::string, std::string> fetch_api(const std::string& lat, const std::string& lon);
    [[nodiscard]] bool has_changed() const;
    void unmark_changed();

    static std::expected<WeatherData, std::string> parse_weather_data(const std::string &str_data);

    std::expected<WeatherData, std::string> get_data(const std::string& lat, const std::string& lon);
};
