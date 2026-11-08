#include <shared/matrix/environment/look.h>

#include <algorithm>
#include <cmath>

namespace Environment {
namespace {
float clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }
float smooth(float a, float b, float x)
{
    const float t = clamp01((x - a) / (b - a));
    return t * t * (3.0f - 2.0f * t);
}

struct SkyKey {
    float elevation;
    Rgb zenith, horizon, ambient, sun;
};
// Sky and light by sun elevation, clear weather. Tuned on the LED panel:
// night keeps enough blue to read silhouettes, twilight is rose then gold.
constexpr std::array<SkyKey, 7> kSky{{
    {-18.0f, {0.010f, 0.016f, 0.055f}, {0.035f, 0.050f, 0.130f}, {0.17f, 0.20f, 0.36f}, {0.0f, 0.0f, 0.0f}},
    {-9.0f, {0.030f, 0.045f, 0.160f}, {0.200f, 0.150f, 0.330f}, {0.24f, 0.24f, 0.42f}, {0.6f, 0.3f, 0.3f}},
    {-3.0f, {0.110f, 0.140f, 0.380f}, {0.900f, 0.420f, 0.380f}, {0.52f, 0.40f, 0.50f}, {1.0f, 0.45f, 0.25f}},
    {2.0f, {0.210f, 0.300f, 0.560f}, {1.000f, 0.620f, 0.380f}, {0.92f, 0.66f, 0.52f}, {1.0f, 0.62f, 0.30f}},
    {8.0f, {0.250f, 0.420f, 0.760f}, {0.960f, 0.800f, 0.640f}, {1.00f, 0.86f, 0.74f}, {1.0f, 0.86f, 0.62f}},
    {20.0f, {0.230f, 0.470f, 0.860f}, {0.720f, 0.830f, 0.950f}, {1.03f, 1.00f, 0.94f}, {1.0f, 0.96f, 0.86f}},
    {60.0f, {0.180f, 0.440f, 0.880f}, {0.640f, 0.800f, 0.960f}, {1.05f, 1.04f, 1.00f}, {1.0f, 0.98f, 0.92f}},
}};

SkyKey sky_at(float e)
{
    if (e <= kSky.front().elevation)
        return kSky.front();
    for (std::size_t i = 1; i < kSky.size(); ++i)
        if (e <= kSky[i].elevation) {
            const auto &a = kSky[i - 1], &b = kSky[i];
            const float t = smooth(a.elevation, b.elevation, e);
            return {e, mix(a.zenith, b.zenith, t), mix(a.horizon, b.horizon, t), mix(a.ambient, b.ambient, t),
                    mix(a.sun, b.sun, t)};
        }
    return kSky.back();
}

Rgb desaturate(Rgb c, float amount)
{
    const float l = luminance(c);
    return mix(c, {l, l, l}, amount);
}

/// Synthetic sun for a fixed time of day.
void fixed_time(Look &l, TimeOfDay t)
{
    switch (t) {
    case TimeOfDay::Dawn:
        l.sun_elevation = 2.5f;
        l.sun_x = 0.24f;
        l.morning = true;
        break;
    case TimeOfDay::Day:
        l.sun_elevation = 38.0f;
        l.sun_x = 0.62f;
        l.morning = false;
        break;
    case TimeOfDay::Dusk:
        l.sun_elevation = 2.0f;
        l.sun_x = 0.76f;
        l.morning = false;
        break;
    case TimeOfDay::Night:
        l.sun_elevation = -24.0f;
        l.sun_x = 0.5f;
        l.morning = false;
        break;
    }
    l.moon_phase = 0.42f;  // a pleasing waxing gibbous
}

void apply_weather(Look &l, Weather w, float intensity)
{
    l.weather = w;
    l.clouds = 0.12f;
    l.overcast = l.rain = l.snow = l.fog = l.storm = 0.0f;
    switch (w) {
    case Weather::Clear: l.clouds = 0.12f + 0.2f * intensity; break;
    case Weather::PartlyCloudy: l.clouds = 0.5f; l.overcast = 0.1f; break;
    case Weather::Overcast: l.clouds = 0.85f; l.overcast = 0.72f; break;
    case Weather::Fog: l.clouds = 0.6f; l.overcast = 0.55f; l.fog = 0.55f + 0.4f * intensity; break;
    case Weather::Drizzle: l.clouds = 0.85f; l.overcast = 0.65f; l.rain = 0.18f + 0.3f * intensity; break;
    case Weather::Rain: l.clouds = 0.9f; l.overcast = 0.78f; l.rain = 0.35f + 0.6f * intensity; break;
    case Weather::Snow: l.clouds = 0.85f; l.overcast = 0.6f; l.snow = 0.35f + 0.6f * intensity; break;
    case Weather::Storm:
        l.clouds = 1.0f;
        l.overcast = 0.92f;
        l.rain = 0.85f + 0.15f * intensity;
        l.storm = 0.7f + 0.3f * intensity;
        break;
    }
}

Weather weather_from_choice(WeatherChoice c)
{
    switch (c) {
    case WeatherChoice::CLOUDY: return Weather::Overcast;
    case WeatherChoice::RAIN: return Weather::Rain;
    case WeatherChoice::SNOW: return Weather::Snow;
    case WeatherChoice::FOG: return Weather::Fog;
    case WeatherChoice::STORM: return Weather::Storm;
    default: return Weather::Clear;
    }
}

float choice_intensity(Weather w)
{
    switch (w) {
    case Weather::Rain: return 0.6f;
    case Weather::Snow: return 0.6f;
    case Weather::Fog: return 0.7f;
    case Weather::Storm: return 0.9f;
    default: return 0.0f;
    }
}

bool allowed(const Adapt &a, Weather w)
{
    if (a.weather_allowed.empty())
        return true;
    // Drizzle counts as rain, partly cloudy as clear for allow-lists.
    const Weather family = w == Weather::Drizzle ? Weather::Rain : w == Weather::PartlyCloudy ? Weather::Clear : w;
    return std::find(a.weather_allowed.begin(), a.weather_allowed.end(), family) != a.weather_allowed.end() ||
           std::find(a.weather_allowed.begin(), a.weather_allowed.end(), w) != a.weather_allowed.end();
}

void set_season(Look &l, Season s)
{
    l.season = s;
    l.spring = s == Season::Spring;
    l.summer = s == Season::Summer;
    l.autumn = s == Season::Autumn;
    l.winter = s == Season::Winter;
}

template <typename E>
std::shared_ptr<Plugins::Property<Plugins::EnumProperty<E>>> enum_prop(const char *name, E value)
{
    return MAKE_ENUM_PROPERTY(name, E, value);
}

float ease(float current, float target, float k) { return current + (target - current) * k; }
Rgb ease(Rgb a, Rgb b, float k) { return mix(a, b, k); }
} // namespace

