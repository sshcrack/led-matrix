#pragma once
// Outdoor activities that follow the real time of day, weather and season:
//
//   activity_cycling - a cyclist rides a coast road past the sea, dunes and a
//                      far lighthouse. Lights come on after dark, a rain
//                      jacket in the rain, flowers or snow on the verge.
//   activity_skiing  - a skier carves down a slope past pines while a
//                      chairlift climbs behind. Always winter; follows the
//                      time of day (floodlit slope at night) and the weather.

#include <vector>

#include "FigureDraw.h"
#include "HobbyScene.h"

namespace Hobbies {

class CyclingScene final : public AudioHobbyScene {
    struct Gull {
        float x, y, phase, speed;
    };
    std::vector<Gull> gulls_;
    float scroll_ = 0.0f, wheel_ = 0.0f, crank_ = 0.0f, nextGulls_ = 4.0f;

public:
    CyclingScene();
    void initialize(int width, int height) override;
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    [[nodiscard]] std::string get_name() const override { return "activity_cycling"; }
    [[nodiscard]] Scenes::SceneDescriptor get_descriptor() const override;
};

class SkiingScene final : public AudioHobbyScene {
    struct Flake {
        float x, y, vx, vy, life;
    };
    std::vector<Flake> spray_;
    float scroll_ = 0.0f, lift_ = 0.0f, jumpT_ = -1.0f, nextJump_ = 5.0f, carve_ = 0.0f;
    float ground(float x) const;

public:
    SkiingScene();
    void initialize(int width, int height) override;
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    [[nodiscard]] std::string get_name() const override { return "activity_skiing"; }
    [[nodiscard]] Scenes::SceneDescriptor get_descriptor() const override;
};

}  // namespace Hobbies
