#include <shared/matrix/environment/service.h>
#include <shared/matrix/environment/occasions.h>

#include <shared/matrix/input_ids.h>
#include <shared/matrix/runtime_inputs.h>

#include <cpr/cpr.h>
#include <spdlog/spdlog.h>

#include <cmath>
#include <iomanip>
#include <sstream>

namespace Environment {
namespace {
using namespace std::chrono_literals;
constexpr auto kWeatherInterval = 15min;
constexpr auto kRetryInterval = 3min;
constexpr auto kLocateRetryInterval = 30min;
constexpr auto kRepublishInterval = 5min;

std::string coordinate(double v)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(4) << v;
    return out.str();
}

struct Located {
    double latitude, longitude;
    std::string label;
};

std::expected<Located, std::string> locate_by_ip()
{
    // Two free, key-less services; the second is only asked when the first fails.
    {
        const auto r = cpr::Get(cpr::Url{"https://ipwho.is/"}, cpr::Timeout{6000L});
        if (r.status_code == 200) {
            try {
                const auto j = nlohmann::json::parse(r.text);
                if (j.value("success", false) && j.contains("latitude") && j.contains("longitude"))
                    return Located{j.at("latitude").get<double>(), j.at("longitude").get<double>(),
                                   j.value("city", std::string{}) + (j.contains("country") ? ", " + j.value("country", std::string{}) : "")};
            } catch (const std::exception &) {}
        }
    }
    const auto r = cpr::Get(cpr::Url{"https://ipapi.co/json/"}, cpr::Timeout{6000L});
    if (r.status_code != 200)
        return std::unexpected("IP location lookup failed (HTTP " + std::to_string(r.status_code) + ")");
    try {
        const auto j = nlohmann::json::parse(r.text);
        if (!j.contains("latitude") || !j.at("latitude").is_number())
            return std::unexpected("IP location lookup returned no coordinates");
        return Located{j.at("latitude").get<double>(), j.at("longitude").get<double>(),
                       j.value("city", std::string{}) + ", " + j.value("country_name", std::string{})};
    } catch (const std::exception &ex) {
        return std::unexpected(std::string("IP location lookup: ") + ex.what());
    }
}
} // namespace

std::expected<WeatherReport, std::string> parse_weather(const std::string &body)
{
    try {
        const auto j = nlohmann::json::parse(body);
        if (!j.contains("current") || !j.at("current").is_object())
            return std::unexpected("weather response has no current block");
        const auto &cur = j.at("current");
        auto num = [&](const char *key, double fallback) {
            return cur.contains(key) && cur.at(key).is_number() ? cur.at(key).get<double>() : fallback;
        };
        if (!cur.contains("weather_code") || !cur.at("weather_code").is_number())
            return std::unexpected("weather response has no weather_code");
        WeatherReport r;
        r.wmo_code = cur.at("weather_code").get<int>();
        r.cloud_cover = static_cast<float>(num("cloud_cover", 0.0) / 100.0);
        r.temperature_c = static_cast<float>(num("temperature_2m", 15.0));
        // Requested in m/s (wind_speed_unit=ms).
        r.wind_speed_ms = static_cast<float>(num("wind_speed_10m", 2.0));
        r.wind_direction_deg = static_cast<float>(num("wind_direction_10m", 270.0));
        r.snow_depth_m = static_cast<float>(num("snow_depth", 0.0));
        r.utc_offset_seconds = j.contains("utc_offset_seconds") && j.at("utc_offset_seconds").is_number()
                                   ? j.at("utc_offset_seconds").get<int>()
                                   : 0;
        return r;
    } catch (const std::exception &ex) {
        return std::unexpected(std::string("weather response: ") + ex.what());
    }
}

std::expected<WeatherReport, std::string> fetch_weather(double latitude, double longitude)
{
    const std::string url = "https://api.open-meteo.com/v1/forecast?latitude=" + coordinate(latitude) +
                            "&longitude=" + coordinate(longitude) +
                            "&current=temperature_2m,weather_code,cloud_cover,wind_speed_10m,wind_direction_10m,snow_depth"
                            "&wind_speed_unit=ms&timezone=auto&forecast_days=1";
    const auto r = cpr::Get(cpr::Url{url}, cpr::Timeout{8000L});
    if (r.status_code != 200)
        return std::unexpected("weather request failed (HTTP " + std::to_string(r.status_code) + "): " +
                               (r.error.message.empty() ? r.text.substr(0, 200) : r.error.message));
    return parse_weather(r.text);
}

