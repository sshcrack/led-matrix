#include <shared/matrix/environment/environment.h>
#include <shared/matrix/environment/occasions.h>
#include <shared/matrix/environment/settings.h>

#include <shared/matrix/input_ids.h>
#include <shared/matrix/runtime_inputs.h>

#include <algorithm>
#include <cmath>
#include <ctime>

namespace Environment {
namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180.0;

double julian_day(std::chrono::system_clock::time_point when)
{
    const double unix_seconds =
        std::chrono::duration<double>(when.time_since_epoch()).count();
    return unix_seconds / 86400.0 + 2440587.5;
}

double wrap(double value, double range)
{
    value = std::fmod(value, range);
    return value < 0.0 ? value + range : value;
}

int system_utc_offset(std::time_t t)
{
#ifdef _WIN32
    std::tm local{}, utc{};
    localtime_s(&local, &t);
    gmtime_s(&utc, &t);
    local.tm_isdst = -1;
    utc.tm_isdst = -1;
    return static_cast<int>(std::difftime(std::mktime(&local), std::mktime(&utc)));
#else
    std::tm local{};
    localtime_r(&t, &local);
    return static_cast<int>(local.tm_gmtoff);
#endif
}

std::tm shifted_tm(std::time_t t, int offset)
{
    const std::time_t shifted = t + offset;
    std::tm local{};
#ifdef _WIN32
    gmtime_s(&local, &shifted);
#else
    gmtime_r(&shifted, &local);
#endif
    return local;
}
} // namespace

SunPosition sun_position(double latitude, double longitude, std::chrono::system_clock::time_point when)
{
    // Low-precision solar ephemeris (Astronomical Almanac), ~0.01° accuracy
    // which is far below one LED.
    const double n = julian_day(when) - 2451545.0;
    const double L = wrap(280.460 + 0.9856474 * n, 360.0);
    const double g = wrap(357.528 + 0.9856003 * n, 360.0) * kDeg;
    const double lambda = (L + 1.915 * std::sin(g) + 0.020 * std::sin(2.0 * g)) * kDeg;
    const double epsilon = (23.439 - 0.0000004 * n) * kDeg;
    const double ra = std::atan2(std::cos(epsilon) * std::sin(lambda), std::cos(lambda));
    const double dec = std::asin(std::sin(epsilon) * std::sin(lambda));
    const double gmst_hours = wrap(18.697374558 + 24.06570982441908 * n, 24.0);
    const double hour_angle = (gmst_hours * 15.0 + longitude) * kDeg - ra;
    const double lat = latitude * kDeg;

    SunPosition p;
    p.elevation_deg = std::asin(std::clamp(
                          std::sin(lat) * std::sin(dec) + std::cos(lat) * std::cos(dec) * std::cos(hour_angle), -1.0, 1.0)) /
                      kDeg;
    p.azimuth_deg = wrap(std::atan2(-std::sin(hour_angle),
                                    std::cos(lat) * std::tan(dec) - std::sin(lat) * std::cos(hour_angle)) /
                             kDeg,
                         360.0);
    return p;
}

double moon_phase(std::chrono::system_clock::time_point when)
{
    constexpr double synodic_month = 29.530588853;
    constexpr double reference_new_moon = 2451550.26;  // 2000-01-06 18:14 UTC
    return wrap((julian_day(when) - reference_new_moon) / synodic_month, 1.0);
}

Season season_for(int month, double latitude)
{
    // Meteorological seasons by calendar month (so leap years cannot shift
    // them): Mar-May, Jun-Aug, Sep-Nov, Dec-Feb.
    Season north;
    if (month >= 3 && month <= 5)
        north = Season::Spring;
    else if (month >= 6 && month <= 8)
        north = Season::Summer;
    else if (month >= 9 && month <= 11)
        north = Season::Autumn;
    else
        north = Season::Winter;
    if (latitude >= 0.0)
        return north;
    switch (north) {
    case Season::Spring: return Season::Autumn;
    case Season::Summer: return Season::Winter;
    case Season::Autumn: return Season::Spring;
    case Season::Winter: return Season::Summer;
    }
    return north;
}

