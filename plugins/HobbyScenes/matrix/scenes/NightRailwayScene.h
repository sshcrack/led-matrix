#pragma once

#include "HobbyScene.h"

namespace Hobbies {

class NightRailwayScene final : public HobbySceneBase {
    std::vector<float> backdrop_;
    void draw_landscape();
    void draw_train(float front);

public:
    void initialize(int width, int height) override;
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    [[nodiscard]] std::string get_name() const override { return "night_railway"; }
    [[nodiscard]] std::string get_category() const override { return "Ambient"; }
    [[nodiscard]] Scenes::SceneDescriptor get_descriptor() const override;
};

}  // namespace Hobbies
