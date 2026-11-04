#pragma once

// Four music-reactive "ambient environment" scenes. Each hides the music
// analysis inside a calm, recognisable place instead of drawing raw meters:
//   audio_ridgeline - perspective spectral ridgeline under a synthwave sun
//   audio_pond      - night pond; percussion becomes raindrops / ripples
//   audio_reef      - aquarium; kelp heights are the spectrum, jellyfish pulse on beats
//   audio_skyline   - night city whose towers are an EQ, fireworks on drops
//   audio_planet    - ringed gas giant; each ring is a spectrum band
//   audio_campfire  - campfire whose flames follow the bass, sparks on hi-hats
//   audio_lighthouse - coast at night; wave layers are bands, beam sweeps on tempo
//   audio_stage      - concert stage; moving-head beams are bands, crowd jumps on kicks
//   audio_night_drive - night drive; streetlights pass on the beat, the car bounces on bass
//
// All of them carry the "music-showcase" tag, which both directors treat as
// the preferred, longer-dwelling tier while live music is playing.

#include <random>
#include <vector>

#include "kit/AudioSceneKit.h"
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
    // Shared per-frame prologue. Returns false when no analysis is available.
    bool begin_frame(AudioState::Snapshot& audio, float& dt);
};

// --------------------------------------------------------------------------
class AudioRidgelineScene final : public AmbientMusicScene {
    static constexpr int Rows = 34;
    static constexpr int Cols = 48;
    std::vector<std::vector<float>> history_;
    int head_ = 0;
    float rowTimer_ = 0.0f;
    AudioKit::Envelope beat_, drop_;
    AudioKit::HueCycle palette_{{300.0f, 195.0f, 255.0f, 330.0f}};
    PropertyPointer<float> scrollSpeed_ = MAKE_PROPERTY_MINMAX("scroll_speed", float, 14.0f, 4.0f, 40.0f);
    PropertyPointer<bool> showSun_ = MAKE_PROPERTY("show_sun", bool, true);

public:
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    void register_properties() override;
    std::string get_name() const override { return "audio_ridgeline"; }
    [[nodiscard]] SceneDescriptor get_descriptor() const override;
};

// --------------------------------------------------------------------------
class AudioPondScene final : public AmbientMusicScene {
    std::vector<float> h0_, h1_;
    std::vector<AudioKit::RGB> sky_;
    struct Pad {
        float x, y, r, phase;
    };
    std::vector<Pad> pads_;
    struct Firefly {
        float x, y, vx, vy, phase;
    };
    std::vector<Firefly> flies_;
    float drizzle_ = 0.0f;
    AudioKit::Envelope beat_;
    PropertyPointer<float> rain_ = MAKE_PROPERTY_MINMAX("rain_amount", float, 1.0f, 0.0f, 3.0f);
    PropertyPointer<float> damping_ = MAKE_PROPERTY_MINMAX("ripple_damping", float, 0.985f, 0.95f, 0.998f);

    void drop(float x, float y, float radius, float strength);
    void build_sky();

public:
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    void register_properties() override;
    std::string get_name() const override { return "audio_pond"; }
    [[nodiscard]] SceneDescriptor get_descriptor() const override;
};

// --------------------------------------------------------------------------
class AudioReefScene final : public AmbientMusicScene {
    struct Kelp {
        float x, height, phase, hue;
        int band;
        float level;
    };
    struct Bubble {
        float x, y, r, wobble;
    };
    struct Fish {
        float x, y, vx, vy, hue, offset;
    };
    struct Jelly {
        float x, y, vy, size, hue, pulse;
    };
    std::vector<Kelp> kelp_;
    std::vector<Bubble> bubbles_;
    std::vector<Fish> fish_;
    std::vector<Jelly> jellies_;
    std::vector<float> sand_;
    AudioKit::Envelope kick_, drop_;
    float scatter_ = 0.0f;
    PropertyPointer<int> fishCount_ = MAKE_PROPERTY_MINMAX("fish_count", int, 18, 0, 40);
    PropertyPointer<int> jellyCount_ = MAKE_PROPERTY_MINMAX("jelly_count", int, 3, 0, 6);

    void init_world();

public:
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    void register_properties() override;
    std::string get_name() const override { return "audio_reef"; }
    [[nodiscard]] SceneDescriptor get_descriptor() const override;
};

// --------------------------------------------------------------------------
class AudioSkylineScene final : public AmbientMusicScene {
    struct Tower {
        int x, w, minH, maxH, band, style;
        float level;
        uint32_t seed;
    };
    struct Spark {
        float x, y, vx, vy, life, maxLife, hue;
    };
    std::vector<Tower> back_, front_;
    std::vector<Spark> sparks_;
    std::vector<float> windowFlicker_;
    AudioKit::Envelope beat_, drop_;
    AudioKit::HueCycle palette_{{235.0f, 270.0f, 205.0f, 300.0f}};
    PropertyPointer<bool> fireworks_ = MAKE_PROPERTY("fireworks_on_drop", bool, true);
    PropertyPointer<bool> reflection_ = MAKE_PROPERTY("river_reflection", bool, true);

    void init_city();
    void firework(float x, float y, float hue, int count);

public:
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    void register_properties() override;
    std::string get_name() const override { return "audio_skyline"; }
    [[nodiscard]] SceneDescriptor get_descriptor() const override;
};

