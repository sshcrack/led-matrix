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
//   hike_cloud_shadows  - a green hill at midday with a switchback trail; small
//                         hikers climb it, sheep graze and cloud shadows sweep
//                         across. Clouds drift faster with the energy, sheep
//                         hop on beats.

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
    void initialize(int width, int height) override;
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    [[nodiscard]] std::string get_name() const override { return "hike_summit_night"; }
    [[nodiscard]] Scenes::SceneDescriptor get_descriptor() const override;
};

class CloudShadowsScene final : public AudioHobbyScene {
    struct Walker {
        float s, speed, phase, rest;
        Rgb jacket;
    };
    struct Sheep {
        float x, y, dir, phase, hop;
    };
    std::vector<Point> trail_;     // switchback polyline, bottom to top
    std::vector<float> trailLen_;  // cumulative length
    std::vector<Walker> walkers_;
    float summitS_ = 0.0f;
    std::vector<Sheep> sheep_;
    std::vector<float> ground_;    // cached lit landscape
    std::vector<uint8_t> isLand_;  // 1 where cloud shadows may fall
    float drift_ = 0.0f;
    void build_world();
    [[nodiscard]] Point trail_at(float s) const;
    [[nodiscard]] float hill_top(float x) const;

public:
    void initialize(int width, int height) override;
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    [[nodiscard]] std::string get_name() const override { return "hike_cloud_shadows"; }
    [[nodiscard]] Scenes::SceneDescriptor get_descriptor() const override;
};

}  // namespace Hobbies
