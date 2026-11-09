#pragma once

// Daemon-side producer of the "environment" Runtime Input. One background
// thread resolves the location (manual, cached or by IP), fetches the weather
// from Open-Meteo every 15 minutes and publishes both. Nothing runs while the
// feature is turned off. Scenes never talk to the service; they read the
// Runtime Input through Environment::current().

#include <condition_variable>
#include <expected>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include <nlohmann/json.hpp>

#include "environment.h"
#include "settings.h"

namespace Environment {

/// One Open-Meteo request for the current weather at a location.
[[nodiscard]] std::expected<WeatherReport, std::string> fetch_weather(double latitude, double longitude);
/// Parses an Open-Meteo forecast response (exposed for tests).
[[nodiscard]] std::expected<WeatherReport, std::string> parse_weather(const std::string &body);
/// Place search through the Open-Meteo geocoding API: [{name, latitude, longitude}].
[[nodiscard]] std::expected<nlohmann::json, std::string> geocode(const std::string &query);

class Service {
public:
    using PersistFn = std::function<void(const Settings &)>;

    static Service &instance();

    /// Starts the worker thread. `persist` is called (from the worker thread)
    /// when an IP lookup found a new location worth caching; only the
    /// detected_* fields of the passed settings are meant to be stored.
    void start(Settings settings, PersistFn persist);
    void stop();

    void update_settings(Settings settings);
    [[nodiscard]] Settings settings() const;
    /// Settings + resolved location + latest report + derived conditions.
    [[nodiscard]] nlohmann::json status() const;

    ~Service();

private:
    Service() = default;
    void run();
    void publish_locked();

    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::thread worker_;
    bool running_ = false;
    bool settings_changed_ = false;
    Settings settings_;
    PersistFn persist_;

    std::optional<double> lat_, lon_;
    std::string location_label_, location_source_;
    std::optional<WeatherReport> weather_;
    std::chrono::system_clock::time_point weather_at_{};
    std::string last_error_;
};

} // namespace Environment