std::pair<Weather, float> weather_from_wmo(int code)
{
    switch (code) {
    case 0: return {Weather::Clear, 0.0f};
    case 1: return {Weather::Clear, 0.2f};
    case 2: return {Weather::PartlyCloudy, 0.5f};
    case 3: return {Weather::Overcast, 0.8f};
    case 45: return {Weather::Fog, 0.7f};
    case 48: return {Weather::Fog, 0.9f};
    case 51: case 56: return {Weather::Drizzle, 0.25f};
    case 53: return {Weather::Drizzle, 0.4f};
    case 55: case 57: return {Weather::Drizzle, 0.55f};
    case 61: case 80: return {Weather::Rain, 0.4f};
    case 63: case 66: case 81: return {Weather::Rain, 0.65f};
    case 65: case 67: case 82: return {Weather::Rain, 0.95f};
    case 71: case 77: case 85: return {Weather::Snow, 0.4f};
    case 73: return {Weather::Snow, 0.65f};
    case 75: case 86: return {Weather::Snow, 0.95f};
    case 95: return {Weather::Storm, 0.7f};
    case 96: case 99: return {Weather::Storm, 1.0f};
    default:
        if (code > 0 && code < 45)
            return {Weather::PartlyCloudy, 0.4f};
        return {Weather::Clear, 0.0f};
    }
}

const char *to_string(Weather weather)
{
    switch (weather) {
    case Weather::Clear: return "clear";
    case Weather::PartlyCloudy: return "partly_cloudy";
    case Weather::Overcast: return "overcast";
    case Weather::Fog: return "fog";
    case Weather::Drizzle: return "drizzle";
    case Weather::Rain: return "rain";
    case Weather::Snow: return "snow";
    case Weather::Storm: return "storm";
    }
    return "clear";
}

const char *to_string(TimeOfDay time)
{
    switch (time) {
    case TimeOfDay::Dawn: return "dawn";
    case TimeOfDay::Day: return "day";
    case TimeOfDay::Dusk: return "dusk";
    case TimeOfDay::Night: return "night";
    }
    return "day";
}

std::optional<TimeOfDay> parse_time_of_day(std::string_view id)
{
    if (id == "dawn") return TimeOfDay::Dawn;
    if (id == "day") return TimeOfDay::Day;
    if (id == "dusk") return TimeOfDay::Dusk;
    if (id == "night") return TimeOfDay::Night;
    return std::nullopt;
}

std::optional<Weather> parse_weather_choice(std::string_view id)
{
    if (id == "clear") return Weather::Clear;
    if (id == "cloudy") return Weather::Overcast;
    if (id == "rain") return Weather::Rain;
    if (id == "snow") return Weather::Snow;
    if (id == "fog") return Weather::Fog;
    if (id == "storm") return Weather::Storm;
    return std::nullopt;
}

std::optional<Season> parse_season(std::string_view id)
{
    if (id == "spring") return Season::Spring;
    if (id == "summer") return Season::Summer;
    if (id == "autumn") return Season::Autumn;
    if (id == "winter") return Season::Winter;
    return std::nullopt;
}

bool Conditions::has_occasion(std::string_view id) const
{
    return std::find(occasions.begin(), occasions.end(), id) != occasions.end();
}

const char *to_string(Season season)
{
    switch (season) {
    case Season::Spring: return "spring";
    case Season::Summer: return "summer";
    case Season::Autumn: return "autumn";
    case Season::Winter: return "winter";
    }
    return "summer";
}

