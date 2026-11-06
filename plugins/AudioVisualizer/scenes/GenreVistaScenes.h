#pragma once

// Ten more music-reactive places, cast from the owner's actual Spotify genre
// cache on ledmat (German indie/pop/hip hop, pop, rock/punk/metal, classic and
// yacht rock, eurodance/hard techno, drum and bass, NDW):
//
//   audio_skatepark     - night halfpipe; the skater airs on kicks, graffiti
//                         wall lights per band            (rock: punk, pop punk)
//   audio_mosh_pit      - top-down circle pit; the crowd orbits faster with the
//                         energy, wall of death on drops   (rock: nu metal, metal)
//   audio_storm_front   - thunderstorm over a field; bolts on onsets, rain with
//                         the treble                      (rock: AC/DC, nu metal)
//   audio_drum_kit      - spot-lit drum kit; kick/snare/hats/toms/crash hit with
//                         the matching analysis feature   (rock, hiphop)
//   audio_tesla_coil    - coils arcing to ground rods; kicks fire bolts, treble
//                         feeds the corona                 (edm: hard techno, electro)
//   audio_fireworks     - harbour fireworks; shells on beats, finale on drops
//                         (party, edm: pop, eurodance)
//   audio_roller_rink   - roller disco from above; skaters lap the rink, floor
//                         tiles flash per beat             (party, edm: pop, eurodance)
//   audio_marina_sunset - sunset marina; sun and water shimmer with the music
//                         (chill, acoustic: yacht rock, indie pop)
//   audio_string_lights - summer-evening garden with garlands; bulbs follow
//                         bands, fireflies drift           (acoustic, chill: German indie)
//   audio_cassette      - mixtape close-up; reels spool, LED bars are the
//                         spectrum                         (acoustic, party: NDW, britpop)

#include <vector>

#include "kit/AmbientMusicScene.h"
#include "kit/GenreVistaKit.h"

namespace Scenes {

// --------------------------------------------------------------------------
class AudioSkateparkScene final : public AmbientMusicScene {
    AudioKit::Envelope beat_, kick_, drop_;
    AudioKit::HueCycle palette_{{300.0f, 175.0f, 50.0f, 340.0f, 130.0f}};
    float phase_ = 0.0f;      // position in the pipe (radians, side to side)
    float air_ = 0.0f;        // 0..1 height envelope of the current trick
    float airVel_ = 0.0f;
    float spin_ = 0.0f;       // trick rotation
    float trickDir_ = 1.0f;
    PropertyPointer<float> speed_ = MAKE_PROPERTY_MINMAX("skater_speed", float, 1.0f, 0.3f, 2.5f);
    PropertyPointer<bool> graffiti_ = MAKE_PROPERTY("show_graffiti", bool, true);

public:
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    void register_properties() override;
    std::string get_name() const override { return "audio_skatepark"; }
    [[nodiscard]] SceneDescriptor get_descriptor() const override;
};

// --------------------------------------------------------------------------
class AudioMoshPitScene final : public AmbientMusicScene {
    struct Moshers {
        float ang, rad, speed, phase, hue;
    };
    std::vector<Moshers> crowd_;
    AudioKit::Envelope beat_, kick_, drop_, strobe_, wall_;
    AudioKit::HueCycle palette_{{0.0f, 215.0f, 35.0f, 290.0f}};
    float spin_ = 0.0f, split_ = 0.0f;
    PropertyPointer<int> crowdSize_ = MAKE_PROPERTY_MINMAX("crowd_size", int, 120, 30, 260);
    PropertyPointer<bool> strobe_on_ = MAKE_PROPERTY("strobe_on_drop", bool, true);

    void init_world();

public:
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    void register_properties() override;
    std::string get_name() const override { return "audio_mosh_pit"; }
    [[nodiscard]] SceneDescriptor get_descriptor() const override;
};

// --------------------------------------------------------------------------
class AudioStormFrontScene final : public AmbientMusicScene {
    struct Bolt {
        std::vector<std::pair<float, float>> pts;
        std::vector<std::pair<float, float>> branch;
        float life = 0.0f;
    };
    struct Streak {
        float x, y, speed;
    };
    std::vector<float> cloudA_, cloudB_;  // cached cloud density, tiled horizontally
    std::vector<Streak> rain_;
    std::vector<Bolt> bolts_;
    AudioKit::Envelope flash_, kick_, drop_;
    AudioKit::HueCycle palette_{{225.0f, 265.0f, 200.0f, 300.0f}};
    float wind_ = 0.0f;
    PropertyPointer<float> rainAmount_ = MAKE_PROPERTY_MINMAX("rain_amount", float, 1.0f, 0.0f, 2.0f);
    PropertyPointer<float> boltRate_ = MAKE_PROPERTY_MINMAX("lightning_rate", float, 1.0f, 0.2f, 2.5f);

