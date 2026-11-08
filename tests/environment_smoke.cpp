// Real-world environment: astronomy, weather parsing, occasions, settings
// sanitising and the scene > global override > real world > default layering.
#include <chrono>
#include <cmath>
#include <iostream>
#include <string>
#include <tuple>

#include "shared/matrix/environment/environment.h"
#include "shared/matrix/environment/look.h"
#include "shared/matrix/environment/occasions.h"
#include "shared/matrix/environment/service.h"
#include "shared/matrix/environment/settings.h"

using namespace Environment;

namespace {
int failures = 0;
void check(bool ok, const std::string& what)
{
    if (!ok) {
        std::cerr << "FAIL: " << what << '\n';
        ++failures;
    }
}
bool near(double a, double b, double tol) { return std::fabs(a - b) <= tol; }

std::chrono::system_clock::time_point utc(int y, int mo, int d, int h, int mi)
{
    using namespace std::chrono;
    return sys_days{year{y} / mo / d} + hours{h} + minutes{mi};
}

bool same(const CivilDate& a, int y, int m, int d) { return a.year == y && a.month == m && a.day == d; }

void sun_and_moon()
{
    // Berlin around solar noon on the solstices (NOAA: ~60.9 deg and ~14.0 deg).
    const auto summer = sun_position(52.52, 13.405, utc(2024, 6, 21, 11, 10));
    check(near(summer.elevation_deg, 60.9, 0.6), "Berlin summer solstice noon elevation " + std::to_string(summer.elevation_deg));
    check(near(summer.azimuth_deg, 180.0, 6.0), "Berlin noon azimuth is south " + std::to_string(summer.azimuth_deg));
    const auto winter = sun_position(52.52, 13.405, utc(2024, 12, 21, 11, 10));
    check(near(winter.elevation_deg, 14.0, 0.6), "Berlin winter solstice noon elevation " + std::to_string(winter.elevation_deg));
    check(sun_position(52.52, 13.405, utc(2024, 6, 21, 23, 0)).elevation_deg < -5.0, "Berlin midnight sun is down");
    // Sydney at local noon in its summer: high sun towards the north.
    const auto sydney = sun_position(-33.87, 151.21, utc(2024, 12, 21, 1, 55));
    check(sydney.elevation_deg > 75.0, "Sydney December noon is high " + std::to_string(sydney.elevation_deg));

    const double full = moon_phase(utc(2024, 1, 25, 17, 54));
    check(near(full, 0.5, 0.04), "full moon 2024-01-25 " + std::to_string(full));
    const double fresh = moon_phase(utc(2024, 1, 11, 11, 57));
    check(fresh < 0.04 || fresh > 0.96, "new moon 2024-01-11 " + std::to_string(fresh));

    check(season_for(7, 52.0) == Season::Summer, "July is summer in the north");
    check(season_for(7, -33.0) == Season::Winter, "July is winter in the south");
    check(season_for(1, 52.0) == Season::Winter, "January is winter in the north");
    check(season_for(4, 52.0) == Season::Spring, "April is spring in the north");
    check(season_for(10, 52.0) == Season::Autumn, "October is autumn in the north");
    check(season_for(2, 52.0) == Season::Winter && season_for(11, 52.0) == Season::Autumn, "February and November edges");
    // Leap day and the last days of May/August/November keep their season.
    for (const auto& [mo, d, want] : {std::tuple{2, 29, Season::Winter}, std::tuple{5, 31, Season::Spring},
                                      std::tuple{8, 31, Season::Summer}, std::tuple{11, 30, Season::Autumn}}) {
        const auto c = conditions_at(52.52, 13.405, utc(2028, mo, d, 11, 0), std::nullopt, 0);
        check(c.season == want, "2028-" + std::to_string(mo) + "-" + std::to_string(d) + " season in a leap year");
    }
}

void weather()
{
    check(weather_from_wmo(0).first == Weather::Clear, "wmo 0 clear");
    check(weather_from_wmo(2).first == Weather::PartlyCloudy, "wmo 2 partly cloudy");
    check(weather_from_wmo(3).first == Weather::Overcast, "wmo 3 overcast");
    check(weather_from_wmo(45).first == Weather::Fog, "wmo 45 fog");
    check(weather_from_wmo(53).first == Weather::Drizzle, "wmo 53 drizzle");
    check(weather_from_wmo(63).first == Weather::Rain, "wmo 63 rain");
    check(weather_from_wmo(73).first == Weather::Snow, "wmo 73 snow");
    check(weather_from_wmo(95).first == Weather::Storm, "wmo 95 storm");
    check(weather_from_wmo(65).second > weather_from_wmo(61).second, "heavy rain is more intense than light rain");

    const auto r = parse_weather(R"({"utc_offset_seconds":3600,"current":{"temperature_2m":-3.5,"weather_code":71,
        "cloud_cover":90,"wind_speed_10m":4.2,"wind_direction_10m":200,"snow_depth":0.12}})");
    check(r.has_value(), "parse a full weather response");
    if (r) {
        check(r->wmo_code == 71 && near(r->cloud_cover, 0.9, 1e-4) && near(r->temperature_c, -3.5, 1e-4), "parsed weather values");
        check(r->utc_offset_seconds == 3600 && near(r->snow_depth_m, 0.12, 1e-4), "parsed offset and snow depth");
    }
    check(!parse_weather(R"({"current":{}})").has_value(), "weather without a code is rejected");
    check(!parse_weather("not json").has_value(), "garbage is rejected");

    // A fresh report shows up in the conditions; local hour uses the offset.
    WeatherReport report;
    report.wmo_code = 61;
    report.utc_offset_seconds = 7200;
    const auto c = conditions_at(52.52, 13.405, utc(2024, 6, 21, 10, 0), report, 7200);
    check(c.sun_known && c.weather_known && c.weather == Weather::Rain, "conditions carry the real weather");
    check(near(c.local_hour, 12.0, 0.01), "local hour applies the UTC offset " + std::to_string(c.local_hour));
}

