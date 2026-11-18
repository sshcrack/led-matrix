#include "shared/matrix/halo/settings.h"

#include <algorithm>
#include <cctype>

#include "shared/matrix/halo/protocol.h"

namespace Halo {

namespace {
int hex_digit(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1;
}
}  // namespace

bool is_allowed_device(const std::string& path)
{
    static constexpr const char* kPrefixes[] = {"/dev/serial", "/dev/ttyAMA", "/dev/ttyS", "/dev/ttyUSB", "/dev/ttyACM", "/dev/pts/"};
    for (const char* prefix : kPrefixes) {
        const std::string p(prefix);
        if (path.size() <= p.size() || path.compare(0, p.size(), p) != 0 || path.size() > p.size() + 3)
            continue;
        return std::all_of(path.begin() + static_cast<std::ptrdiff_t>(p.size()), path.end(), [](char c) { return c >= '0' && c <= '9'; });
    }
    return false;
}

bool parse_color(const std::string& text, unsigned char& r, unsigned char& g, unsigned char& b)
{
    if (text.size() != 7 || text[0] != '#')
        return false;
    int v[6];
    for (int i = 0; i < 6; ++i)
        if ((v[i] = hex_digit(text[1 + i])) < 0)
            return false;
    r = static_cast<unsigned char>(v[0] * 16 + v[1]);
    g = static_cast<unsigned char>(v[2] * 16 + v[3]);
    b = static_cast<unsigned char>(v[4] * 16 + v[5]);
    return true;
}

void Settings::sanitize()
{
    const Settings defaults;
    if (mode != "fixed" && mode != "match")
        mode = defaults.mode;
    unsigned char r, g, b;
    if (!parse_color(color, r, g, b))
        color = defaults.color;
    brightness = std::clamp(brightness, 0, 100);
    led_count = std::clamp(led_count, 1, static_cast<int>(kMaxLeds));
    rotation = std::clamp(rotation, 0, 7);
    sample_columns = std::clamp(sample_columns, 1, kMaxSampleColumns);
    sample_depth = std::clamp(sample_depth, kOuterSampleDepth + 1, kMaxSampleDepth);
    if (!is_allowed_device(device))
        device = defaults.device;
}

std::string Settings::validate() const
{
    unsigned char r, g, b;
    if (mode != "fixed" && mode != "match")
        return "mode must be 'fixed' or 'match'";
    if (!parse_color(color, r, g, b))
        return "color must look like #rrggbb";
    if (brightness < 0 || brightness > 100)
        return "brightness must be 0..100";
    if (led_count < 1 || led_count > kMaxLeds)
        return "led_count must be 1..200";
    if (rotation < 0 || rotation > 7)
        return "rotation must be 0..7";
    if (sample_columns < 1 || sample_columns > kMaxSampleColumns)
        return "sample_columns must be 1..16";
    if (sample_depth <= kOuterSampleDepth || sample_depth > kMaxSampleDepth)
        return "sample_depth must be 3..24";
    if (!is_allowed_device(device))
        return "device must be a serial port such as /dev/serial0, /dev/ttyAMA0, /dev/ttyS0 or /dev/ttyUSB0";
    return {};
}

void to_json(nlohmann::json& j, const Settings& s)
{
    j = nlohmann::json{
        {"enabled", s.enabled},
        {"mode", s.mode},
        {"brightness", s.brightness},
        {"color", s.color},
        {"led_count", s.led_count},
        {"device", s.device},
        {"rotation", s.rotation},
        {"reverse", s.reverse},
        {"start_between_zones", s.start_between_zones},
        {"sample_columns", s.sample_columns},
        {"sample_depth", s.sample_depth},
    };
}

void from_json(const nlohmann::json& j, Settings& s)
{
    s = Settings{};
    if (!j.is_object())
        return;
    const auto get = [&](const char* key, auto& out) {
        if (!j.contains(key))
            return;
        try {
            out = j.at(key).get<std::decay_t<decltype(out)>>();
        }
        catch (const nlohmann::json::exception&) {
        }
    };
    get("enabled", s.enabled);
    get("mode", s.mode);
    get("brightness", s.brightness);
    get("color", s.color);
    get("led_count", s.led_count);
    get("device", s.device);
    get("rotation", s.rotation);
    get("reverse", s.reverse);
    get("start_between_zones", s.start_between_zones);
    get("sample_columns", s.sample_columns);
    get("sample_depth", s.sample_depth);
    s.sanitize();
}

}  // namespace Halo