    void init_world();
    void strike(float x);

public:
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    void register_properties() override;
    std::string get_name() const override { return "audio_storm_front"; }
    [[nodiscard]] SceneDescriptor get_descriptor() const override;
};

// --------------------------------------------------------------------------
class AudioDrumKitScene final : public AmbientMusicScene {
    AudioKit::Envelope kick_, snare_, hat_, tom1_, tom2_, crash_, beat_, drop_;
    AudioKit::HueCycle palette_{{20.0f, 200.0f, 330.0f, 50.0f}};
    uint32_t hits_ = 0;
    float hatOpen_ = 0.0f;
    PropertyPointer<float> glowAmount_ = MAKE_PROPERTY_MINMAX("backlight", float, 1.0f, 0.2f, 2.0f);
    PropertyPointer<bool> sticks_ = MAKE_PROPERTY("show_sticks", bool, true);

public:
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    void register_properties() override;
    std::string get_name() const override { return "audio_drum_kit"; }
    [[nodiscard]] SceneDescriptor get_descriptor() const override;
};

// --------------------------------------------------------------------------
class AudioTeslaCoilScene final : public AmbientMusicScene {
    struct Arc {
        std::vector<std::pair<float, float>> pts;
        float life = 0.0f, hue = 0.0f;
    };
    std::vector<Arc> arcs_;
    AudioKit::Envelope beat_, kick_, drop_, corona_;
    AudioKit::HueCycle palette_{{275.0f, 195.0f, 320.0f, 160.0f}};
    PropertyPointer<int> coils_ = MAKE_PROPERTY_MINMAX("coil_count", int, 3, 1, 4);
    PropertyPointer<float> arcRate_ = MAKE_PROPERTY_MINMAX("arc_rate", float, 1.0f, 0.3f, 2.5f);

    void spark(float x0, float y0, float x1, float y1, float hue);

public:
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    void register_properties() override;
    std::string get_name() const override { return "audio_tesla_coil"; }
    [[nodiscard]] SceneDescriptor get_descriptor() const override;
};

// --------------------------------------------------------------------------
class AudioFireworksScene final : public AmbientMusicScene {
    struct Shell {
        float x, y, vx, vy, apex, hue;
        int band;
    };
    struct Spark {
        float x, y, vx, vy, life, maxLife, hue, sat;
    };
    std::vector<Shell> shells_;
    std::vector<Spark> sparks_;
    std::vector<float> skyline_;   // per-column rooftop y (cached)
    std::vector<uint8_t> windows_; // lit-window mask (cached)
    AudioKit::Envelope beat_, kick_, drop_, flash_;
    AudioKit::HueCycle palette_{{10.0f, 200.0f, 320.0f, 50.0f, 275.0f}};
    float finale_ = 0.0f;
    PropertyPointer<float> density_ = MAKE_PROPERTY_MINMAX("firework_density", float, 1.0f, 0.3f, 2.0f);
    PropertyPointer<bool> reflection_ = MAKE_PROPERTY("water_reflection", bool, true);

    void init_world();
    void launch(float x, int band, float power);
    void burst(const Shell& s, float power);

public:
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    void register_properties() override;
    std::string get_name() const override { return "audio_fireworks"; }
    [[nodiscard]] SceneDescriptor get_descriptor() const override;
};

// --------------------------------------------------------------------------
class AudioRollerRinkScene final : public AmbientMusicScene {
    struct Skater {
        float lane, phase, speed, hue;
    };
    std::vector<Skater> skaters_;
    std::vector<float> tile_;   // per tile glow
    AudioKit::Envelope beat_, kick_, drop_;
    AudioKit::HueCycle palette_{{320.0f, 190.0f, 50.0f, 270.0f, 150.0f}};
    uint32_t beatCount_ = 0;
    float ballSpin_ = 0.0f;
    PropertyPointer<int> skaterCount_ = MAKE_PROPERTY_MINMAX("skater_count", int, 14, 4, 30);
    PropertyPointer<float> lapSpeed_ = MAKE_PROPERTY_MINMAX("lap_speed", float, 1.0f, 0.3f, 2.5f);

