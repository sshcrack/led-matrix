#pragma once

#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace Environment {

/// Global "make it so" choices. Each one replaces the real world (or a
/// scene's designed look) for every scene left on "auto"; a scene's own fixed
/// property still wins. Values are the lowercase ids used in the API:
///   time_of_day: auto | dawn | day | dusk | night
///   weather:     auto | clear | cloudy | rain | snow | fog | storm
///   season:      auto | spring | summer | autumn | winter
struct Overrides {
    std::string time_of_day = "auto";
    std::string weather = "auto";
    std::string season = "auto";
    /// Occasions (see occasions.h): "auto" follows the calendar, "off" never
    /// shows any, otherwise a comma-separated list of ids to force on.
    std::string occasions = "auto";

    [[nodiscard]] bool any() const
    {
        return time_of_day != "auto" || weather != "auto" || season != "auto" || occasions != "auto";
    }
};

/// Persisted in config.json under "environment".
struct Settings {
    /// Follow the real time of day, weather and season at the location.
    bool enabled = true;
    /// Locate the matrix by its public IP when no manual location is set.
    bool auto_locate = true;
    std::optional<double> latitude, longitude;  // manual location
    std::string label;                          // manual location name
    // Last IP-detected location, cached so a reboot does not need a lookup.
    std::optional<double> detected_latitude, detected_longitude;
    std::string detected_label;
    Overrides overrides;

    [[nodiscard]] bool has_manual_location() const { return latitude.has_value() && longitude.has_value(); }
    /// Clamps coordinates and replaces unknown override ids with "auto".
    void sanitize();
};

void to_json(nlohmann::json &j, const Overrides &o);
void from_json(const nlohmann::json &j, Overrides &o);
void to_json(nlohmann::json &j, const Settings &s);
void from_json(const nlohmann::json &j, Settings &s);

} // namespace Environment
