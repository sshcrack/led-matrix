#include "WeatherParser.h"
#include <nlohmann/json.hpp>
#include "Constants.h"
#include "icons/weather_icons.h"
#include <cpr/cpr.h>
#include <spdlog/spdlog.h>

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

std::expected<WeatherData, std::string> WeatherParser::parse_weather_data(const std::string &str_data) {
    nlohmann::json json;
    try {
        spdlog::enable_backtrace(20);
        json = nlohmann::json::parse(str_data);
        spdlog::info("Full JSON is " + str_data);

        spdlog::debug("Getting current");
        auto curr = json.at("current");
        spdlog::info("Current is " + curr.dump());
        spdlog::debug("Checking if day");
        bool night = curr["is_day"].get<int>() == 0;
        spdlog::debug("Cloud cover check");
        bool clouds = curr["cloud_cover"].get<int>() > 50;

        int color = SkyColor::DAY_CLEAR;

        if (clouds) color = SkyColor::DAY_CLOUDS;
        if (night) color = SkyColor::NIGHT_CLEAR;
        if (clouds && night) color = SkyColor::NIGHT_CLOUDS;

        auto units = json.at("current_units");
        auto temp_unit = units["temperature_2m"].get<std::string>();

        spdlog::debug("Obtaining weather_code");
        int code = curr["weather_code"].get<int>();
        auto icon_root = ICONS.at(std::to_string(code));

        spdlog::debug("Getting image / description from " + icon_root["night"].dump());
        auto icon_group = icon_root.is_null() ? nullptr : (night ? icon_root["night"] : icon_root["day"]);
        auto icon_url = icon_group == nullptr ? "" : icon_group["image"].get<std::string>();
        auto description = icon_group == nullptr ? "No info" : icon_group["description"].get<std::string>();

        auto temperature = std::to_string(curr["temperature_2m"].get<int>()) + temp_unit;

        auto data = WeatherData();
        data.color = hex_to_rgb(color);
        data.description = description;
        data.temperature = temperature;
        data.icon_url = icon_url;
        data.weatherCode = code;

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

bool WeatherParser::has_changed() {
    return changed;
}

void WeatherParser::unmark_changed() {
    changed = false;
}
