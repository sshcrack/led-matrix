#pragma once

#include <shared/matrix/audio_state.h>
#include <shared/matrix/particles.h>
#include "shared/matrix/Scene.h"
#include "shared/matrix/wrappers.h"
#include <random>

namespace Scenes {

class AudioParticleFieldScene final : public Scene {
    struct Particle : Particles::KinematicParticle { float hue = 0.0f; };
    Particles::ParticlePool<Particle> particles_{3000};
    std::mt19937 rng_{std::random_device{}()};
    uint64_t beatSeen_ = 0, onsetSeen_ = 0, dropSeen_ = 0;
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

    void spawn(const AudioState::Snapshot &audio, int count, bool radial, float strength);

public:
    AudioParticleFieldScene() = default;
    bool render(rgb_matrix::FrameCanvas *canvas) override;
    void register_properties() override;
    std::string get_name() const override { return "audio_particles"; }
    std::string get_category() const override { return "Audio Reactive"; }
    tmillis_t get_default_duration() override { return 30000; }
    int get_default_weight() override { return 5; }
    bool needs_desktop_app() override { return true; }
    [[nodiscard]] Previews::SceneSpec get_preview_spec() const override {
        return Previews::SceneSpec::with_inputs({Previews::Inputs::Audio});
    }
    SceneCapabilities get_capabilities() const override {
        auto caps = Scene::get_capabilities();
        caps.requires_audio = true; caps.supports_audio = true;
        return caps;
    }
    [[nodiscard]] bool supports_virtual_time() const override { return true; }
};

class AudioPulseTunnelScene final : public Scene {
    uint64_t beatSeen_ = 0, dropSeen_ = 0, sectionSeen_ = 0;
    float travel_ = 0.0f, rotation_ = 0.0f, beatPulse_ = 0.0f, dropPulse_ = 0.0f;
    float paletteOffset_ = 0.0f;

    PropertyPointer<float> sensitivity_ = MAKE_PROPERTY_MINMAX("sensitivity", float, 1.0f, 0.2f, 3.0f);
    PropertyPointer<float> speed_ = MAKE_PROPERTY_MINMAX("speed", float, 1.0f, 0.1f, 4.0f);
    PropertyPointer<int> ringCount_ = MAKE_PROPERTY_MINMAX("ring_count", int, 13, 4, 28);
    PropertyPointer<float> twist_ = MAKE_PROPERTY_MINMAX("twist", float, 1.0f, -3.0f, 3.0f);
    PropertyPointer<bool> rainbow_ = MAKE_PROPERTY("rainbow", bool, true);
    PropertyPointer<rgb_matrix::Color> baseColor_ = MAKE_PROPERTY("base_color", rgb_matrix::Color, rgb_matrix::Color(50, 80, 255));
    PropertyPointer<bool> spectrumRibs_ = MAKE_PROPERTY("spectrum_ribs", bool, true);
    PropertyPointer<bool> tempoLock_ = MAKE_PROPERTY("tempo_lock", bool, true);
    PropertyPointer<bool> useSpotifyArtwork_ = MAKE_PROPERTY("use_spotify_artwork", bool, false);

public:
    AudioPulseTunnelScene() = default;
    bool render(rgb_matrix::FrameCanvas *canvas) override;
    void register_properties() override;
    std::string get_name() const override { return "audio_pulse_tunnel"; }
    std::string get_category() const override { return "Audio Reactive"; }
    tmillis_t get_default_duration() override { return 30000; }
    int get_default_weight() override { return 5; }
    bool needs_desktop_app() override { return true; }
    [[nodiscard]] Previews::SceneSpec get_preview_spec() const override {
        return Previews::SceneSpec::with_inputs({Previews::Inputs::Audio});
    }
    SceneCapabilities get_capabilities() const override {
        auto caps = Scene::get_capabilities();
        caps.requires_audio = true; caps.supports_audio = true;
        return caps;
    }
    [[nodiscard]] bool supports_virtual_time() const override { return true; }
};

class AudioAuroraScene final : public Scene {
    uint64_t beatSeen_ = 0, sectionSeen_ = 0, dropSeen_ = 0;
    float time_ = 0.0f, beatGlow_ = 0.0f, dropGlow_ = 0.0f, palette_ = 0.0f;

