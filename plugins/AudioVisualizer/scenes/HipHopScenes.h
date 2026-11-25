#pragma once

// Hip hop showcase places (style:hiphop):
//
//   audio_graffiti_wall - a night wall where an abstract piece is sprayed
//                         stroke by stroke; the can bursts on beats, paint
//                         drips on snares, the piece is buffed and redone
//   audio_lowrider      - a candy-painted lowrider cruising a palm-lined
//                         boulevard at dusk; the hydraulics hop on the bass
//   audio_block_party   - a street corner at dusk: speaker stack, DJ table,
//                         string lights, an open hydrant and a bobbing crowd
//   audio_turntable     - a DJ booth from above: the record spins, a hand
//                         scratches on onsets, the other cuts the crossfader

#include <vector>

#include "kit/AmbientMusicScene.h"
#include "kit/GenreVistaKit.h"

namespace Scenes {

// --------------------------------------------------------------------------
class AudioGraffitiWallScene final : public AmbientMusicScene {
public:
    struct Point {
        float x, y;
    };
    enum class Layer : uint8_t { Cloud, Fill, Shadow, Outline, Highlight };
    struct Stroke {
        std::vector<Point> pts;
        Layer layer;
        float radius;
    };

private:
    struct Capsule {
        float ax, ay, bx, by, r;
    };
    struct Drip {
        float x, y, length, max, speed;
        AudioKit::RGB color;
    };
    struct Palette {
        AudioKit::RGB fillA, fillB, outline, shadow, cloud;
    };
    enum class Phase : uint8_t { Buff, Paint, Hold };

    std::vector<AudioKit::RGB> wall_;  // lit bricks (static)
    std::vector<float> light_;         // lamp light on wall and pavement
    std::vector<float> texture_;       // brick relief the paint follows
    std::vector<AudioKit::RGB> paint_;
    std::vector<float> alpha_;
    std::vector<float> dist_;    // signed distance to the piece (negative inside)
    std::vector<float> shadow_;  // the same field shifted for the 3D block
    std::vector<float> cloud_;   // bubbly radius of the background cloud
    std::vector<Capsule> capsules_;
    std::vector<Stroke> strokes_;
    std::vector<Drip> drips_;
    std::vector<Point> stars_;
    Palette colors_{};
    int paletteIndex_ = 0;
    int top_ = 0, bottom_ = 0, left_ = 0, right_ = 0;                  // piece bounds
    int buffTop_ = 0, buffBottom_ = 0, buffLeft_ = 0, buffRight_ = 0;  // patch the roller covers
    Phase phase_ = Phase::Paint;
    size_t stroke_ = 0, point_ = 0;
    float sub_ = 0.0f;  // distance carried into the next step
    float canX_ = 64.0f, canY_ = 60.0f;
    bool traveling_ = false;
    float hold_ = 0.0f, roller_ = 0.0f, shake_ = 0.0f;
    AudioKit::Envelope beat_, spray_, hat_, drop_;
    PropertyPointer<float> paintSpeed_ = MAKE_PROPERTY_MINMAX("paint_speed", float, 1.0f, 0.3f, 3.0f);

    void init_world();
    void new_piece();
    void build_strokes();
    void spray(float x, float y, float radius, Layer layer, float strength);
    [[nodiscard]] bool clip(size_t i, Layer layer) const;
    [[nodiscard]] AudioKit::RGB layer_color(int x, int y, Layer layer) const;
    void advance(float distance);

public:
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    void register_properties() override;
    std::string get_name() const override { return "audio_graffiti_wall"; }
    [[nodiscard]] SceneDescriptor get_descriptor() const override;
};

// --------------------------------------------------------------------------
class AudioLowriderScene final : public AmbientMusicScene {
    struct Axle {
        float lift = 0.0f, vel = 0.0f;    // body height over the axle, design px (up is positive)
        float air = 0.0f, airVel = 0.0f;  // wheel off the ground after a big hop
    };
    struct Spark {
        float x, y, vx, vy, life;
    };
    Axle front_, rear_;
    std::vector<Spark> sparks_;
    std::vector<AudioKit::RGB> sky_;  // cached sky and far skyline
    float scrollFar_ = 0.0f, scrollMid_ = 0.0f, scrollRoad_ = 0.0f, wheelAngle_ = 0.0f;
    int beatCount_ = 0;
    AudioKit::Envelope beat_, kick_, drop_, glowPulse_;
    AudioKit::HueCycle paint_{{330.0f, 275.0f, 200.0f, 20.0f, 160.0f}};
    PropertyPointer<float> bounce_ = MAKE_PROPERTY_MINMAX("hop_strength", float, 1.0f, 0.2f, 2.0f);
    PropertyPointer<bool> underglow_ = MAKE_PROPERTY("underglow", bool, true);

