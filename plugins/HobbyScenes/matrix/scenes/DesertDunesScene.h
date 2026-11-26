#pragma once

#include "HobbyScene.h"

namespace Hobbies {

// Layered sand dunes under an open sky. Dunes show a lit and a shaded face
// from the sun or moon, wind ripples on the near slopes, sand blowing off the
// crests, heat shimmer over the horizon by day, a camel caravan now and then,
// and a large moon with dense stars at night. Stormy weather turns into a
// sandstorm.
class DesertDunesScene final : public HobbySceneBase {
    struct Grain {
        float x, y, vx, vy, life, max_life;
    };
    std::vector<float> backdrop_;  // sky and dunes; rebuilt when the look changes
    std::vector<uint8_t> sky_;     // 1 where open sky shows through the backdrop
    std::array<float, 12> backdropKey_{};
    std::vector<Grain> grains_;
    std::vector<float> row_;    // scratch row for the heat shimmer
    float caravan_ = -1000.0f;  // design x of the lead camel; -1000 while no caravan is out
    float caravanWait_ = 6.0f;
    float spawn_ = 0.0f;
    void build_backdrop();
    void draw_shimmer();
    void draw_grains(float raw_dt);
    void draw_caravan(float dt);
    void draw_sandstorm();

public:
    DesertDunesScene();
    void initialize(int width, int height) override;
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    [[nodiscard]] std::string get_name() const override { return "desert_dunes"; }
    [[nodiscard]] std::string get_category() const override { return "Ambient"; }
    [[nodiscard]] Scenes::SceneDescriptor get_descriptor() const override;
};

}  // namespace Hobbies