Rgb mix(Rgb a, Rgb b, float t)
{
    t = clamp01(t);
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}

float luminance(Rgb c) { return 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b; }

bool Look::occasion(std::string_view id) const
{
    return std::find(occasions.begin(), occasions.end(), id) != occasions.end();
}

Rgb Look::sky(float v) const
{
    v = clamp01(v);
    return mix(zenith, horizon, v * v * (3.0f - 2.0f * v) * 0.6f + v * 0.4f);
}

Rgb Look::lit(Rgb albedo, float depth) const
{
    Rgb c = albedo * ambient;
    c = desaturate(c, 0.30f * night + 0.25f * overcast);
    const float d = clamp01(depth);
    const float amount = clamp01(d * (0.22f + 0.12f * overcast) + fog * (0.12f + 0.75f * d));
    c = mix(c, haze, amount);
    if (flash > 0.0f)
        c = c + Rgb{0.55f, 0.58f, 0.70f} * (flash * (0.35f + 0.25f * (1.0f - depth)));
    return c;
}

Rgb Look::foliage(Rgb c, float strength) const
{
    const float l = luminance(c);
    const Rgb springC{c.r * 1.06f + 0.03f, c.g * 1.14f + 0.03f, c.b * 0.88f};
    // Autumn: warm ochre/rust, keeping some of the original so it reads as leaves.
    const Rgb autumnC = mix(c, Rgb{std::min(1.0f, l * 1.95f + 0.04f), l * 1.05f + 0.02f, l * 0.40f}, 0.85f);
    const Rgb winterC{l * 0.92f + 0.05f, l * 0.84f + 0.04f, l * 0.76f + 0.04f};
    const Rgb seasonal = springC * spring + c * summer + autumnC * autumn + winterC * winter;
    return mix(c, seasonal, strength);
}

Rgb Look::grass(Rgb c) const
{
    // Meadows only yellow a little in autumn and go dull in winter.
    const float l = luminance(c);
    const Rgb autumnC{c.r * 0.75f + l * 0.55f, c.g * 0.80f + l * 0.25f, c.b * 0.6f};
    const Rgb winterC{l * 0.85f + 0.04f, l * 0.88f + 0.03f, l * 0.70f + 0.03f};
    const Rgb springC{c.r * 1.02f + 0.02f, c.g * 1.12f + 0.03f, c.b * 0.9f};
    return springC * spring + c * summer + autumnC * autumn + winterC * winter;
}

