#pragma once

#include <shared/matrix/audio_state.h>
#include <shared/matrix/particles.h>

#include <random>

#include "shared/matrix/Scene.h"
#include "shared/matrix/wrappers.h"

namespace Scenes {

class AudioParticleFieldScene final : public Scene {
    struct Particle : Particles::KinematicParticle {
        float hue = 0.0f;
    };
    Particles::ParticlePool<Particle> particles_{3000};
    std::mt19937 rng_{std::random_device{}()};
    uint64_t beatSeen_ = 0, onsetSeen_ = 0, dropSeen_ = 0;
    bool eventsPrimed_ = false;
    float spawnAccumulator_ = 0.0f;
    float hueTime_ = 0.0f;

    PropertyPointer<float> sensitivity_ = MAKE_PROPERTY_MINMAX("sensitivity", float, 1.0f, 0.2f, 3.0f);
    PropertyPointer<int> particleLimit_ = MAKE_PROPERTY_MINMAX("particle_limit", int, 900, 100, 3000);
    PropertyPointer<float> persistence_ = MAKE_PROPERTY_MINMAX("persistence", float, 1.0f, 0.2f, 3.0f);
    PropertyPointer<float> gravity_ = MAKE_PROPERTY_MINMAX("gravity", float, 18.0f, -30.0f, 60.0f);
    PropertyPointer<bool> rainbow_ = MAKE_PROPERTY("rainbow", bool, true);
    PropertyPointer<rgb_matrix::Color> baseColor_ = MAKE_PROPERTY("base_color", rgb_matrix::Color, rgb_matrix::Color(50, 210, 255));
    PropertyPointer<bool> percussionBursts_ = MAKE_PROPERTY("percussion_bursts", bool, true);
    PropertyPointer<bool> dropExplosion_ = MAKE_PROPERTY("drop_explosion", bool, true);
    PropertyPointer<bool> useSpotifyArtwork_ = MAKE_PROPERTY("use_spotify_artwork", bool, false);

    void spawn(const AudioState::Snapshot& audio, int count, bool radial, float strength);

public:
    AudioParticleFieldScene() = default;
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    void register_properties() override;
    std::string get_name() const override { return "audio_particles"; }
    std::string get_category() const override { return "Audio Reactive"; }
    [[nodiscard]] SceneDescriptor get_descriptor() const override;
    tmillis_t get_default_duration() override { return 30000; }
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
};

class AudioParticleFieldSceneWrapper final : public Plugins::SceneWrapper {
public:
    std::unique_ptr<Scenes::Scene> create() override;
};

}  // namespace Scenes
