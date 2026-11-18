#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace Halo {

/// Persisted in config.json under "halo".
struct Settings {
    /// Off by default: machines without the Pro Micro are untouched.
    bool enabled = false;
    /// "fixed" (one colour) or "match" (follow the display's edge colours).
    std::string mode = "match";
    /// Halo brightness 0..100 %, independent of the matrix brightness.
    int brightness = 50;
    /// Colour of the "fixed" mode, "#rrggbb".
    std::string color = "#ffb070";
    /// LEDs on the strip (sent to the firmware, 1..200).
    int led_count = 40;
    /// Serial device wired to the Pro Micro (is_allowed_device()).
    std::string device = "/dev/serial0";
    /// Shift the 8 edge zones clockwise by this many steps (0..7), and/or
    /// reverse their direction, to fit where the strip starts and which way it
    /// runs. Zone 0 is the top-left half of the top edge, seen from the front.
    int rotation = 0;
    bool reverse = false;

    /// Clamps numbers and replaces unknown mode/colour with the defaults.
    void sanitize();
    /// Strict check used by the REST route: empty when valid, else the reason.
    [[nodiscard]] std::string validate() const;
};

/// True for serial device names the daemon may open: /dev/serial<N>, /dev/ttyAMA<N>,
/// /dev/ttyS<N>, /dev/ttyUSB<N>, /dev/ttyACM<N> and /dev/pts/<N> (tests). The device is
/// opened as root before the privilege drop, so it must never name an arbitrary file.
bool is_allowed_device(const std::string& path);

/// Parses "#rrggbb" (case-insensitive). False when malformed.
bool parse_color(const std::string& text, unsigned char& r, unsigned char& g, unsigned char& b);

void to_json(nlohmann::json& j, const Settings& s);
/// Lenient: missing or wrongly typed keys keep their defaults; the result is
/// sanitized.
void from_json(const nlohmann::json& j, Settings& s);

}  // namespace Halo
