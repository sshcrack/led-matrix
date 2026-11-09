#pragma once

// Scene side of the environment system.
//
// A scenery scene owns one SceneEnvironment, adds its properties in
// register_properties() and calls update() once per frame. The result is a
// Look: smoothed numbers (daylight, rain, snow cover, season weights, ...) and
// a palette (sky, ambient light, fog colour) the scene paints with.
//
// Every aspect (time of day, weather, season) resolves independently:
//
//     scene property (fixed)  >  global override  >  real world  >  scene default
//
// so a user can pin one scene to "night", make it snow everywhere because it
// is winter, or turn real-world sync off and get the designed looks back.
// Occasions (holidays, occasions.h) are listed in Look::occasions; a scene
// decorates for the ids it knows and ignores the rest.
//
// Painting helpers that work on any float RGB buffer live in paint.h.

#include <array>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include <shared/matrix/environment/environment.h>
#include <shared/matrix/plugin/property.h>
#include <shared/matrix/plugin/PropertyMacros.h>
#include <shared/matrix/scene_descriptor.h>

namespace Environment {

// Per-scene property values (UPPER_CASE like every other enum property).
enum class TimeChoice { AUTO, DAWN, DAY, DUSK, NIGHT };
enum class WeatherChoice { AUTO, CLEAR, CLOUDY, RAIN, SNOW, FOG, STORM };
enum class SeasonChoice { AUTO, SPRING, SUMMER, AUTUMN, WINTER };

/// Linear RGB in 0..1 (paint.h scales to a buffer's unit).
struct Rgb {
    float r = 0, g = 0, b = 0;
};
inline Rgb operator*(Rgb a, float s) { return {a.r * s, a.g * s, a.b * s}; }
inline Rgb operator*(Rgb a, Rgb b) { return {a.r * b.r, a.g * b.g, a.b * b.b}; }
inline Rgb operator+(Rgb a, Rgb b) { return {a.r + b.r, a.g + b.g, a.b + b.b}; }
Rgb mix(Rgb a, Rgb b, float t);
float luminance(Rgb c);

/// What a scene was designed as; used when nothing else decides an aspect.
struct Defaults {
    TimeOfDay time = TimeOfDay::Day;
    Weather weather = Weather::Clear;
    Season season = Season::Summer;
    /// Fine-tune the designed sun (degrees, 0..1 across the view) when the
    /// coarse time of day is not exact enough, e.g. a mid-morning scene.
    std::optional<float> sun_elevation = std::nullopt;
    std::optional<float> sun_x = std::nullopt;
};

/// Which aspects a scene follows. An aspect that is off gets no property and
/// always uses the default (a scene whose identity is "night" keeps its night).
struct Adapt {
    bool time = true;
    bool weather = true;
    bool season = true;
    /// Weather the scene cannot show sensibly falls back to the default
    /// (e.g. an underwater scene and snow). Empty = all.
    std::vector<Weather> weather_allowed;
    /// Seasons offered as fixed-look variants (empty = all). A scene where
    /// only winter looks different still follows the real season, but lists
    /// just the variants that are worth picking.
    std::vector<Season> season_variants;
    /// Same for weather (empty = every allowed one): the real weather still
    /// shows, only the listed ones are offered as variants.
    std::vector<Weather> weather_variants;
};

struct Look {
    // ---- time ----
    float sun_elevation = 40.0f;  // degrees
    float sun_x = 0.5f;           // 0 left .. 1 right, where the sun is across the view
    bool morning = false;
    float daylight = 1.0f;  // 0 night .. 1 full day
    float twilight = 0.0f;  // 0..1 golden/rose glow near sunrise and sunset
    float night = 0.0f;     // 0..1 stars visible, lamps on
    float moon_phase = 0.5f;
    float moon_light = 0.0f;  // 0..1 moonlight (night * illumination)
    float lights = 0.0f;      // 0..1 artificial lights on (dark sky or heavy weather)
    // ---- weather ----
    float clouds = 0.1f;    // 0..1 cloud amount
    float overcast = 0.0f;  // 0..1 grey sheet over the sky
    float rain = 0.0f, snow = 0.0f, fog = 0.0f, storm = 0.0f;
    float wind = 0.2f;        // -1..1 signed horizontal drift
    float snow_cover = 0.0f;  // 0..1 snow lying on the ground
    float wetness = 0.0f;     // 0..1 wet ground (reflections)
    float flash = 0.0f;       // 0..1 current lightning flash
    bool bolt = false;        // a new lightning strike started this frame
    float temperature = 15.0f;
    // ---- season weights (sum to 1) ----
    float spring = 0.0f, summer = 1.0f, autumn = 0.0f, winter = 0.0f;
    // ---- discrete state ----
    TimeOfDay time = TimeOfDay::Day;
    Weather weather = Weather::Clear;
    Season season = Season::Summer;
    std::vector<std::string> occasions;
    // ---- palette ----
    Rgb zenith, horizon;  // sky gradient, weather applied
    Rgb ambient{1, 1, 1}; // light multiplier for lit surfaces
    Rgb sun_color{1, 0.95f, 0.85f};
    Rgb haze{0.7f, 0.8f, 0.9f};  // colour distant things fade into