    void init_world();

public:
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    void register_properties() override;
    std::string get_name() const override { return "audio_roller_rink"; }
    [[nodiscard]] SceneDescriptor get_descriptor() const override;
};

// --------------------------------------------------------------------------
class AudioMarinaSunsetScene final : public AmbientMusicScene {
    struct Gull {
        float x, y, vx, flap, size;
    };
    std::vector<Gull> gulls_;
    std::vector<float> shimmer_;   // per-column water shimmer level
    AudioKit::Envelope beat_, kick_, drop_;
    AudioKit::HueCycle palette_{{18.0f, 345.0f, 30.0f, 300.0f}};
    float bob_ = 0.0f, sunDrift_ = 0.0f;
    PropertyPointer<bool> boat_ = MAKE_PROPERTY("show_sailboat", bool, true);
    PropertyPointer<float> gullRate_ = MAKE_PROPERTY_MINMAX("seagulls", float, 1.0f, 0.0f, 2.0f);

public:
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    void register_properties() override;
    std::string get_name() const override { return "audio_marina_sunset"; }
    [[nodiscard]] SceneDescriptor get_descriptor() const override;
};

// --------------------------------------------------------------------------
class AudioStringLightsScene final : public AmbientMusicScene {
    struct Fly {
        float x, y, phase, speed;
    };
    std::vector<Fly> flies_;
    std::vector<float> bulbLevel_;
    AudioKit::Envelope beat_, drop_, sway_;
    AudioKit::HueCycle palette_{{38.0f, 12.0f, 320.0f, 195.0f, 55.0f}};
    PropertyPointer<int> rows_ = MAKE_PROPERTY_MINMAX("garland_rows", int, 3, 2, 5);
    PropertyPointer<bool> fireflies_ = MAKE_PROPERTY("fireflies", bool, true);

    void init_world();

public:
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    void register_properties() override;
    std::string get_name() const override { return "audio_string_lights"; }
    [[nodiscard]] SceneDescriptor get_descriptor() const override;
};

// --------------------------------------------------------------------------
class AudioCassetteScene final : public AmbientMusicScene {
    AudioKit::Envelope beat_, kick_, drop_;
    AudioKit::HueCycle palette_{{25.0f, 190.0f, 335.0f, 130.0f}};
    float reelA_ = 0.0f, reelB_ = 0.0f, tape_ = 0.0f, wobble_ = 0.0f;
    PropertyPointer<float> spoolSpeed_ = MAKE_PROPERTY_MINMAX("spool_speed", float, 1.0f, 0.3f, 3.0f);
    PropertyPointer<bool> ledBars_ = MAKE_PROPERTY("led_bars", bool, true);

public:
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    void register_properties() override;
    std::string get_name() const override { return "audio_cassette"; }
    [[nodiscard]] SceneDescriptor get_descriptor() const override;
};

using AudioSkateparkSceneWrapper = AmbientSceneWrapper<AudioSkateparkScene>;
using AudioMoshPitSceneWrapper = AmbientSceneWrapper<AudioMoshPitScene>;
using AudioStormFrontSceneWrapper = AmbientSceneWrapper<AudioStormFrontScene>;
using AudioDrumKitSceneWrapper = AmbientSceneWrapper<AudioDrumKitScene>;
using AudioTeslaCoilSceneWrapper = AmbientSceneWrapper<AudioTeslaCoilScene>;
using AudioFireworksSceneWrapper = AmbientSceneWrapper<AudioFireworksScene>;
using AudioRollerRinkSceneWrapper = AmbientSceneWrapper<AudioRollerRinkScene>;
using AudioMarinaSunsetSceneWrapper = AmbientSceneWrapper<AudioMarinaSunsetScene>;
using AudioStringLightsSceneWrapper = AmbientSceneWrapper<AudioStringLightsScene>;
using AudioCassetteSceneWrapper = AmbientSceneWrapper<AudioCassetteScene>;

}  // namespace Scenes