Rgb Look::ground(Rgb albedo, float exposure) const
{
    return mix(albedo, {0.90f, 0.93f, 0.98f}, snow_cover * clamp01(exposure));
}

void derive_palette(Look &l)
{
    const float e = l.sun_elevation;
    const SkyKey k = sky_at(e);
    l.twilight = std::exp(-((e - 1.0f) / 6.5f) * ((e - 1.0f) / 6.5f));
    l.night = smooth(1.0f, -10.0f, e);
    l.daylight = smooth(-6.0f, 14.0f, e);
    l.moon_light = l.night * (0.5f - 0.5f * std::cos(l.moon_phase * 6.2831853f));

    // Weather greys the sky and dims the light; storms most of all.
    const float grey = clamp01(l.overcast * 0.9f + l.fog * 0.35f);
    const float dim = 1.0f - 0.32f * l.overcast - 0.22f * l.storm - 0.08f * l.rain;
    const float zl = luminance(k.zenith), hl = luminance(k.horizon);
    const Rgb greyZ = Rgb{0.86f, 0.90f, 0.98f} * (zl * 0.8f + 0.02f);
    const Rgb greyH = Rgb{0.92f, 0.93f, 0.97f} * (hl * 0.78f + 0.02f);
    // Mornings are cooler and pinker than the golden evening.
    const float dawn = l.morning ? l.twilight : 0.0f;
    const Rgb horizonClear = mix(k.horizon, Rgb{0.98f, 0.70f, 0.66f}, 0.45f * dawn);
    const Rgb zenithClear = mix(k.zenith, Rgb{0.24f, 0.34f, 0.62f}, 0.35f * dawn);
    l.zenith = mix(zenithClear, greyZ, grey) * dim;
    l.horizon = mix(horizonClear, greyH, grey) * dim;
    l.ambient = desaturate(mix(k.ambient, k.ambient * Rgb{0.95f, 0.92f, 1.04f}, dawn), 0.35f * l.overcast) * dim;
    // Moonlight lifts the night a touch.
    l.ambient = l.ambient + Rgb{0.04f, 0.05f, 0.09f} * l.moon_light;
    // Winter light is a little colder.
    l.ambient = mix(l.ambient, l.ambient * Rgb{0.94f, 0.97f, 1.06f}, l.winter);
    l.sun_color = k.sun;
    l.haze = mix(l.horizon, mix(l.horizon, Rgb{0.7f, 0.72f, 0.76f} * (0.15f + 0.85f * l.daylight), 0.6f), l.fog);
    l.lights = clamp01(smooth(6.0f, -5.0f, e) + 0.35f * l.storm + 0.15f * l.overcast * smooth(15.0f, 2.0f, e));
    l.wetness = clamp01(l.rain * 1.2f);
}