    [[nodiscard]] bool occasion(std::string_view id) const;
    /// Sky colour at v (0 = top of the sky, 1 = horizon).
    [[nodiscard]] Rgb sky(float v) const;
    /// A daytime albedo under the current light. depth 0 = foreground,
    /// 1 = on the horizon (fades into the haze; more in fog).
    [[nodiscard]] Rgb lit(Rgb albedo, float depth = 0.0f) const;
    /// lit() for a scene's foreground subject (a rider, a horse): it never
    /// drops below a cool fill light, so it stays readable at night and under
    /// storm clouds instead of vanishing into the dark landscape.
    [[nodiscard]] Rgb subject(Rgb albedo) const;
    /// Season tint for summer-green tree leaves (fresh in spring, ochre and
    /// rust in autumn, bare brown in winter). `strength` < 1 for mixed or
    /// evergreen woods. Apply before lit().
    [[nodiscard]] Rgb foliage(Rgb summer, float strength = 1.0f) const;
    /// Milder season tint for grass and meadows.
    [[nodiscard]] Rgb grass(Rgb summer) const;
    /// Ground/rooftop albedo with lying snow. `exposure` 0..1 scales how much
    /// snow sticks (steep rock, water: 0). Apply before lit().
    [[nodiscard]] Rgb ground(Rgb albedo, float exposure = 1.0f) const;
    /// Brightness 0..1 a lamp/window should glow with (on at dusk/night).
    [[nodiscard]] float lamp() const { return lights; }
};

/// Fills the palette and derived values from the raw numbers.
void derive_palette(Look &look);
/// The un-smoothed look for resolved choices (exposed for tests).
[[nodiscard]] Look target_look(const Conditions &conditions, const Defaults &defaults, const Adapt &adapt,
                               TimeChoice time, WeatherChoice weather, SeasonChoice season);

class SceneEnvironment {
public:
    explicit SceneEnvironment(Defaults defaults = {}, Adapt adapt = {});

    /// Properties to add in register_properties() (only adapted aspects).
    [[nodiscard]] std::vector<std::shared_ptr<Plugins::PropertyBase>> properties() const;

    /// Call once per frame with the real (unscaled) frame delta. Reads the
    /// shared conditions about once per second and eases towards them, so
    /// changes cross-fade instead of popping. The first call snaps.
    const Look &update(float dt);
    [[nodiscard]] const Look &look() const { return look_; }
    /// Make the next update() jump instead of easing (call in initialize()).
    void snap() { snapped_ = false; }
    void set_random_seed(uint32_t seed) { rng_.seed(seed); }

    /// For migrations, e.g. weather_property().legacy_name("mode").
    [[nodiscard]] Plugins::PropertyBase &weather_property() { return *weather_; }
    [[nodiscard]] const Defaults &defaults() const { return defaults_; }
    [[nodiscard]] const Adapt &adapt() const { return adapt_; }

    /// Fixed looks for Scene Lab / presets / previews: "night", "rain", ...
    /// They are off in Automatic Mode by default (the scene already follows
    /// the real world there). `extra` adds scene-specific combinations.
    void add_variants(Scenes::SceneDescriptor &descriptor,
                      std::vector<Scenes::SceneVariant> extra = {}) const;

private:
    Defaults defaults_;
    Adapt adapt_;
    PropertyPointer<Plugins::EnumProperty<TimeChoice>> time_;
    PropertyPointer<Plugins::EnumProperty<WeatherChoice>> weather_;
    PropertyPointer<Plugins::EnumProperty<SeasonChoice>> season_;
    Conditions conditions_;
    float poll_ = 0.0f;
    bool snapped_ = false;
    Look look_;
    float flash_timer_ = 4.0f;
    float flicker_ = -1.0f;
    std::mt19937 rng_{0xE17u};
};

} // namespace Environment
