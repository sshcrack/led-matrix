#pragma once

#include <shared/matrix/audio_state.h>

#include <array>
#include <memory>
#include <random>

#include <shared/matrix/environment/look.h>
#include <shared/matrix/environment/paint.h>

#include "Draw.h"
#include "shared/matrix/Scene.h"
#include "shared/matrix/plugin/main.h"

namespace Hobbies {
struct Look;  // FigureDraw.h

// Shared plumbing: float framebuffer, animation clock, speed property and
// output to the canvas.
class HobbySceneBase : public Scenes::Scene {
protected:
    Canvas cv_;
    float t_ = 0.0f;
    std::mt19937 rng_{std::random_device{}()};
    PropertyPointer<float> speed_ = MAKE_PROPERTY_MINMAX("speed", float, 1.0f, 0.2f, 2.5f);
    float rnd() { return std::uniform_real_distribution<float>(0.0f, 1.0f)(rng_); }
    // Advances the clock; dt is already scaled by the speed property.
    void begin_frame(float& dt);
    void present(rgb_matrix::FrameCanvas* canvas);
    void fill_gradient(Rgb top, Rgb mid, Rgb bottom, float mid_at) { cv_.fill_gradient(top, mid, bottom, mid_at); }

    // ---- real-world environment (time of day, weather, season) -----------
    // Scenery scenes call use_environment() in their constructor; begin_frame()
    // then updates env() every frame and the helpers below paint with it.
    std::unique_ptr<Environment::SceneEnvironment> environment_;
    Environment::Precipitation precip_;
    Environment::Lightning lightning_;
    void use_environment(Environment::Defaults defaults, Environment::Adapt adapt = {})
    {
        environment_ = std::make_unique<Environment::SceneEnvironment>(defaults, std::move(adapt));
    }
    [[nodiscard]] const Environment::Look& env() const { return environment_->look(); }
    [[nodiscard]] Environment::Surface surface() { return {cv_.px.data(), cv_.w, cv_.h, 1.0f}; }
    static Environment::Rgb to_env(Rgb c) { return {c.r, c.g, c.b}; }
    static Rgb from_env(Environment::Rgb c) { return {c.r, c.g, c.b}; }
    // Albedo under the current light; depth 0 foreground .. 1 horizon.
    [[nodiscard]] Rgb lit(Rgb albedo, float depth = 0.0f) const { return from_env(env().lit(to_env(albedo), depth)); }
    // Tree leaves tinted for the season, then lit.
    [[nodiscard]] Rgb plant(Rgb summer, float depth = 0.0f, float snow_exposure = 0.5f, float season_strength = 1.0f) const
    {
        return from_env(env().lit(env().ground(env().foliage(to_env(summer), season_strength), snow_exposure), depth));
    }
    // Grass/meadow tinted for the season, then lit.
    [[nodiscard]] Rgb meadow(Rgb summer, float depth = 0.0f, float snow_exposure = 1.0f) const
    {
        return from_env(env().lit(env().ground(env().grass(to_env(summer)), snow_exposure), depth));
    }
    // Ground/roof albedo with lying snow, then lit.
    [[nodiscard]] Rgb terrain(Rgb albedo, float depth = 0.0f, float snow_exposure = 1.0f) const
    {
        return from_env(env().lit(env().ground(to_env(albedo), snow_exposure), depth));
    }
    // Sky, stars, sun, moon and clouds over rows [0, horizon).
    void paint_sky_layers(float horizon, float scroll = 0.0f, float sun_radius = 7.0f, float cloud_top = 0.06f,
                          float cloud_bottom = 0.42f);
    // Rain/snow (landing on `ground`), lightning and nothing else.
    void paint_weather(float raw_dt, float horizon, const Environment::Precipitation::GroundFn& ground = {});
    // True when the look moved enough that a cached backdrop must be redrawn
    // (every frame during a cross-fade, otherwise rarely). Updates `key`.
    bool environment_changed(std::array<float, 12>& key) const;
    // Figure clothing under the current light.
    void light_look(Look& look) const;

public:
    void initialize(int width, int height) override;
    void register_properties() override;
    void set_random_seed(uint32_t seed) override
    {
        rng_.seed(seed);
        precip_.set_seed(seed + 1);
        lightning_.set_seed(seed + 2);
        if (environment_)
            environment_->set_random_seed(seed + 3);
    }
    [[nodiscard]] std::string get_category() const override { return "Hobbies"; }
    [[nodiscard]] bool supports_virtual_time() const override { return true; }
    tmillis_t get_default_duration() override { return 45000; }
    int get_default_weight() override { return 6; }
};

// Smoothed view of the live music analysis. Everything fades to zero when no
// fresh analysis arrives, so a scene that uses it stays a calm ambient scene
// without music and comes alive while music plays.
struct AudioPulse {
    float amount = 0.0f;  // 0..1: how much audio drives the scene right now
    float bass = 0.0f, mid = 0.0f, treble = 0.0f, level = 0.0f;
    float beat = 0.0f, onset = 0.0f, drop = 0.0f;  // decaying event envelopes
    bool beatHit = false, onsetHit = false, dropHit = false;
    float bpm = 0.0f;         // 0 while no confident tempo
    float beatPhase = 0.0f;   // 0..1 within the current beat (valid if bpm > 0)

    void update(bool enabled, float strength, float dt);

private:
    uint64_t beatSeen_ = 0, onsetSeen_ = 0, dropSeen_ = 0;
    bool primed_ = false;
};

// Hobby scene that optionally follows music (property "audio_reactive").
class AudioHobbyScene : public HobbySceneBase {
protected:
    AudioPulse audio_;
    PropertyPointer<bool> audioReactive_ = MAKE_PROPERTY("audio_reactive", bool, true);
    PropertyPointer<float> audioStrength_ = MAKE_PROPERTY_MINMAX("audio_strength", float, 1.0f, 0.0f, 2.0f);
    // begin_frame() plus the audio update. `raw_dt` is unscaled by speed.
    void begin_audio_frame(float& dt, float& raw_dt);

public:
    void register_properties() override;
    Scenes::SceneCapabilities get_capabilities() const override
    {
        auto caps = Scene::get_capabilities();
        caps.supports_audio = true;
        return caps;
    }
    [[nodiscard]] Previews::SceneSpec get_preview_spec() const override
    {
        auto spec = Previews::SceneSpec::with_inputs({Previews::Inputs::Audio});
        spec.set_property("audio_reactive", true);
        return spec;
    }
};

template <typename SceneT>
class HobbySceneWrapper final : public Plugins::SceneWrapper {
public:
    std::unique_ptr<Scenes::Scene> create() override { return std::make_unique<SceneT>(); }
};

}  // namespace Hobbies