    PropertyPointer<int> ribbonCount_ = MAKE_PROPERTY_MINMAX("ribbon_count", int, 6, 3, 12);
    PropertyPointer<float> flowSpeed_ = MAKE_PROPERTY_MINMAX("flow_speed", float, 1.0f, 0.1f, 3.0f);
    PropertyPointer<float> sensitivity_ = MAKE_PROPERTY_MINMAX("sensitivity", float, 1.0f, 0.2f, 3.0f);
    PropertyPointer<float> glow_ = MAKE_PROPERTY_MINMAX("glow", float, 0.8f, 0.0f, 2.0f);
    PropertyPointer<bool> stars_ = MAKE_PROPERTY("high_frequency_stars", bool, true);
    PropertyPointer<bool> useSpotifyArtwork_ = MAKE_PROPERTY("use_spotify_artwork", bool, false);

public:
    AudioAuroraScene() = default;
    bool render(rgb_matrix::FrameCanvas *canvas) override;
    void register_properties() override;
    std::string get_name() const override { return "audio_aurora"; }
    std::string get_category() const override { return "Audio Reactive"; }
    tmillis_t get_default_duration() override { return 35000; }
    int get_default_weight() override { return 6; }
    bool needs_desktop_app() override { return true; }
    [[nodiscard]] Previews::SceneSpec get_preview_spec() const override {
        return Previews::SceneSpec::with_inputs({Previews::Inputs::Audio});
    }
    SceneCapabilities get_capabilities() const override {
        auto caps = Scene::get_capabilities();
        caps.requires_audio = true; caps.supports_audio = true;
        return caps;
    }
    [[nodiscard]] bool supports_virtual_time() const override { return true; }
};

class AudioKaleidoscopeScene final : public Scene {
    uint64_t beatSeen_ = 0, onsetSeen_ = 0, sectionSeen_ = 0;
    float rotation_ = 0.0f, beatPulse_ = 0.0f, onsetPulse_ = 0.0f, palette_ = 0.0f;

    PropertyPointer<int> symmetry_ = MAKE_PROPERTY_MINMAX("symmetry", int, 8, 4, 16);
    PropertyPointer<float> sensitivity_ = MAKE_PROPERTY_MINMAX("sensitivity", float, 1.0f, 0.2f, 3.0f);
    PropertyPointer<float> rotationSpeed_ = MAKE_PROPERTY_MINMAX("rotation_speed", float, 0.45f, -2.0f, 2.0f);
    PropertyPointer<float> detail_ = MAKE_PROPERTY_MINMAX("detail", float, 1.0f, 0.3f, 2.5f);
    PropertyPointer<bool> waveformCore_ = MAKE_PROPERTY("waveform_core", bool, true);
    PropertyPointer<bool> useSpotifyArtwork_ = MAKE_PROPERTY("use_spotify_artwork", bool, false);

public:
    AudioKaleidoscopeScene() = default;
    bool render(rgb_matrix::FrameCanvas *canvas) override;
    void register_properties() override;
    std::string get_name() const override { return "audio_kaleidoscope"; }
    std::string get_category() const override { return "Audio Reactive"; }
    tmillis_t get_default_duration() override { return 30000; }
    int get_default_weight() override { return 5; }
    bool needs_desktop_app() override { return true; }
    [[nodiscard]] Previews::SceneSpec get_preview_spec() const override {
        return Previews::SceneSpec::with_inputs({Previews::Inputs::Audio});
    }
    SceneCapabilities get_capabilities() const override {
        auto caps = Scene::get_capabilities();
        caps.requires_audio = true; caps.supports_audio = true;
        return caps;
    }
    [[nodiscard]] bool supports_virtual_time() const override { return true; }
};

class AudioParticleFieldSceneWrapper final : public Plugins::SceneWrapper { public: std::unique_ptr<Scenes::Scene> create() override; };
class AudioPulseTunnelSceneWrapper final : public Plugins::SceneWrapper { public: std::unique_ptr<Scenes::Scene> create() override; };
class AudioAuroraSceneWrapper final : public Plugins::SceneWrapper { public: std::unique_ptr<Scenes::Scene> create() override; };
class AudioKaleidoscopeSceneWrapper final : public Plugins::SceneWrapper { public: std::unique_ptr<Scenes::Scene> create() override; };

} // namespace Scenes