std::expected<nlohmann::json, std::string> geocode(const std::string &query)
{
    if (query.empty() || query.size() > 100)
        return std::unexpected("query must be 1-100 characters");
    const auto r = cpr::Get(cpr::Url{"https://geocoding-api.open-meteo.com/v1/search"},
                            cpr::Parameters{{"name", query}, {"count", "6"}, {"format", "json"}}, cpr::Timeout{6000L});
    if (r.status_code != 200)
        return std::unexpected("place search failed (HTTP " + std::to_string(r.status_code) + ")");
    try {
        const auto j = nlohmann::json::parse(r.text);
        auto out = nlohmann::json::array();
        if (j.contains("results") && j.at("results").is_array())
            for (const auto &p : j.at("results")) {
                if (!p.contains("latitude") || !p.contains("longitude"))
                    continue;
                std::string name = p.value("name", std::string{});
                for (const char *part : {"admin1", "country"})
                    if (p.contains(part) && p.at(part).is_string() && !p.at(part).get<std::string>().empty())
                        name += ", " + p.at(part).get<std::string>();
                out.push_back({{"name", name}, {"latitude", p.at("latitude")}, {"longitude", p.at("longitude")}});
            }
        return out;
    } catch (const std::exception &ex) {
        return std::unexpected(std::string("place search: ") + ex.what());
    }
}

Service &Service::instance()
{
    static Service service;
    return service;
}

Service::~Service() { stop(); }

void Service::start(Settings settings, PersistFn persist)
{
    std::lock_guard lock(mutex_);
    if (running_)
        return;
    settings.sanitize();
    settings_ = std::move(settings);
    persist_ = std::move(persist);
    running_ = true;
    settings_changed_ = true;
    worker_ = std::thread([this] { run(); });
}

void Service::stop()
{
    {
        std::lock_guard lock(mutex_);
        if (!running_)
            return;
        running_ = false;
    }
    wake_.notify_all();
    if (worker_.joinable())
        worker_.join();
    RuntimeInputs::clear(RuntimeInputIds::Environment);
}

void Service::update_settings(Settings settings)
{
    settings.sanitize();
    {
        std::lock_guard lock(mutex_);
        // Keep the cached detection unless the caller replaced it.
        if (!settings.detected_latitude && settings_.detected_latitude) {
            settings.detected_latitude = settings_.detected_latitude;
            settings.detected_longitude = settings_.detected_longitude;
            settings.detected_label = settings_.detected_label;
        }
        settings_ = std::move(settings);
        settings_changed_ = true;
    }
    wake_.notify_all();
}

Settings Service::settings() const
{
    std::lock_guard lock(mutex_);
    return settings_;
}

nlohmann::json Service::status() const
{
    std::lock_guard lock(mutex_);
    nlohmann::json j = settings_;
    j["location"] = nullptr;
    if (lat_ && lon_)
        j["location"] = {{"latitude", *lat_}, {"longitude", *lon_}, {"label", location_label_}, {"source", location_source_}};
    j["error"] = last_error_.empty() ? nlohmann::json(nullptr) : nlohmann::json(last_error_);
    j["conditions"] = nullptr;
    j["occasions"] = nlohmann::json::array();
    for (const auto &o : occasions())
        j["occasions"].push_back({{"id", o.id}, {"label", o.label}});
    {
        // What scenes on "auto" see right now (overrides applied).
        const auto now_c = current();
        j["active_occasions"] = now_c.occasions;
    }
    if (settings_.enabled && lat_ && lon_) {
        const auto c = conditions_at(*lat_, *lon_, std::chrono::system_clock::now(), weather_,
                                     weather_ ? std::optional<int>(weather_->utc_offset_seconds) : std::nullopt);
        nlohmann::json cj{{"sun_elevation_deg", std::round(c.sun.elevation_deg * 10.0) / 10.0},
                          {"sun_azimuth_deg", std::round(c.sun.azimuth_deg)},
                          {"moon_illumination", std::round(c.moon_illumination * 100.0) / 100.0},
                          {"season", to_string(c.season)},
                          {"local_hour", std::round(c.local_hour * 100.0) / 100.0},
                          {"weather", c.weather_known ? nlohmann::json(to_string(c.weather)) : nlohmann::json(nullptr)}};
        if (c.weather_known) {
            cj["temperature_c"] = c.temperature_c;
            cj["cloud_cover"] = c.cloud_cover;
            cj["wind_speed_ms"] = c.wind_speed_ms;
            cj["snow_depth_m"] = c.snow_depth_m;
            cj["weather_age_seconds"] = std::chrono::duration_cast<std::chrono::seconds>(
                                            std::chrono::system_clock::now() - weather_at_).count();
        }
        j["conditions"] = cj;
    }
    return j;
}

