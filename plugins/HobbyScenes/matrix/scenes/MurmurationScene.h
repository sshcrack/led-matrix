#pragma once

#include "HobbyScene.h"

namespace Hobbies {

// A starling murmuration over a wetland at dusk. Hundreds of birds fly as one
// cloud that drifts, stretches, bends and now and then splits in two, with
// dark waves rippling through it where the birds turn. A falcon passes now
// and then and the flock tears open around it. Below: a far shore with a
// lone tree, still water that mirrors the sky and reeds in the foreground.
//
// The scene id stays "boids" (it replaced the old abstract flocking demo), so
// saved presets keep working.
class MurmurationScene final : public HobbySceneBase {
    struct Bird {
        float u, v;    // place in the flock: u along its length, v across (unit disk)
        float x, y;    // design position
        float follow;  // how quickly the bird catches up with its place (1/s)
        float wobble;  // individual jitter phase
        float size;    // splat strength
    };
    struct Falcon {
        float x = 0, y = 0, vx = 0, vy = 0;
        bool active = false;
        float wait = 25.0f;
    };

    std::vector<Bird> birds_;
    std::vector<float> backdrop_, previous_;  // cached sky, shore, water and reeds
    float fade_ = 0.0f;
    std::vector<uint8_t> sky_, water_;  // open sky / open water masks
    std::vector<float> ink_;            // bird darkness per pixel; decays into soft trails
    std::array<float, 12> backdropKey_{};
    Falcon falcon_;
    float flockTime_ = 0.0f;

    PropertyPointer<int> birdCount_ = MAKE_PROPERTY_MINMAX("birds", int, 480, 120, 800);
    PropertyPointer<bool> falconOn_ = MAKE_PROPERTY("falcon", bool, true);

    void build_backdrop();
    void reset_flock();
    void update_flock(float dt);
    void draw_birds(float raw_dt);

public:
    MurmurationScene();
    void initialize(int width, int height) override;
    void register_properties() override;
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    [[nodiscard]] std::string get_name() const override { return "boids"; }
    [[nodiscard]] std::string get_category() const override { return "Ambient"; }
    [[nodiscard]] Scenes::SceneDescriptor get_descriptor() const override;
    // 1: the abstract flocking demo. 2: the murmuration (different properties).
    [[nodiscard]] int remote_contract_version() const override { return 2; }
    tmillis_t get_default_duration() override { return 60000; }
};

}  // namespace Hobbies
