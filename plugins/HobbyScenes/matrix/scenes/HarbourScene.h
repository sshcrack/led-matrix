#pragma once

#include "HobbyScene.h"

namespace Hobbies {

// A small fishing harbour at first light: a row of quay houses, moored boats
// rocking, a lighthouse on the pier, gulls, and now and then a boat heading
// out to sea. Morning mist lifts off the water; the harbour follows the real
// time of day, weather and season.
class HarbourScene final : public HobbySceneBase {
    struct Gull {
        float x, y, vx, phase, glide;
    };
    struct Ripple {
        float x, y, life;
    };
    std::vector<float> backdrop_;  // sky, coast, houses, quay, pier; rebuilt when the look changes
    std::vector<uint8_t> sky_;     // 1 where open sky shows (clouds paint there)
    std::vector<uint8_t> water_;   // 1 where open water shows
    std::vector<uint8_t> pier_;    // 1 on the pier (hides a far boat behind it)
    std::array<float, 12> backdropKey_{};
    std::vector<Gull> gulls_;
    std::vector<Ripple> ripples_;
    float voyage_ = 0.0f;  // seconds into the departure cycle
    float beam_ = 0.0f;    // lighthouse beam angle

    void build_backdrop();
    [[nodiscard]] Rgb boat_color(Rgb albedo) const;
    void draw_water(float raw_dt);
    void draw_boat(float x, float waterline, float scale, float face, float roll, Rgb hull, Rgb trim, bool lights,
                   const std::vector<uint8_t>* mask);
    void draw_boats(float dt);
    void draw_gulls(float dt);
    void draw_lighthouse();

public:
    HarbourScene();
    void initialize(int width, int height) override;
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    [[nodiscard]] std::string get_name() const override { return "harbour_dawn"; }
    [[nodiscard]] std::string get_category() const override { return "Ambient"; }
    [[nodiscard]] Scenes::SceneDescriptor get_descriptor() const override;
};

}  // namespace Hobbies
