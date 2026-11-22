#pragma once

#include "HobbyScene.h"

namespace Hobbies {

// A short passenger train crossing a stone viaduct over a river. The valley
// follows the real time of day, weather and season.
class RailwayViaductScene final : public HobbySceneBase {
    struct Ripple {
        float x, y, r, life;
    };
    std::vector<float> backdrop_;  // sky, valley, viaduct; rebuilt when the look changes
    std::vector<uint8_t> sky_;     // 1 where open sky shows through the backdrop
    std::vector<uint8_t> water_;   // 1 where open water shows through the backdrop
    std::array<float, 12> backdropKey_{};
    std::vector<Ripple> ripples_;
    Environment::SeasonalDrift drift_;
    float cycle_ = 0.0f;
    void build_backdrop();
    void draw_water(float raw_dt, float front, bool train);
    void draw_train(float front);

public:
    RailwayViaductScene();
    void initialize(int width, int height) override;
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    void set_random_seed(uint32_t seed) override
    {
        HobbySceneBase::set_random_seed(seed);
        drift_.set_seed(seed + 4);
    }
    [[nodiscard]] std::string get_name() const override { return "railway_viaduct"; }
    [[nodiscard]] std::string get_category() const override { return "Ambient"; }
    [[nodiscard]] Scenes::SceneDescriptor get_descriptor() const override;
};

}  // namespace Hobbies