Look target_look(const Conditions &c, const Defaults &d, const Adapt &a, TimeChoice tc, WeatherChoice wc,
                 SeasonChoice sc)
{
    Look l;
    // ---- time of day ----
    if (a.time && tc != TimeChoice::AUTO) {
        const TimeOfDay t = static_cast<TimeOfDay>(static_cast<int>(tc) - 1);
        fixed_time(l, t);
        l.time = t;
    } else if (a.time && c.forced_time) {
        fixed_time(l, *c.forced_time);
        l.time = *c.forced_time;
    } else if (a.time && c.sun_known) {
        l.sun_elevation = static_cast<float>(c.sun.elevation_deg);
        l.morning = c.morning;
        // Facing south (north in the southern hemisphere): the sun travels
        // from the left (east) to the right (west) across the view.
        double az = c.sun.azimuth_deg;
        if (c.latitude < 0.0)
            az = std::fmod(540.0 - az, 360.0);
        l.sun_x = std::clamp(static_cast<float>((az - 90.0) / 180.0), -0.2f, 1.2f);
        l.moon_phase = static_cast<float>(c.moon_phase);
        l.time = time_of_day_for(c.sun.elevation_deg, c.morning);
    } else {
        fixed_time(l, d.time);
        l.time = d.time;
        if (d.sun_elevation)
            l.sun_elevation = *d.sun_elevation;
        if (d.sun_x) {
            l.sun_x = *d.sun_x;
            l.morning = *d.sun_x < 0.5f;
        }
    }

    // ---- season (before weather: winter without a report has snow lying) ----
    Season season = d.season;
    if (a.season && sc != SeasonChoice::AUTO)
        season = static_cast<Season>(static_cast<int>(sc) - 1);
    else if (a.season && c.forced_season)
        season = *c.forced_season;
    else if (a.season && c.season_known)
        season = c.season;
    set_season(l, season);

    // ---- weather ----
    bool real_weather = false;
    Weather w = d.weather;
    float intensity = choice_intensity(d.weather);
    if (a.weather && wc != WeatherChoice::AUTO) {
        w = weather_from_choice(wc);
        intensity = choice_intensity(w);
    } else if (a.weather && c.forced_weather) {
        w = *c.forced_weather;
        intensity = choice_intensity(w);
    } else if (a.weather && c.weather_known) {
        w = c.weather;
        intensity = c.weather_intensity;
        real_weather = true;
    }
    if (!allowed(a, w)) {
        w = d.weather;
        intensity = choice_intensity(w);
        real_weather = false;
    }
    apply_weather(l, w, intensity);
    // A little morning mist in the valleys around sunrise.
    if (l.morning && l.sun_elevation > -6.0f && l.sun_elevation < 8.0f)
        l.fog = std::max(l.fog, 0.18f);
    if (real_weather) {
        l.clouds = std::max(l.clouds * 0.5f, c.cloud_cover);
        l.temperature = c.temperature_c;
        const float rad = c.wind_direction_deg * 0.0174533f;
        l.wind = std::clamp(-std::sin(rad) * c.wind_speed_ms / 10.0f, -1.0f, 1.0f);
        // Snow lying on the ground follows the reported depth.
        l.snow_cover = clamp01(c.snow_depth_m / 0.03f);
        if (w == Weather::Snow)
            l.snow_cover = std::max(l.snow_cover, 0.55f);
    } else {
        l.wind = w == Weather::Storm ? 0.8f : w == Weather::Rain ? 0.35f : 0.2f;
        l.temperature = season == Season::Winter ? -2.0f : season == Season::Summer ? 22.0f : 12.0f;
        // A chosen snowy look (or a chosen winter) shows snow on the ground.
        if (w == Weather::Snow)
            l.snow_cover = 1.0f;
        else if (season == Season::Winter && w != Weather::Rain && w != Weather::Storm)
            l.snow_cover = 0.75f;
    }

    l.occasions = c.occasions;
    derive_palette(l);
    return l;
}

SceneEnvironment::SceneEnvironment(Defaults defaults, Adapt adapt)
    : defaults_(defaults), adapt_(std::move(adapt)),
      time_(enum_prop("time_of_day", TimeChoice::AUTO)),
      weather_(enum_prop("weather", WeatherChoice::AUTO)),
      season_(enum_prop("season", SeasonChoice::AUTO))
{
    time_->label("Time of day")
        .description("Auto follows the real sun (or the global override); pick one to pin this scene.")
        .group("Environment");
    weather_->label("Weather")
        .description("Auto follows the real weather (or the global override); pick one to pin this scene.")
        .group("Environment");
    season_->label("Season")
        .description("Auto follows the calendar (or the global override); pick one to pin this scene.")
        .group("Environment");
}

std::vector<std::shared_ptr<Plugins::PropertyBase>> SceneEnvironment::properties() const
{
    std::vector<std::shared_ptr<Plugins::PropertyBase>> out;
    if (adapt_.time)
        out.push_back(time_);
    if (adapt_.weather)
        out.push_back(weather_);
    if (adapt_.season)
        out.push_back(season_);
    return out;
}

