#pragma once

// Five more music-reactive places, cast by genre. They lean towards the
// styles the owner actually plays most (German rap, indie/pop, rock/punk):
//
//   audio_rain_window - moody night window: city bokeh is the spectrum,
//                       raindrops run down the glass on beats    (hiphop, chill)
//   audio_boombox     - street corner at night; speaker cones pump with the
//                       bass, the wall mural lights up per band  (hiphop, party)
//   audio_volcano     - island volcano at dusk; lava glows with the bass,
//                       fountains on kicks, eruption on drops    (rock, edm)
//   audio_festival    - open-air festival at sunset; the LED wall is the
//                       spectrum, crowd jumps, confetti on drops (rock, party)
//   audio_disco       - mirror-ball room; reflected spots are bands, floor
//                       tiles change on beats                    (party, edm)

#include <vector>

#include "kit/AmbientMusicScene.h"

namespace Scenes {

// --------------------------------------------------------------------------
class AudioRainWindowScene final : public AmbientMusicScene {
    struct Bokeh {
        float x, y, r, hue, sat, phase;
        int band;
        float level;
    };
    struct Drop {
        float x, y, vy, size, wobble;
    };
    std::vector<Bokeh> bokeh_;
    std::vector<Drop> drops_;     // running drops
    std::vector<float> beads_;    // static bead mask (0..1) on the glass
    std::vector<float> trail_;    // wet trails left by running drops
    AudioKit::Envelope beat_, drop_;
    AudioKit::HueCycle palette_{{32.0f, 200.0f, 330.0f, 45.0f, 280.0f}};
    PropertyPointer<float> rain_ = MAKE_PROPERTY_MINMAX("rain_amount", float, 1.0f, 0.0f, 3.0f);
    PropertyPointer<bool> showMug_ = MAKE_PROPERTY("show_mug", bool, true);

    void init_world();

public:
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    void register_properties() override;
    std::string get_name() const override { return "audio_rain_window"; }
    [[nodiscard]] SceneDescriptor get_descriptor() const override;
};

// --------------------------------------------------------------------------
class AudioVolcanoScene final : public AmbientMusicScene {
    struct Ember {
        float x, y, vx, vy, life, maxLife, heat;
    };
    struct Puff {
        float x, y, r, life;
    };
    std::vector<Ember> embers_;
    std::vector<Puff> smoke_;
    std::vector<float> cone_;       // per-column mountain top (y)
    std::vector<uint8_t> lava_;     // lava-river mask (river index + 1)
    std::vector<float> lavaPos_;    // position along its river 0..1
    std::vector<std::pair<float, float>> bolt_;
    AudioKit::Envelope kick_, drop_, bolt_env_, glow_;
    float boltCooldown_ = 0.0f;
    PropertyPointer<float> eruption_ = MAKE_PROPERTY_MINMAX("eruption_strength", float, 1.0f, 0.3f, 2.0f);
    PropertyPointer<bool> lightning_ = MAKE_PROPERTY("ash_lightning", bool, true);

    void init_world();

public:
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    void register_properties() override;
    std::string get_name() const override { return "audio_volcano"; }
    [[nodiscard]] SceneDescriptor get_descriptor() const override;
};

// --------------------------------------------------------------------------
class AudioDiscoScene final : public AmbientMusicScene {
    struct Spot {
        float lon, lat, size;
        int band;
        float hueOffset;
    };
    std::vector<Spot> spots_;
    std::vector<float> tileHue_, tileLit_;
    AudioKit::Envelope beat_, kick_, drop_;
    AudioKit::HueCycle palette_{{300.0f, 190.0f, 40.0f, 260.0f, 340.0f}};
    float spin_ = 0.0f;
    uint32_t beatCount_ = 0;
    PropertyPointer<float> spinSpeed_ = MAKE_PROPERTY_MINMAX("spin_speed", float, 1.0f, 0.2f, 3.0f);
    PropertyPointer<int> spotCount_ = MAKE_PROPERTY_MINMAX("spot_count", int, 70, 10, 160);

    void init_world();

public:
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    void register_properties() override;
    std::string get_name() const override { return "audio_disco"; }
    [[nodiscard]] SceneDescriptor get_descriptor() const override;
};

// --------------------------------------------------------------------------
class AudioFestivalScene final : public AmbientMusicScene {
    struct Fan {
        float x, row, phase, hand, flagHue;
        bool flag;
    };
    struct Confetti {
        float x, y, vx, vy, hue, spin, life;
    };
    std::vector<Fan> crowd_;
    std::vector<Confetti> confetti_;
    std::vector<float> wallLevel_;
    std::vector<float> hills_;
    AudioKit::Envelope beat_, kick_, drop_;
    AudioKit::HueCycle palette_{{20.0f, 320.0f, 200.0f, 45.0f, 280.0f}};
    float sway_ = 0.0f;
    PropertyPointer<float> crowdSize_ = MAKE_PROPERTY_MINMAX("crowd_density", float, 1.0f, 0.3f, 1.6f);
    PropertyPointer<bool> confettiOn_ = MAKE_PROPERTY("confetti_on_drop", bool, true);

    void init_world();

public:
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    void register_properties() override;
    std::string get_name() const override { return "audio_festival"; }
    [[nodiscard]] SceneDescriptor get_descriptor() const override;
};

// --------------------------------------------------------------------------
class AudioBoomboxScene final : public AmbientMusicScene {
    struct Ring {
        float r, strength;
        int side;
    };
    struct Leaf {
        float x, y, vx, vy, spin, hue;
    };
    std::vector<Ring> rings_;
    std::vector<Leaf> litter_;
    std::vector<float> bricks_;   // per-pixel brick texture (cached)
    std::vector<float> mural_;    // per-pixel mural shape id + shade (cached)
    std::vector<float> lamp_;     // per-pixel street-lamp falloff (cached; the lamp never moves)
    std::vector<float> eq_;
    AudioKit::Envelope beat_, kick_, snare_, drop_;
    AudioKit::HueCycle palette_{{285.0f, 25.0f, 190.0f, 330.0f, 140.0f}};
    float cone_ = 0.0f, coneV_ = 0.0f, netSwing_ = 0.0f;
    PropertyPointer<float> pump_ = MAKE_PROPERTY_MINMAX("speaker_pump", float, 1.0f, 0.0f, 2.0f);
    PropertyPointer<bool> mural_on_ = MAKE_PROPERTY("show_mural", bool, true);

    void init_world();

public:
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    void register_properties() override;
    std::string get_name() const override { return "audio_boombox"; }
    [[nodiscard]] SceneDescriptor get_descriptor() const override;
};

using AudioRainWindowSceneWrapper = AmbientSceneWrapper<AudioRainWindowScene>;
using AudioVolcanoSceneWrapper = AmbientSceneWrapper<AudioVolcanoScene>;
using AudioDiscoSceneWrapper = AmbientSceneWrapper<AudioDiscoScene>;
using AudioFestivalSceneWrapper = AmbientSceneWrapper<AudioFestivalScene>;
using AudioBoomboxSceneWrapper = AmbientSceneWrapper<AudioBoomboxScene>;

}  // namespace Scenes