// --------------------------------------------------------------------------
class AudioPlanetScene final : public AmbientMusicScene {
    struct Meteor {
        float x, y, vx, vy, life;
    };
    std::vector<Meteor> meteors_;
    std::vector<float> ringLevel_;
    AudioKit::Envelope beat_, drop_;
    float spin_ = 0.0f, moonAngle_ = 0.6f;
    AudioKit::HueCycle palette_{{28.0f, 200.0f, 330.0f, 265.0f}};
    PropertyPointer<float> ringTilt_ = MAKE_PROPERTY_MINMAX("ring_tilt", float, 0.30f, 0.12f, 0.6f);

public:
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    void register_properties() override;
    std::string get_name() const override { return "audio_planet"; }
    [[nodiscard]] SceneDescriptor get_descriptor() const override;
};

// --------------------------------------------------------------------------
class AudioCampfireScene final : public AmbientMusicScene {
    static constexpr int FireW = 56, FireH = 84;
    std::vector<float> heat_;
    struct Spark {
        float x, y, vx, vy, life, maxLife;
    };
    std::vector<Spark> sparks_;
    AudioKit::Envelope beat_, drop_;
    float glow_ = 0.5f;
    PropertyPointer<float> fireSize_ = MAKE_PROPERTY_MINMAX("fire_size", float, 1.0f, 0.5f, 1.6f);

public:
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    void register_properties() override;
    std::string get_name() const override { return "audio_campfire"; }
    [[nodiscard]] SceneDescriptor get_descriptor() const override;
};

// --------------------------------------------------------------------------
class AudioLighthouseScene final : public AmbientMusicScene {
    std::vector<float> layerLevel_;
    AudioKit::Envelope beat_, drop_, lightning_;
    float beamAngle_ = 0.0f;
    std::vector<std::pair<float, float>> bolt_;
    PropertyPointer<int> beatsPerTurn_ = MAKE_PROPERTY_MINMAX("beats_per_rotation", int, 8, 2, 32);
    PropertyPointer<bool> storm_ = MAKE_PROPERTY("lightning_on_drop", bool, true);

public:
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    void register_properties() override;
    std::string get_name() const override { return "audio_lighthouse"; }
    [[nodiscard]] SceneDescriptor get_descriptor() const override;
};

// --------------------------------------------------------------------------
class AudioStageScene final : public AmbientMusicScene {
    static constexpr int Lights = 6;
    struct Head {
        float x, y, phase, hand;
    };
    std::vector<Head> crowd_;
    std::vector<float> lightLevel_;
    AudioKit::Envelope beat_, kick_, drop_, snare_;
    AudioKit::HueCycle palette_{{0.0f, 210.0f, 285.0f, 30.0f, 330.0f, 190.0f}};
    float sweep_ = 0.0f;
    PropertyPointer<float> crowdSize_ = MAKE_PROPERTY_MINMAX("crowd_density", float, 1.0f, 0.0f, 2.0f);
    PropertyPointer<bool> strobe_ = MAKE_PROPERTY("strobe_on_drop", bool, true);

public:
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    void register_properties() override;
    std::string get_name() const override { return "audio_stage"; }
    [[nodiscard]] SceneDescriptor get_descriptor() const override;
};

// --------------------------------------------------------------------------
class AudioNightDriveScene final : public AmbientMusicScene {
    struct Lamp {
        float z;
        int side;
    };
    struct Tower {
        int x, w, h;
        uint32_t seed;
        int band;
    };
    std::vector<Lamp> lamps_;
    std::vector<Tower> towers_;
    AudioKit::Envelope kick_, beat_, drop_;
    AudioKit::HueCycle palette_{{280.0f, 200.0f, 320.0f, 250.0f}};
    float travel_ = 0.0f, speed_ = 8.0f, bounce_ = 0.0f, bounceV_ = 0.0f;
    PropertyPointer<float> baseSpeed_ = MAKE_PROPERTY_MINMAX("drive_speed", float, 1.0f, 0.3f, 2.5f);
    PropertyPointer<float> bounceAmount_ = MAKE_PROPERTY_MINMAX("bass_bounce", float, 1.0f, 0.0f, 2.0f);

    void init_city();

public:
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    void register_properties() override;
    std::string get_name() const override { return "audio_night_drive"; }
    [[nodiscard]] SceneDescriptor get_descriptor() const override;
};

#define AMBIENT_WRAPPER(Name)                                                                  \
    class Name##Wrapper final : public Plugins::SceneWrapper {                                 \
    public:                                                                                    \
        std::unique_ptr<Scenes::Scene> create() override { return std::make_unique<Name>(); } \
    };
AMBIENT_WRAPPER(AudioRidgelineScene)
AMBIENT_WRAPPER(AudioPondScene)
AMBIENT_WRAPPER(AudioReefScene)
AMBIENT_WRAPPER(AudioSkylineScene)
AMBIENT_WRAPPER(AudioPlanetScene)
AMBIENT_WRAPPER(AudioCampfireScene)
AMBIENT_WRAPPER(AudioLighthouseScene)
AMBIENT_WRAPPER(AudioStageScene)
AMBIENT_WRAPPER(AudioNightDriveScene)
#undef AMBIENT_WRAPPER

}  // namespace Scenes
