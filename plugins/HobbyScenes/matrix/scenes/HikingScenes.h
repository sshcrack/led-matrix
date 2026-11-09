#pragma once
// Hiking and hills. All three are calm landscapes first; with music playing
// (audio_reactive, on by default) they pick up the beat:
//
//   hike_ridge_trail    - golden-hour side-scroller; a hiker walks a ridge path
//                         through parallax hills. Steps lock to the tempo, the
//                         sun pulses on beats, flowers glow with the treble and
//                         a flock of birds passes on drops.
//   hike_summit_night   - a hiker sits by a lit tent on a summit above a misty
//                         valley. The lantern breathes with the bass, village
//                         lights twinkle with the treble, aurora follows the
//                         mids, drops send shooting stars.
//   hike_alpine_lake    - a hiker rests on a boulder by a still mountain lake;
//                         snowy peaks and pines mirror in the water, mist
//                         drifts and now and then a skipped stone leaves rings.
//                         Treble shimmers the reflection, beats bring fish
//                         rings, drops make the hiker skip a stone.

#include <array>
#include <vector>

#include "FigureDraw.h"
#include "HobbyScene.h"

namespace Hobbies {

class RidgeTrailScene final : public AudioHobbyScene {
    struct Bird {
        float x, y, phase, speed;
    };
    std::vector<Bird> birds_;
    float scroll_ = 0.0f, walk_ = 0.0f, nextFlock_ = 20.0f;
    void spawn_flock(float W, float H, float k);

public:
    RidgeTrailScene();
    void initialize(int width, int height) override;
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    [[nodiscard]] std::string get_name() const override { return "hike_ridge_trail"; }
    [[nodiscard]] Scenes::SceneDescriptor get_descriptor() const override;
};

class SummitNightScene final : public AudioHobbyScene {
    struct Star {
        float x, y, size, phase, rate;
    };
    struct Light {
        float x, y, phase, hue;
    };
    std::vector<Star> stars_;
    std::vector<Light> village_;
    float shootX_ = 0, shootY_ = 0, shootLife_ = 0, nextShoot_ = 7.0f;

public:
    SummitNightScene();
    void initialize(int width, int height) override;
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    [[nodiscard]] std::string get_name() const override { return "hike_summit_night"; }
    [[nodiscard]] Scenes::SceneDescriptor get_descriptor() const override;
};

class AlpineLakeScene final : public AudioHobbyScene {
    struct Ripple {
        float x, y, r, life;
    };
    struct Stone {
        std::vector<Point> hops;  // touchdown points, first is the hand
        float t = -1.0f;          // position along hops (segment index + fraction); <0 idle
    };
    std::vector<float> backdrop_;  // sky, peaks and far shore above the waterline
    std::vector<Ripple> ripples_;
    Stone stone_;
    float throwT_ = -1.0f, nextThrow_ = 14.0f, mist_ = 0.0f, nextFish_ = 5.0f;
    float horizon_ = 0.0f;
    std::array<float, 12> backdropKey_{};
    void build_backdrop();
    void start_throw();

public:
    AlpineLakeScene();
    void initialize(int width, int height) override;
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    [[nodiscard]] std::string get_name() const override { return "hike_alpine_lake"; }
    [[nodiscard]] Scenes::SceneDescriptor get_descriptor() const override;
};

}  // namespace Hobbies
