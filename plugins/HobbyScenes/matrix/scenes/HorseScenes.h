#pragma once

#include "HobbyScene.h"
#include "HorseDraw.h"
#include <random>
#include <vector>

namespace Hobbies {

class SunsetGallopScene final : public HobbySceneBase {
    struct Dust { float x, y, vx, vy, life; };
    std::vector<Dust> dust_;
    float gait_phase_ = 0.0f, scroll_ = 0.0f;
public:
    SunsetGallopScene();
    bool render(rgb_matrix::FrameCanvas *canvas) override;
    [[nodiscard]] std::string get_name() const override { return "horse_sunset_gallop"; }
    [[nodiscard]] Scenes::SceneDescriptor get_descriptor() const override;
};

class MistyHerdScene final : public HobbySceneBase {
    struct Member { float x, depth, phase, speed; int dir; bool graze; };
    std::vector<Member> herd_;
    float mist_ = 0.0f;
public:
    MistyHerdScene();
    void initialize(int width, int height) override;
    bool render(rgb_matrix::FrameCanvas *canvas) override;
    [[nodiscard]] std::string get_name() const override { return "horse_misty_herd"; }
    [[nodiscard]] Scenes::SceneDescriptor get_descriptor() const override;
};

class PegasusSkyScene final : public HobbySceneBase {
    struct Star { float x, y, size, phase, rate; };
    std::vector<Star> field_;
    float shoot_x_ = 0, shoot_y_ = 0, shoot_life_ = 0, next_shoot_ = 6.0f;
public:
    void initialize(int width, int height) override;
    bool render(rgb_matrix::FrameCanvas *canvas) override;
    [[nodiscard]] std::string get_name() const override { return "horse_pegasus_sky"; }
    [[nodiscard]] Scenes::SceneDescriptor get_descriptor() const override;
};

}  // namespace Hobbies