void Service::publish_locked()
{
    const bool real = settings_.enabled && lat_ && lon_;
    if (!real && !settings_.overrides.any() && !settings_.enabled) {
        RuntimeInputs::set_available(RuntimeInputIds::Environment, false);
        return;
    }
    RuntimeInputs::Signals s{
        {"real_world", settings_.enabled},
        {"time_override", settings_.overrides.time_of_day},
        {"weather_override", settings_.overrides.weather},
        {"season_override", settings_.overrides.season},
        {"occasions", settings_.overrides.occasions},
    };
    if (real) {
        s["latitude"] = *lat_;
        s["longitude"] = *lon_;
        s["source"] = location_source_;
    }
    if (real && weather_) {
        s["weather_code"] = static_cast<std::int64_t>(weather_->wmo_code);
        s["cloud_cover"] = static_cast<double>(weather_->cloud_cover);
        s["temperature_c"] = static_cast<double>(weather_->temperature_c);
        s["wind_speed_ms"] = static_cast<double>(weather_->wind_speed_ms);
        s["wind_direction_deg"] = static_cast<double>(weather_->wind_direction_deg);
        s["snow_depth_m"] = static_cast<double>(weather_->snow_depth_m);
        s["utc_offset_seconds"] = static_cast<std::int64_t>(weather_->utc_offset_seconds);
        s["weather_unix"] = std::chrono::duration<double>(weather_at_.time_since_epoch()).count();
    }
    RuntimeInputs::set_available(RuntimeInputIds::Environment, true, std::move(s));
}

void Service::run()
{
    using Clock = std::chrono::steady_clock;
    Clock::time_point next_weather{}, next_locate{}, next_publish{};
    std::unique_lock lock(mutex_);
    while (running_) {
        if (settings_changed_) {
            settings_changed_ = false;
            // Re-resolve the location now; fetch the weather right after.
            std::optional<double> lat, lon;
            std::string label, source;
            if (settings_.has_manual_location()) {
                lat = settings_.latitude;
                lon = settings_.longitude;
                label = settings_.label;
                source = "manual";
            } else if (settings_.auto_locate && settings_.detected_latitude) {
                lat = settings_.detected_latitude;
                lon = settings_.detected_longitude;
                label = settings_.detected_label;
                source = "ip";
            }
            const bool moved = lat != lat_ || lon != lon_;
            lat_ = lat;
            lon_ = lon;
            location_label_ = label;
            location_source_ = source;
            if (moved)
                weather_.reset();
            next_locate = Clock::now();
            next_weather = Clock::now();
            last_error_.clear();
            publish_locked();
        }

        const auto now = Clock::now();
        if (settings_.enabled) {
            // Locate by IP once when nothing else is known, retrying slowly.
            if (!lat_ && settings_.auto_locate && !settings_.has_manual_location() && now >= next_locate) {
                lock.unlock();
                auto found = locate_by_ip();
                lock.lock();
                if (!running_)
                    break;
                if (found && !settings_changed_ && !settings_.has_manual_location()) {
                    spdlog::info("Environment: located by IP at {} ({:.3f}, {:.3f})", found->label, found->latitude, found->longitude);
                    lat_ = found->latitude;
                    lon_ = found->longitude;
                    location_label_ = found->label;
                    location_source_ = "ip";
                    settings_.detected_latitude = found->latitude;
                    settings_.detected_longitude = found->longitude;
                    settings_.detected_label = found->label;
                    if (persist_) {
                        const auto copy = settings_;
                        auto persist = persist_;
                        lock.unlock();
                        persist(copy);
                        lock.lock();
                    }
                    next_weather = Clock::now();
                    publish_locked();
                } else if (!found) {
                    last_error_ = found.error();
                    spdlog::warn("Environment: {}", last_error_);
                }
                next_locate = Clock::now() + kLocateRetryInterval;
            }
            if (lat_ && lon_ && Clock::now() >= next_weather) {
                const double lat = *lat_, lon = *lon_;
                lock.unlock();
                auto report = fetch_weather(lat, lon);
                lock.lock();
                if (!running_)
                    break;
                if (settings_changed_ || lat_ != lat || lon_ != lon)
                    continue;  // stale request for a location that just changed
                if (report) {
                    weather_ = *report;
                    weather_at_ = std::chrono::system_clock::now();
                    last_error_.clear();
                    next_weather = Clock::now() + kWeatherInterval;
                } else {
                    last_error_ = report.error();
                    spdlog::warn("Environment: {}", last_error_);
                    next_weather = Clock::now() + kRetryInterval;
                }
                publish_locked();
                next_publish = Clock::now() + kRepublishInterval;
            }
            if (Clock::now() >= next_publish) {
                publish_locked();
                next_publish = Clock::now() + kRepublishInterval;
            }
        } else {
            next_publish = Clock::now() + kRepublishInterval;
        }

        auto deadline = next_publish;
        if (settings_.enabled && lat_)
            deadline = std::min(deadline, next_weather);
        else if (settings_.enabled && settings_.auto_locate)
            deadline = std::min(deadline, next_locate);
        wake_.wait_until(lock, std::max(deadline, Clock::now() + 1s), [this] { return !running_ || settings_changed_; });
    }
}

} // namespace Environment
