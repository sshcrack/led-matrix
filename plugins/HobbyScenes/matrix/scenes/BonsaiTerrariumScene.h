#pragma once

#include "HobbyScene.h"

namespace Hobbies {

// A glass terrarium with a small bonsai on a shelf against a plaster wall.
// Window light falls across the wall and the glass and follows the time of
// day (a long warm patch at dawn and dusk, a higher white one by day,
// moonlight and a string of fairy lights at night). Condensation fogs the
// glass, droplets gather and now and then run down, a snail crosses the moss
// and dust drifts in the light. The tree puts out new shoots over the hours.
class BonsaiTerrariumScene final : public HobbySceneBase {
    struct Drop {
        float x, y, r;
        float speed;  // 0 while resting on the glass
        float wobble;
    };
    struct Mote {
        float x, y, vx, vy, phase;
    };
    struct Trail {
        float x, y, age;
    };

    // Static picture: albedo and opacity change only when the tree grows;
    // the lit backdrop and light map only when the light changes.
    std::vector<float> albedo_;    // RGB
    std::vector<float> opaque_;    // 1 where something blocks the window light
    std::vector<float> light_;     // RGB light reaching each pixel
    std::vector<float> backdrop_;  // RGB lit picture
    std::vector<float> beamAir_;   // window light in the room air (dust motes)
    std::vector<float> fog_;       // condensation on the front glass, 0..1
    std::vector<float> fogTarget_;
    std::vector<uint8_t> glass_;  // 1 on the front pane, 2 on the gable pane
    std::array<float, 12> lightKey_{};
    int growthStep_ = -1;
    float growthOffset_ = 0.0f;  // where in the growth cycle this showing starts
    float condensation_ = 0.5f;  // how much the glass fogs under this light
    bool fogPrimed_ = false;
    Rgb beam_;                                    // colour of the window light
    float scale_ = 1.0f, ox_ = 0.0f, oy_ = 0.0f;  // design space (128²) to pixels

    std::vector<Drop> drops_;
    std::vector<Mote> motes_;
    std::vector<Trail> slime_;
    float spawnDebt_ = 0.0f;

    // Snail crossing the moss.
    bool snailActive_ = false;
    float snailX_ = 0.0f, snailDir_ = 1.0f, snailWait_ = 4.0f, snailPause_ = 0.0f, snailCrawl_ = 0.0f, snailEyes_ = 1.0f;

    [[nodiscard]] float px(float design) const { return design * scale_; }
    [[nodiscard]] float sx(float x) const { return ox_ + x * scale_; }
    [[nodiscard]] float sy(float y) const { return oy_ + y * scale_; }
    [[nodiscard]] float growth() const;

    void build_albedo();
    void build_light();
    void update_fog(float dt);
    void update_drops(float dt);
    void draw_drops();
    void draw_fog();
    void update_snail(float dt);
    void draw_snail();
    void draw_shoots();
    void draw_fairy_lights();
    void draw_motes(float dt);
    [[nodiscard]] Rgb lit_at(float x, float y, Rgb albedo) const;

public:
    BonsaiTerrariumScene();
    void initialize(int width, int height) override;
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    [[nodiscard]] std::string get_name() const override { return "bonsai_terrarium"; }
    [[nodiscard]] std::string get_category() const override { return "Ambient"; }
    [[nodiscard]] Scenes::SceneDescriptor get_descriptor() const override;
};

}  // namespace Hobbies