const Look &SceneEnvironment::update(float dt)
{
    dt = std::clamp(dt, 0.0f, 0.25f);
    poll_ -= dt;
    if (!snapped_ || poll_ <= 0.0f) {
        conditions_ = current();
        poll_ = 1.0f;
    }
    // Properties of aspects the scene does not adapt are never registered.
    const Look target = target_look(conditions_, defaults_, adapt_, adapt_.time ? time_->get().get() : TimeChoice::AUTO,
                                    adapt_.weather ? weather_->get().get() : WeatherChoice::AUTO,
                                    adapt_.season ? season_->get().get() : SeasonChoice::AUTO);
    if (!snapped_) {
        look_ = target;
        snapped_ = true;
    } else {
        // ~3 s cross-fade for every continuous value.
        const float k = 1.0f - std::exp(-dt / 3.0f);
        look_.sun_elevation = ease(look_.sun_elevation, target.sun_elevation, k);
        look_.sun_x = ease(look_.sun_x, target.sun_x, k);
        look_.moon_phase = target.moon_phase;
        look_.morning = target.morning;
        look_.clouds = ease(look_.clouds, target.clouds, k);
        look_.overcast = ease(look_.overcast, target.overcast, k);
        look_.rain = ease(look_.rain, target.rain, k);
        look_.snow = ease(look_.snow, target.snow, k);
        look_.fog = ease(look_.fog, target.fog, k);
        look_.storm = ease(look_.storm, target.storm, k);
        look_.wind = ease(look_.wind, target.wind, k);
        // Lying snow builds and melts slowly.
        look_.snow_cover = ease(look_.snow_cover, target.snow_cover, 1.0f - std::exp(-dt / 8.0f));
        look_.temperature = target.temperature;
        look_.spring = ease(look_.spring, target.spring, k);
        look_.summer = ease(look_.summer, target.summer, k);
        look_.autumn = ease(look_.autumn, target.autumn, k);
        look_.winter = ease(look_.winter, target.winter, k);
        look_.time = target.time;
        look_.weather = target.weather;
        look_.season = target.season;
        look_.occasions = target.occasions;
        derive_palette(look_);
    }

    // Lightning: rare strikes while stormy, each a quick double flicker.
    look_.bolt = false;
    look_.flash = std::max(0.0f, look_.flash - dt * 3.2f);
    if (look_.storm > 0.2f) {
        flash_timer_ -= dt;
        if (flash_timer_ <= 0.0f) {
            look_.flash = 0.9f;
            look_.bolt = true;
            std::uniform_real_distribution<float> u(0.0f, 1.0f);
            flash_timer_ = (3.5f + 7.0f * u(rng_)) / std::max(0.4f, look_.storm);
            flicker_ = 0.16f;
        }
    }
    if (flicker_ > 0.0f) {
        flicker_ -= dt;
        if (flicker_ <= 0.0f)
            look_.flash = std::max(look_.flash, 0.7f);  // second flicker of the same strike
    }
    return look_;
}

void SceneEnvironment::add_variants(Scenes::SceneDescriptor &d, std::vector<Scenes::SceneVariant> extra) const
{
    auto variant = [&](std::string id, std::string label, std::string description, nlohmann::json props,
                       std::vector<std::string> tags) {
        Scenes::SceneVariant v;
        v.id = std::move(id);
        v.label = std::move(label);
        v.description = std::move(description);
        v.properties = std::move(props);
        v.tags = std::move(tags);
        v.automatic_default = false;  // Automatic Mode follows the real world instead
        d.variants.push_back(std::move(v));
    };
    if (adapt_.time) {
        static constexpr std::array<std::pair<TimeOfDay, const char *>, 4> times{{
            {TimeOfDay::Dawn, "DAWN"}, {TimeOfDay::Day, "DAY"}, {TimeOfDay::Dusk, "DUSK"}, {TimeOfDay::Night, "NIGHT"}}};
        for (const auto &[t, value] : times) {
            if (t == defaults_.time)
                continue;
            const std::string id = to_string(t);
            std::string label = id;
            label[0] = static_cast<char>(std::toupper(label[0]));
            variant(id, label, "Always " + id + ", whatever the real time.", {{"time_of_day", value}}, {"time:" + id});
        }
    }
    if (adapt_.weather) {
        static constexpr std::array<std::tuple<Weather, const char *, const char *>, 4> weathers{{
            {Weather::Rain, "rain", "RAIN"}, {Weather::Snow, "snow", "SNOW"}, {Weather::Fog, "fog", "FOG"},
            {Weather::Storm, "storm", "STORM"}}};
        for (const auto &[w, id, value] : weathers) {
            if (w == defaults_.weather || !allowed(adapt_, w))
                continue;
            std::string label = id;
            label[0] = static_cast<char>(std::toupper(label[0]));
            variant(id, label, std::string("Always ") + id + ", whatever the real weather.", {{"weather", value}},
                    {std::string("weather:") + id});
        }
    }
    if (adapt_.season) {
        static constexpr std::array<std::tuple<Season, const char *, const char *>, 4> seasons{{
            {Season::Spring, "spring", "SPRING"}, {Season::Summer, "summer", "SUMMER"},
            {Season::Autumn, "autumn", "AUTUMN"}, {Season::Winter, "winter", "WINTER"}}};
        for (const auto &[season, id, value] : seasons) {
            if (season == defaults_.season)
                continue;
            std::string label = id;
            label[0] = static_cast<char>(std::toupper(label[0]));
            variant(id, label, std::string("Always ") + id + ", whatever the calendar says.", {{"season", value}},
                    {std::string("season:") + id});
        }
    }
    for (auto &v : extra) {
        v.automatic_default = false;
        d.variants.push_back(std::move(v));
    }
    d.tags.emplace_back("environment-adaptive");
}

} // namespace Environment