Conditions conditions_at(double latitude, double longitude, std::chrono::system_clock::time_point when,
                         const std::optional<WeatherReport> &weather, std::optional<int> utc_offset_seconds)
{
    Conditions c;
    c.sun_known = true;
    c.season_known = true;
    c.latitude = latitude;
    c.longitude = longitude;
    c.sun = sun_position(latitude, longitude, when);
    c.morning = c.sun.azimuth_deg < 180.0;
    c.moon_phase = moon_phase(when);
    c.moon_illumination = 0.5 * (1.0 - std::cos(2.0 * kPi * c.moon_phase));

    const std::time_t t = std::chrono::system_clock::to_time_t(when);
    const int offset = utc_offset_seconds.value_or(system_utc_offset(t));
    const std::tm local = shifted_tm(t, offset);
    c.local_hour = local.tm_hour + local.tm_min / 60.0 + local.tm_sec / 3600.0;
    c.day_of_year = static_cast<float>(local.tm_yday) + static_cast<float>(c.local_hour / 24.0);
    c.season = season_for(local.tm_mon + 1, latitude);

    if (weather.has_value()) {
        c.weather_known = true;
        const auto [family, intensity] = weather_from_wmo(weather->wmo_code);
        c.weather = family;
        c.weather_intensity = intensity;
        // Measured precipitation beats the WMO code: the code can report light rain/snow while the gauge reads 0.
        const bool wet_family = family == Weather::Rain || family == Weather::Drizzle || family == Weather::Snow;
        if (wet_family && weather->precipitation_mm >= 0.0f && weather->precipitation_mm < 0.05f) {
            c.weather = weather->cloud_cover > 0.6f ? Weather::Overcast : Weather::PartlyCloudy;
            c.weather_intensity = 0.6f;
        }
        c.cloud_cover = std::clamp(weather->cloud_cover, 0.0f, 1.0f);
        c.temperature_c = weather->temperature_c;
        c.wind_speed_ms = std::max(0.0f, weather->wind_speed_ms);
        c.wind_direction_deg = weather->wind_direction_deg;
        c.snow_depth_m = std::max(0.0f, weather->snow_depth_m);
    }
    return c;
}

Conditions current() { return conditions_from(RuntimeInputs::snapshot(), std::chrono::system_clock::now()); }

TimeOfDay time_of_day_for(double elevation, bool morning)
{
    if (elevation < -6.0)
        return TimeOfDay::Night;
    if (elevation < 10.0)
        return morning ? TimeOfDay::Dawn : TimeOfDay::Dusk;
    return TimeOfDay::Day;
}

Moment moment_of(const Conditions &c)
{
    Moment m;
    if (c.forced_time)
        m.time = c.forced_time;
    else if (c.sun_known)
        m.time = time_of_day_for(c.sun.elevation_deg, c.morning);
    if (c.forced_weather)
        m.weather = c.forced_weather;
    else if (c.weather_known)
        m.weather = c.weather;
    if (c.forced_season)
        m.season = c.forced_season;
    else if (c.season_known)
        m.season = c.season;
    m.occasions = c.occasions;
    return m;
}

const char *fit_id(TimeOfDay t)
{
    switch (t) {
    case TimeOfDay::Dawn: return "dawn";
    case TimeOfDay::Day: return "day";
    case TimeOfDay::Dusk: return "dusk";
    case TimeOfDay::Night: return "night";
    }
    return "day";
}

const char *fit_id(Weather w)
{
    switch (w) {
    case Weather::Clear:
    case Weather::PartlyCloudy: return "clear";
    case Weather::Overcast: return "cloudy";
    case Weather::Fog: return "fog";
    case Weather::Drizzle:
    case Weather::Rain: return "rain";
    case Weather::Snow: return "snow";
    case Weather::Storm: return "storm";
    }
    return "clear";
}

const char *fit_id(Season s)
{
    switch (s) {
    case Season::Spring: return "spring";
    case Season::Summer: return "summer";
    case Season::Autumn: return "autumn";
    case Season::Winter: return "winter";
    }
    return "summer";
}