void occasions_registry()
{
    check(same(easter_sunday(2024), 2024, 3, 31), "Easter 2024");
    check(same(easter_sunday(2025), 2025, 4, 20), "Easter 2025");
    check(same(easter_sunday(2026), 2026, 4, 5), "Easter 2026");
    auto has = [](const std::vector<std::string>& v, const char* id) { return std::find(v.begin(), v.end(), id) != v.end(); };
    check(has(active_occasions({2026, 12, 24}), "christmas"), "Christmas Eve is Christmas");
    check(!has(active_occasions({2026, 11, 30}), "christmas"), "end of November is not Christmas yet");
    check(has(active_occasions({2026, 10, 31}), "halloween"), "Halloween");
    check(has(active_occasions({2027, 1, 1}), "new_years_eve"), "New Year's Day");
    check(has(active_occasions({2026, 4, 3}), "easter"), "Good Friday is in the Easter window");
    check(active_occasions({2026, 7, 14}).empty(), "a plain summer day has no occasion");
    for (const auto& o : occasions()) check(find_occasion(o.id) == &o, "find_occasion(" + o.id + ")");
}

void settings()
{
    Settings s;
    s.latitude = 100.0;
    s.longitude = 10.0;
    s.overrides.time_of_day = "noon";
    s.overrides.weather = "cloudy";
    s.overrides.occasions = "christmas,bogus,christmas,halloween";
    s.sanitize();
    check(!s.latitude && !s.longitude, "an out-of-range location is dropped");
    check(s.overrides.time_of_day == "auto", "an unknown time override falls back to auto");
    check(s.overrides.weather == "cloudy", "cloudy is a valid weather override");
    check(s.overrides.occasions == "christmas,halloween", "occasion list keeps known ids once: " + s.overrides.occasions);
    s.overrides.occasions = "bogus";
    s.sanitize();
    check(s.overrides.occasions == "off", "an empty occasion list means off");

    const nlohmann::json j = nlohmann::json::parse(R"({"enabled":false,"latitude":48.1,"longitude":11.6,"label":"Munich",
        "overrides":{"weather":"snow"}})");
    const auto parsed = j.get<Settings>();
    check(!parsed.enabled && parsed.has_manual_location() && parsed.label == "Munich", "settings parse");
    check(parsed.overrides.weather == "snow" && parsed.overrides.time_of_day == "auto", "missing override keys default to auto");
    const Settings round = nlohmann::json(parsed).get<Settings>();
    check(round.latitude == parsed.latitude && round.overrides.weather == "snow", "settings round-trip");
}

