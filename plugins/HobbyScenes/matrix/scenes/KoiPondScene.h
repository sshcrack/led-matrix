#pragma once

#include "HobbyScene.h"

namespace Hobbies {

// A garden koi pond seen from above. Koi drift under lily pads, a maple
// overhangs one corner and the pond follows the real time of day, weather
// and season: petals in spring, flowers and dragonflies in summer, floating
// leaves in autumn, an icy rim and snow in winter, a lantern at night.
class KoiPondScene final : public HobbySceneBase {
    struct Koi {
        std::array<Point, 9> spine;  // [0] head .. tail
        float heading, speed, turn, wander;
        float size;
        int pattern;
    };
    struct Ripple {
        float x, y, r, life, strength;
    };
    struct Pad {
        float x, y, r, angle, spin;
        bool flower;
    };
    struct Floater {
        float x, y, vx, vy, angle, life;
        Rgb color;
        bool leaf;
    };
    struct Dragonfly {
        float x, y, tx, ty, wait, life;
    };
    std::vector<float> backdrop_;  // ground, stones and still water; rebuilt when the look changes
    std::vector<float> depth_;     // 0 outside the water, 0..1 towards the deep middle
    std::vector<float> canopy_;    // maple crown, rgb + alpha per pixel
    std::vector<float> ice_;       // ice cover alpha per pixel
    std::array<float, 12> backdropKey_{};
    std::vector<Koi> koi_;
    std::vector<Ripple> ripples_;
    std::vector<Pad> pads_;
    std::vector<Floater> floaters_;
    Dragonfly fly_{};
    float floatClock_ = 0.0f;
    bool seeded_ = false;

    [[nodiscard]] float rim(float angle) const;          // pond radius at an angle (design units)
    [[nodiscard]] float inside(float x, float y) const;  // >0 inside the pond, distance-like
    void build_backdrop();
    void update_koi(float dt);
    void draw_water(float dt);
    void draw_koi();
    void draw_pads();
    void draw_floaters(float dt);
    void draw_dragonfly(float dt);

public:
    KoiPondScene();
    void initialize(int width, int height) override;
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    [[nodiscard]] std::string get_name() const override { return "koi_pond"; }
    [[nodiscard]] std::string get_category() const override { return "Ambient"; }
    [[nodiscard]] Scenes::SceneDescriptor get_descriptor() const override;
};

}  // namespace Hobbies