Conditions conditions_from(const RuntimeInputs::Snapshot &snap, std::chrono::system_clock::time_point now)
{
    const auto id = RuntimeInputIds::Environment;
    if (!snap.available(id))
        return {};
    std::optional<int> offset;
    if (const auto o = snap.number(id, "utc_offset_seconds"))
        offset = static_cast<int>(*o);

    Conditions c;
    const auto lat = snap.number(id, "latitude");
    const auto lon = snap.number(id, "longitude");
    if (lat && lon) {
        std::optional<WeatherReport> report;
        if (const auto at = snap.number(id, "weather_unix"); at && snap.number(id, "weather_code")) {
            const double age = std::chrono::duration<double>(now.time_since_epoch()).count() - *at;
            // An old report is worse than none: scenes then keep their weather.
            if (age < 3.0 * 3600.0) {
                WeatherReport r;
                r.wmo_code = static_cast<int>(*snap.number(id, "weather_code"));
                r.cloud_cover = static_cast<float>(snap.number(id, "cloud_cover").value_or(0.0));
                r.temperature_c = static_cast<float>(snap.number(id, "temperature_c").value_or(15.0));
                r.wind_speed_ms = static_cast<float>(snap.number(id, "wind_speed_ms").value_or(2.0));
                r.wind_direction_deg = static_cast<float>(snap.number(id, "wind_direction_deg").value_or(270.0));
                r.snow_depth_m = static_cast<float>(snap.number(id, "snow_depth_m").value_or(0.0));
                r.precipitation_mm = static_cast<float>(snap.number(id, "precipitation_mm").value_or(-1.0));
                report = r;
            }
        }
        c = conditions_at(*lat, *lon, now, report, offset);
    }

    c.forced_time = parse_time_of_day(snap.text(id, "time_override").value_or("auto"));
    c.forced_weather = parse_weather_choice(snap.text(id, "weather_override").value_or("auto"));
    c.forced_season = parse_season(snap.text(id, "season_override").value_or("auto"));

    // Occasions follow the local calendar ("auto", only with real-world sync
    // on), are switched off, or are a forced comma-separated list.
    const std::string mode = snap.text(id, "occasions").value_or("auto");
    if (mode == "auto") {
        if (snap.boolean(id, "real_world").value_or(false)) {
            const std::time_t t = std::chrono::system_clock::to_time_t(now);
            const std::tm local = shifted_tm(t, offset.value_or(system_utc_offset(t)));
            c.occasions = active_occasions({local.tm_year + 1900, local.tm_mon + 1, local.tm_mday});
        }
    } else if (mode != "off") {
        std::size_t pos = 0;
        while (pos <= mode.size()) {
            const auto comma = mode.find(',', pos);
            const auto item = mode.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
            if (find_occasion(item) != nullptr && !c.has_occasion(item))
                c.occasions.push_back(item);
            if (comma == std::string::npos)
                break;
            pos = comma + 1;
        }
    }
    return c;
}

// ---- settings ---------------------------------------------------------------
void Settings::sanitize()
{
    auto valid = [](const std::optional<double> &v, double limit) {
        return v.has_value() && std::isfinite(*v) && std::fabs(*v) <= limit;
    };
    if (!valid(latitude, 90.0) || !valid(longitude, 180.0)) {
        latitude.reset();
        longitude.reset();
    }
    if (!valid(detected_latitude, 90.0) || !valid(detected_longitude, 180.0)) {
        detected_latitude.reset();
        detected_longitude.reset();
    }
    if (!parse_time_of_day(overrides.time_of_day))
        overrides.time_of_day = "auto";
    if (!parse_weather_choice(overrides.weather))
        overrides.weather = "auto";
    if (!parse_season(overrides.season))
        overrides.season = "auto";
    if (overrides.occasions != "auto" && overrides.occasions != "off") {
        // Keep only known ids; an empty list means "off".
        std::string cleaned;
        std::size_t pos = 0;
        const std::string list = overrides.occasions;
        while (pos <= list.size()) {
            const auto comma = list.find(',', pos);
            const auto item = list.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
            if (find_occasion(item) != nullptr && (","+cleaned+",").find(","+item+",") == std::string::npos)
                cleaned += (cleaned.empty() ? "" : ",") + item;
            if (comma == std::string::npos)
                break;
            pos = comma + 1;
        }
        overrides.occasions = cleaned.empty() ? "off" : cleaned;
    }
    if (label.size() > 120)
        label.resize(120);
    if (detected_label.size() > 120)
        detected_label.resize(120);
}

void to_json(nlohmann::json &j, const Overrides &o)
{
    j = nlohmann::json{{"time_of_day", o.time_of_day}, {"weather", o.weather}, {"season", o.season}, {"occasions", o.occasions}};
}

