#include "shared/matrix/config/data.h"

#include <iostream>

namespace {
    int failures = 0;

    void expect(const char *name, const int actual, const int expected) {
        if (actual != expected) {
            std::cerr << "FAIL: " << name << " got " << actual << " expected " << expected << "\n";
            ++failures;
        }
    }

    int at(const ConfigData::BrightnessSchedule &s, int hour, int minute) {
        return s.brightness_at(hour * 60 + minute);
    }
}

int main() {
    ConfigData::BrightnessSchedule s;
    s.enabled = true;
    s.day_brightness = 100;
    s.night_brightness = 20;
    s.night_start_hour = 22;
    s.night_end_hour = 7;
    s.fade_minutes = 20;

    expect("midday is day", at(s, 12, 0), 100);
    expect("night start begins fade", at(s, 22, 0), 100);
    expect("half way through dusk fade", at(s, 22, 10), 60);
    expect("after dusk fade", at(s, 22, 20), 20);
    expect("past midnight is night", at(s, 3, 0), 20);
    expect("dawn fade starts at night value", at(s, 7, 0), 20);
    expect("half way through dawn fade", at(s, 7, 10), 60);
    expect("after dawn fade", at(s, 7, 30), 100);
    expect("minute wraps", s.brightness_at(24 * 60 + 12 * 60), 100);

    auto no_fade = s;
    no_fade.fade_minutes = 0;
    expect("no fade switches immediately", at(no_fade, 22, 0), 20);
    expect("no fade dawn", at(no_fade, 7, 0), 100);

    auto same_day = s;
    same_day.night_start_hour = 13;
    same_day.night_end_hour = 15;
    same_day.fade_minutes = 0;
    expect("non-wrapping window inside", at(same_day, 14, 0), 20);
    expect("non-wrapping window outside", at(same_day, 16, 0), 100);

    auto empty_window = s;
    empty_window.night_end_hour = 22;
    expect("empty window is always day", at(empty_window, 23, 0), 100);

    auto long_fade = s;
    long_fade.night_start_hour = 22;
    long_fade.night_end_hour = 22;
    long_fade.night_end_minute = 10;
    long_fade.fade_minutes = 60;
    expect("fade clamped to short night window", at(long_fade, 22, 5), 60);
    expect("dawn fade continues from night value", at(long_fade, 22, 10), 20);
    expect("dawn fade progresses", at(long_fade, 22, 40), 60);

    ConfigData::BrightnessSchedule wild;
    wild.day_brightness = 500;
    wild.night_brightness = -3;
    wild.fade_minutes = 9999;
    wild.sanitize();
    expect("sanitize day", wild.day_brightness, 100);
    expect("sanitize night", wild.night_brightness, 1);
    expect("sanitize fade", wild.fade_minutes, 180);

    nlohmann::json j = s;
    const auto round_trip = j.get<ConfigData::BrightnessSchedule>();
    expect("json round trip night", round_trip.night_brightness, 20);
    expect("json round trip enabled", round_trip.enabled ? 1 : 0, 1);
    const auto partial = nlohmann::json{{"enabled", true}}.get<ConfigData::BrightnessSchedule>();
    expect("json defaults", partial.night_brightness, ConfigData::BrightnessSchedule{}.night_brightness);

    if (failures == 0) std::cout << "brightness_schedule_smoke: all checks passed\n";
    return failures == 0 ? 0 : 1;
}
