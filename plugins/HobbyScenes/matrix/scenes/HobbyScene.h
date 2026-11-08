#pragma once

#include <shared/matrix/audio_state.h>

#include <random>

#include "Draw.h"
#include "shared/matrix/Scene.h"
#include "shared/matrix/plugin/main.h"

namespace Hobbies {

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

public:
    void initialize(int width, int height) override;
    void register_properties() override;
    void set_random_seed(uint32_t seed) override { rng_.seed(seed); }
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