void layering()
{
    const Defaults d{TimeOfDay::Dusk, Weather::Clear, Season::Summer};
    const Adapt all;
    Conditions c;  // nothing known: the designed look
    auto look = target_look(c, d, all, TimeChoice::AUTO, WeatherChoice::AUTO, SeasonChoice::AUTO);
    check(look.time == TimeOfDay::Dusk && look.weather == Weather::Clear && look.season == Season::Summer, "defaults apply");

    // Real world.
    c.sun_known = c.weather_known = c.season_known = true;
    c.sun.elevation_deg = 35.0;
    c.sun.azimuth_deg = 180.0;
    c.weather = Weather::Rain;
    c.weather_intensity = 0.6f;
    c.season = Season::Autumn;
    look = target_look(c, d, all, TimeChoice::AUTO, WeatherChoice::AUTO, SeasonChoice::AUTO);
    check(look.time == TimeOfDay::Day && look.daylight > 0.95f, "real sun high up is day");
    check(look.weather == Weather::Rain && look.rain > 0.2f && look.season == Season::Autumn, "real weather and season apply");
    check(near(look.sun_x, 0.5, 0.01), "a south sun is in the middle of the view");

    // Global override beats the real world.
    c.forced_weather = Weather::Snow;
    c.forced_time = TimeOfDay::Night;
    look = target_look(c, d, all, TimeChoice::AUTO, WeatherChoice::AUTO, SeasonChoice::AUTO);
    check(look.weather == Weather::Snow && look.snow_cover > 0.9f, "global snow override");
    check(look.time == TimeOfDay::Night && look.night > 0.9f && look.lamp() > 0.9f, "global night override");

    // The scene property beats the global override.
    look = target_look(c, d, all, TimeChoice::DAY, WeatherChoice::FOG, SeasonChoice::SPRING);
    check(look.time == TimeOfDay::Day && look.weather == Weather::Fog && look.season == Season::Spring, "scene properties win");

    // An aspect the scene does not adapt keeps its default, whatever is forced.
    Adapt fixed;
    fixed.time = false;
    fixed.weather_allowed = {Weather::Clear, Weather::Rain};
    look = target_look(c, d, fixed, TimeChoice::AUTO, WeatherChoice::AUTO, SeasonChoice::AUTO);
    check(look.time == TimeOfDay::Dusk, "non-adapted time keeps the default");
    check(look.weather == Weather::Clear, "a disallowed weather falls back to the default");

    // Chosen winter shows snow on the ground; occasions pass through.
    c = {};
    c.occasions = {"christmas"};
    look = target_look(c, d, all, TimeChoice::AUTO, WeatherChoice::AUTO, SeasonChoice::WINTER);
    check(look.snow_cover > 0.5f && look.winter > 0.99f, "chosen winter has snow lying");
    check(look.occasion("christmas") && !look.occasion("easter"), "occasions reach the look");
}
}  // namespace

int main()
{
    sun_and_moon();
    weather();
    occasions_registry();
    settings();
    layering();
    if (failures == 0)
        std::cout << "environment_smoke: all checks passed\n";
    return failures == 0 ? 0 : 1;
}
