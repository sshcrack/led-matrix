#pragma once

// Base class of every "ambient music" scene: an audio-reactive place rather
// than a meter. Provides the float frame buffer, an auto-gain spectrum
// follower, exactly-once beat/onset/drop/section events and a shared
// sensitivity property. Subclasses call begin_frame() first in render().

#include <random>

#include "AudioSceneKit.h"
#include "shared/matrix/Scene.h"
#include "shared/matrix/plugin/property.h"
#include "shared/matrix/wrappers.h"

namespace Scenes {

class AmbientMusicScene : public Scene {
public:
    std::string get_category() const override { return "Audio Reactive"; }
    tmillis_t get_default_duration() override { return 45000; }
    int get_default_weight() override { return 5; }
    bool needs_desktop_app() override { return true; }
    [[nodiscard]] Previews::SceneSpec get_preview_spec() const override
    {
        return Previews::SceneSpec::with_inputs({Previews::Inputs::Audio});
    }
    SceneCapabilities get_capabilities() const override
    {
        auto caps = Scene::get_capabilities();
        caps.requires_audio = true;
        caps.supports_audio = true;
        return caps;
    }
    [[nodiscard]] bool supports_virtual_time() const override { return true; }

protected:
    PropertyPointer<float> sensitivity_ = MAKE_PROPERTY_MINMAX("sensitivity", float, 1.0f, 0.2f, 3.0f);
    AudioKit::Buffer buf_;
    AudioKit::SpectrumFollower spectrum_{32};
    AudioKit::EventTracker events_;
    std::mt19937 rng_{0xA0D10u};
    float time_ = 0.0f;

    float rnd() { return std::uniform_real_distribution<float>(0.0f, 1.0f)(rng_); }
    // Lattice step for Buffer::add_field(): per pixel at full quality (desktop
    // worker, previews, an unloaded Pi), coarser as the Pi's adaptive quality
    // drops. The Pi then offloads the scene to a compatible desktop worker.
    [[nodiscard]] int field_step() const
    {
        const float q = render_quality_scale();
        return q >= 0.95f ? 1 : q >= 0.75f ? 2 : 3;
    }
    // Shared per-frame prologue. Returns false when no analysis is available.
    bool begin_frame(AudioState::Snapshot& audio, float& dt);
};

/// Shorthand for the per-scene plugin wrapper.
template <typename SceneT>
class AmbientSceneWrapper final : public Plugins::SceneWrapper {
public:
    std::unique_ptr<Scenes::Scene> create() override { return std::make_unique<SceneT>(); }
};

}  // namespace Scenes
