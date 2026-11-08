#pragma once

// Real-world environment shared by every scenery scene: where the matrix is,
// where the sun and moon are, what the weather is doing, which season it is
// and which occasions (holidays) are on. The daemon's Environment::Service
// publishes location, weather, the user's global overrides and the active
// occasions as the "environment" Runtime Input (so the desktop scene worker
// sees the same data); scenes read Environment::current(), which adds the
// sun/moon position computed locally from the wall clock.
//
// Scenes normally use SceneEnvironment (look.h) instead of this header. It
// resolves, per aspect (time of day, weather, season):
//     scene property  >  global override  >  real world  >  scene default
// and turns the result into smoothed paint parameters.

#include <chrono>
#include <optional>

#include <shared/matrix/runtime_inputs.h>
#include <string>
#include <utility>
#include <vector>

namespace Environment {

/// Weather families. Mapped from WMO weather codes.
enum class Weather { Clear, PartlyCloudy, Overcast, Fog, Drizzle, Rain, Snow, Storm };
enum class Season { Spring, Summer, Autumn, Winter };
enum class TimeOfDay { Dawn, Day, Dusk, Night };

struct SunPosition {
    double elevation_deg = 0.0;  // above the horizon, negative below
    double azimuth_deg = 0.0;    // 0 north, 90 east, 180 south, 270 west
};

struct Conditions {
    // ---- real world (each flag says whether that part is known) ----
    bool sun_known = false;      // location known and real-world sync enabled
    bool weather_known = false;  // a fresh weather report is available
    bool season_known = false;   // real-world sync enabled (calendar based)
    double latitude = 0.0, longitude = 0.0;
    SunPosition sun;
    bool morning = true;             // sun is east of the meridian
    double moon_phase = 0.5;         // 0 new, 0.5 full, 1 new again
    double moon_illumination = 1.0;  // 0..1 lit fraction
    Weather weather = Weather::Clear;
    float weather_intensity = 0.0f;  // 0..1 within the family
    float cloud_cover = 0.0f;        // 0..1
    float temperature_c = 15.0f;
    float wind_speed_ms = 2.0f;
    float wind_direction_deg = 270.0f;  // where the wind comes from
    float snow_depth_m = 0.0f;
    Season season = Season::Summer;
    float day_of_year = 172.0f;  // local, 0-based
    double local_hour = 12.0;    // 0..24 at the location

    // ---- global overrides from the web UI ----
    std::optional<TimeOfDay> forced_time;
    std::optional<Weather> forced_weather;
    std::optional<Season> forced_season;

    // ---- occasions (ids from occasions.h) currently shown ----
    std::vector<std::string> occasions;

    [[nodiscard]] bool has_occasion(std::string_view id) const;
};

// ---- pure helpers (unit-tested) -------------------------------------------
[[nodiscard]] SunPosition sun_position(double latitude, double longitude,
                                       std::chrono::system_clock::time_point when);
/// Moon phase 0..1 (0 = new moon, 0.5 = full moon).
[[nodiscard]] double moon_phase(std::chrono::system_clock::time_point when);
/// Meteorological season for a day of year (0-based) and hemisphere.
/// Meteorological season for a calendar month (1-12) at a latitude.
[[nodiscard]] Season season_for(int month, double latitude);
/// WMO weather code (Open-Meteo `weather_code`) to family + 0..1 intensity.
[[nodiscard]] std::pair<Weather, float> weather_from_wmo(int code);
[[nodiscard]] const char *to_string(Weather weather);
[[nodiscard]] const char *to_string(Season season);
[[nodiscard]] const char *to_string(TimeOfDay time);
/// Parse the lowercase override ids from settings.h ("auto" -> nullopt).
[[nodiscard]] std::optional<TimeOfDay> parse_time_of_day(std::string_view id);
[[nodiscard]] std::optional<Weather> parse_weather_choice(std::string_view id);
[[nodiscard]] std::optional<Season> parse_season(std::string_view id);

struct WeatherReport {
    int wmo_code = 0;
    float cloud_cover = 0.0f;  // 0..1
    float temperature_c = 15.0f;
    float wind_speed_ms = 2.0f;
    float wind_direction_deg = 270.0f;
    float snow_depth_m = 0.0f;
    int utc_offset_seconds = 0;
};

/// Real-world part of the conditions for `when` at a location.
[[nodiscard]] Conditions conditions_at(double latitude, double longitude,
                                       std::chrono::system_clock::time_point when,
                                       const std::optional<WeatherReport> &weather,
                                       std::optional<int> utc_offset_seconds);

/// Conditions right now, from the "environment" Runtime Input. Cheap enough
/// to call about once per second; SceneEnvironment caches it. Returns a
/// default (nothing known, nothing forced) object when the feature is off.
[[nodiscard]] Conditions current();
/// Same, from a given snapshot and time (the director passes its own).
[[nodiscard]] Conditions conditions_from(const RuntimeInputs::Snapshot &snapshot,
                                         std::chrono::system_clock::time_point now);

/// Coarse time of day for a sun elevation (dawn/dusk below 10 degrees).
[[nodiscard]] TimeOfDay time_of_day_for(double elevation_deg, bool morning);

/// What the world looks like right now with the global overrides applied,
/// each part only when it is known. Used to pick scenes that fit the moment.
struct Moment {
    std::optional<TimeOfDay> time;
    std::optional<Weather> weather;
    std::optional<Season> season;
    std::vector<std::string> occasions;
};
[[nodiscard]] Moment moment_of(const Conditions &conditions);
/// Lowercase ids used by `fits:<id>` scene tags: dawn/day/dusk/night,
/// clear/cloudy/fog/rain/snow/storm, spring/summer/autumn/winter.
[[nodiscard]] const char *fit_id(TimeOfDay time);
[[nodiscard]] const char *fit_id(Weather weather);
[[nodiscard]] const char *fit_id(Season season);

} // namespace Environment
