#include "WeatherParser.h"
#include <nlohmann/json.hpp>
#include "Constants.h"
#include "icons/weather_icons.h"
#include <cpr/cpr.h>
#include <spdlog/spdlog.h>
#include <ctime>
#include <iomanip>
#include <sstream>

std::expected<std::string, std::string> WeatherParser::fetch_api() {
    cpr::Response r = cpr::Get(cpr::Url{get_api_url()});
    if (r.status_code != 200) {
        return std::unexpected("Could not fetch api: " + r.text);
    }

    return r.text;
}

RGB hex_to_rgb(int hex) {
    return RGB{
        static_cast<uint8_t>((hex >> 16) & 0xFF),
        static_cast<uint8_t>((hex >> 8) & 0xFF),
        static_cast<uint8_t>(hex & 0xFF)
    };
}

std::string get_day_name(const std::string &date_str) {
    std::tm tm = {};
    std::istringstream ss(date_str);
    ss >> std::get_time(&tm, "%Y-%m-%d");

    if (ss.fail()) {
        return "Unknown";
    }

    std::time_t time = std::mktime(&tm);
    char buffer[10];
    std::strftime(buffer, sizeof(buffer), "%a", std::localtime(&time));
    return {buffer};
}

std::expected<WeatherData, std::string> WeatherParser::parse_weather_data(const std::string &str_data) {
    nlohmann::json json;
    try {
        spdlog::enable_backtrace(20);
        json = nlohmann::json::parse(str_data);
        spdlog::info("Full JSON is " + str_data);

        // Check if required objects exist
        if (!json.contains("current") || !json.contains("current_units") || !json.contains("daily")) {
            return std::unexpected("Missing required fields in weather data");
        }

        spdlog::debug("Getting current");
        auto curr = json.at("current");
        if (curr.is_null()) {
            return std::unexpected("Current weather data is null");
        }

        spdlog::info("Current is " + curr.dump());

        // Check if required fields exist in current
        if (!curr.contains("is_day") || !curr.contains("cloud_cover") || !curr.contains("weather_code")) {
            return std::unexpected("Missing required fields in current weather data");
        }

        spdlog::debug("Checking if day");
        bool night = curr["is_day"].get<int>() == 0;
        spdlog::debug("Cloud cover check");
        bool clouds = curr["cloud_cover"].get<int>() > 50;

        int color = SkyColor::DAY_CLEAR;
        if (clouds) color = SkyColor::DAY_CLOUDS;
        if (night) color = SkyColor::NIGHT_CLEAR;
        if (clouds && night) color = SkyColor::NIGHT_CLOUDS;

        auto units = json.at("current_units");
        if (units.is_null() || !units.contains("temperature_2m")) {
            return std::unexpected("Missing temperature units");
        }

        auto temp_unit = units["temperature_2m"].get<std::string>();
        auto daily_units = json.at("daily_units");

        spdlog::debug("Obtaining weather_code");
        int code = curr["weather_code"].get<int>();

        // Check if weather code exists in icons
        if (!ICONS.contains(std::to_string(code))) {
            spdlog::warn("Unknown weather code: {}", code);
            return std::unexpected("Unknown weather code");
        }

        auto icon_root = ICONS.at(std::to_string(code));
        if (icon_root.is_null()) {
            return std::unexpected("Weather code has no icon data");
        }

        spdlog::debug("Getting image / description from " + icon_root[night ? "night" : "day"].dump());
        auto icon_group = night ? icon_root["night"] : icon_root["day"];

        std::string icon_url;
        std::string description;
        if (!icon_group.is_null()) {
            icon_url = icon_group.value("image", "");
            description = icon_group.value("description", "No info");
        } else {
            icon_url = "";
            description = "No info";
        }

        if (!curr.contains("temperature_2m")) {
            return std::unexpected("Missing temperature data");
        }
        auto temperature = std::to_string(curr["temperature_2m"].get<int>()) + temp_unit;

        // Get additional weather data
        std::string humidity = curr.contains("relative_humidity_2m")
                                   ? std::to_string(curr["relative_humidity_2m"].get<int>()) + "%"
                                   : "N/A";

        std::string wind_speed = curr.contains("wind_speed_10m")
                                     ? std::to_string(curr["wind_speed_10m"].get<int>()) +
                                       units.value("wind_speed_10m", "m/s")
                                     : "N/A";

        // Process forecast data
        auto daily = json.at("daily");
        std::vector<ForecastDay> forecast;

        if (!daily.is_null() && daily.contains("time") && daily.contains("weather_code") &&
            daily.contains("temperature_2m_max") && daily.contains("temperature_2m_min")) {
            auto dates = daily["time"].get<std::vector<std::string> >();
            auto codes = daily["weather_code"].get<std::vector<int> >();
            auto max_temps = daily["temperature_2m_max"].get<std::vector<float> >();
            auto min_temps = daily["temperature_2m_min"].get<std::vector<float> >();

            size_t forecast_days = std::min({
                dates.size(), codes.size(), max_temps.size(), min_temps.size(), size_t(4)
            });

            for (size_t i = 1; i < forecast_days; i++) {
                // Start from 1 to skip current day
                ForecastDay day;
                day.day_name = get_day_name(dates[i]);
                day.weatherCode = codes[i];

                // Get icon URL based on weather code
                if (ICONS.contains(std::to_string(codes[i]))) {
                    auto day_icon = ICONS.at(std::to_string(codes[i]))["day"];
                    if (!day_icon.is_null()) {
                        day.icon_url = day_icon.value("image", "");
                    }
                }

                day.temperature_max = std::to_string(static_cast<int>(max_temps[i])) + temp_unit;
                day.temperature_min = std::to_string(static_cast<int>(min_temps[i])) + temp_unit;

                forecast.push_back(day);
            }
        }

        auto data = WeatherData();
        data.color = hex_to_rgb(color);
        data.description = description;
        data.temperature = temperature;
        data.humidity = humidity;
        data.wind_speed = wind_speed;
        data.icon_url = icon_url;
        data.weatherCode = code;
        data.forecast = forecast;
        data.is_day = !night;

        spdlog::disable_backtrace();
        spdlog::trace("Setting description to " + data.description);
        spdlog::trace("Code is " + std::to_string(code));
        spdlog::trace("Temperature is " + temperature);
        spdlog::trace("Icon url is " + icon_url);
        return data;
    } catch (std::exception &ex) {
        spdlog::dump_backtrace();
        return std::unexpected("Could not parse json: " + std::string(ex.what()));
    }
}

std::expected<WeatherData, std::string> WeatherParser::get_data() {
    auto curr = GetTimeInMillis();
    if (curr - last_fetch < CACHE_INVALIDATION) {
        return cached_data.value();
    }

    auto api = fetch_api();
    if (!api) {
        return std::unexpected(api.error());
    }

    auto data = parse_weather_data(api.value());
    if (!data) {
        return std::unexpected(data.error());
    }

    changed = true;
    last_fetch = curr;
    cached_data = data.value();

    return data.value();
}

bool WeatherParser::has_changed() const {
    return changed;
}

void WeatherParser::unmark_changed() {
    changed = false;
}
