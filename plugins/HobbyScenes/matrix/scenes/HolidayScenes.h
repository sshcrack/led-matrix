#pragma once
// Christmas scenes. Tagged "occasion:christmas", so Automatic Mode plays them
// only while Christmas is on (1-26 December by the calendar, or forced in the
// web UI); in Scene Lab and presets they are always available. Both are winter
// nights that follow the real weather (snowfall, fog, a clear starry sky).
//
//   christmas_market     - a market square in an old town: wooden stalls with
//                          garlands, a big lit tree with a star, people strolling
//                          with mulled wine, steam, lights strung over the
//                          square. With music the lights pulse on the beat.
//   christmas_sleigh     - a snowy village under the moon; lit windows and
//                          chimney smoke, and now and then a sleigh pulled by
//                          reindeer crosses the sky in front of the moon.

#include <array>
#include <vector>

#include "FigureDraw.h"
#include "HobbyScene.h"

namespace Hobbies {

class ChristmasMarketScene final : public AudioHobbyScene {
    struct Stroller {
        float x, speed, phase, scale;
        float dir;
        int look;
        float pause;  // > 0: stopped at a stall
    };
    struct Puff {
        float x, y, r, life;
    };
    std::vector<Stroller> people_;
    std::vector<Puff> steam_;
    std::vector<float> backdrop_;
    std::array<float, 12> backdropKey_{};
    float horizon_ = 0.0f, steamCarry_ = 0.0f;
    void build_backdrop();

public:
    ChristmasMarketScene();
    void initialize(int width, int height) override;
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    [[nodiscard]] std::string get_name() const override { return "christmas_market"; }
    [[nodiscard]] std::string get_category() const override { return "Ambient"; }
    [[nodiscard]] Scenes::SceneDescriptor get_descriptor() const override;
};

class ChristmasSleighScene final : public HobbySceneBase {
    struct Puff {
        float x, y, r, life;
    };
    struct Spark {
        float x, y, life;
    };
    std::vector<Puff> smoke_;
    std::vector<Spark> trail_;
    std::vector<float> backdrop_;
    std::array<float, 12> backdropKey_{};
    float horizon_ = 0.0f, smokeCarry_ = 0.0f;
    float sleighT_ = 0.35f;     // 0..1 across the sky, < 0 waiting
    float sleighWait_ = 0.0f;
    void build_backdrop();

public:
    ChristmasSleighScene();
    void initialize(int width, int height) override;
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    [[nodiscard]] std::string get_name() const override { return "christmas_sleigh"; }
    [[nodiscard]] std::string get_category() const override { return "Ambient"; }
    [[nodiscard]] Scenes::SceneDescriptor get_descriptor() const override;
};

}  // namespace Hobbies