    void init_world();
    void hit(Axle& a, float amount);
    void step(Axle& a, float target, float dt, float x);

public:
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    void register_properties() override;
    std::string get_name() const override { return "audio_lowrider"; }
    [[nodiscard]] SceneDescriptor get_descriptor() const override;
};

// --------------------------------------------------------------------------
class AudioBlockPartyScene final : public AmbientMusicScene {
    struct Person {
        float x, phase, scale, width;
        int row;    // 0 back, 1 front
        int style;  // 0 nod, 1 bounce, 2 arms up on drops, 3 sway
    };
    struct Drop {
        float x, y, vx, vy, life;
    };
    std::vector<AudioKit::RGB> backdrop_;
    std::vector<uint8_t> crowd_;  // silhouette mask, rebuilt every frame
    std::vector<Person> people_;
    std::vector<Drop> water_;
    float cone_ = 0.0f, coneVel_ = 0.0f, waterClock_ = 0.0f, tv_ = 0.0f, pumpClock_ = 0.0f;
    AudioKit::Envelope beat_, kick_, snare_, drop_, offbeat_;
    AudioKit::HueCycle palette_{{30.0f, 330.0f, 190.0f, 280.0f, 50.0f}};
    PropertyPointer<bool> hydrant_ = MAKE_PROPERTY("open_hydrant", bool, true);

    void init_world();
    void draw_person(const Person& p, float bob, int arms);

public:
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    void register_properties() override;
    std::string get_name() const override { return "audio_block_party"; }
    [[nodiscard]] SceneDescriptor get_descriptor() const override;
};

// --------------------------------------------------------------------------
class AudioTurntableScene final : public AmbientMusicScene {
    std::vector<AudioKit::RGB> booth_;   // static booth, plinth and mixer
    std::vector<float> radius_, angle_;  // polar coordinates around the platter
    std::vector<float> sheen_;           // static highlight on the vinyl
    std::vector<float> vu_;
    float recordAngle_ = 0.0f;
    float scratch_ = 1.0f;  // 0..1 progress of the current scratch, 1 = idle
    float scratchAmp_ = 0.0f;
    float handOffset_ = 0.0f;  // along the groove, design px
    float fader_ = 0.5f, faderTarget_ = 0.5f;
    float sweep_ = 0.0f;
    int cut_ = 0;
    AudioKit::Envelope beat_, kick_, drop_, press_;
    AudioKit::HueCycle palette_{{200.0f, 320.0f, 40.0f, 270.0f, 160.0f}};
    PropertyPointer<bool> scratching_ = MAKE_PROPERTY("scratching", bool, true);

    void init_world();
    void draw_hand(float x, float y, float angle, float lift, float scale);

public:
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    void register_properties() override;
    std::string get_name() const override { return "audio_turntable"; }
    [[nodiscard]] SceneDescriptor get_descriptor() const override;
};

using AudioGraffitiWallSceneWrapper = AmbientSceneWrapper<AudioGraffitiWallScene>;
using AudioLowriderSceneWrapper = AmbientSceneWrapper<AudioLowriderScene>;
using AudioBlockPartySceneWrapper = AmbientSceneWrapper<AudioBlockPartyScene>;
using AudioTurntableSceneWrapper = AmbientSceneWrapper<AudioTurntableScene>;

}  // namespace Scenes