void from_json(const nlohmann::json &j, Overrides &o)
{
    const Overrides defaults;
    auto text = [&](const char *key, const std::string &fallback) {
        return j.contains(key) && j.at(key).is_string() ? j.at(key).get<std::string>() : fallback;
    };
    o.time_of_day = text("time_of_day", defaults.time_of_day);
    o.weather = text("weather", defaults.weather);
    o.season = text("season", defaults.season);
    o.occasions = text("occasions", defaults.occasions);
}

void to_json(nlohmann::json &j, const Settings &s)
{
    j = nlohmann::json{{"enabled", s.enabled}, {"auto_locate", s.auto_locate}, {"label", s.label},
                       {"detected_label", s.detected_label}, {"overrides", s.overrides}};
    j["latitude"] = s.latitude ? nlohmann::json(*s.latitude) : nlohmann::json(nullptr);
    j["longitude"] = s.longitude ? nlohmann::json(*s.longitude) : nlohmann::json(nullptr);
    j["detected_latitude"] = s.detected_latitude ? nlohmann::json(*s.detected_latitude) : nlohmann::json(nullptr);
    j["detected_longitude"] = s.detected_longitude ? nlohmann::json(*s.detected_longitude) : nlohmann::json(nullptr);
}

void from_json(const nlohmann::json &j, Settings &s)
{
    const Settings defaults;
    auto number = [&](const char *key) -> std::optional<double> {
        if (!j.contains(key) || !j.at(key).is_number())
            return std::nullopt;
        return j.at(key).get<double>();
    };
    auto text = [&](const char *key) {
        return j.contains(key) && j.at(key).is_string() ? j.at(key).get<std::string>() : std::string{};
    };
    s.enabled = j.contains("enabled") && j.at("enabled").is_boolean() ? j.at("enabled").get<bool>() : defaults.enabled;
    s.auto_locate = j.contains("auto_locate") && j.at("auto_locate").is_boolean() ? j.at("auto_locate").get<bool>()
                                                                                 : defaults.auto_locate;
    s.latitude = number("latitude");
    s.longitude = number("longitude");
    s.label = text("label");
    s.detected_latitude = number("detected_latitude");
    s.detected_longitude = number("detected_longitude");
    s.detected_label = text("detected_label");
    s.overrides = j.contains("overrides") && j.at("overrides").is_object() ? j.at("overrides").get<Overrides>() : Overrides{};
    s.sanitize();
}

std::optional<LegacyLocation> legacy_weather_location(const nlohmann::json &presets)
{
    if (!presets.is_object())
        return std::nullopt;
    auto number = [](const nlohmann::json &args, const char *key) -> std::optional<double> {
        if (!args.contains(key))
            return std::nullopt;
        const auto &v = args.at(key);
        try {
            const double d = v.is_string() ? std::stod(v.get<std::string>()) : v.is_number() ? v.get<double>() : NAN;
            return std::isfinite(d) ? std::optional<double>(d) : std::nullopt;
        } catch (const std::exception &) {
            return std::nullopt;
        }
    };
    std::optional<LegacyLocation> found;
    for (const auto &[id, preset] : presets.items()) {
        if (!preset.is_object() || !preset.contains("scenes") || !preset.at("scenes").is_array())
            continue;
        for (const auto &scene : preset.at("scenes")) {
            if (!scene.is_object() || scene.value("type", std::string()) != "weather_ambience")
                continue;
            const auto args = scene.value("arguments", nlohmann::json::object());
            const auto lat = number(args, "location_lat"), lon = number(args, "location_lon");
            if (!lat || !lon || std::fabs(*lat) > 90.0 || std::fabs(*lon) > 180.0)
                continue;
            if (std::fabs(*lat - 52.52) < 1e-4 && std::fabs(*lon - 13.405) < 1e-4)
                continue;  // the old default, not a choice
            if (!found)
                found = LegacyLocation{*lat, *lon, 0};
            else if (std::fabs(found->latitude - *lat) > 1e-4 || std::fabs(found->longitude - *lon) > 1e-4)
                ++found->conflicts;
        }
    }
    return found;
}

} // namespace Environment
