#pragma once

#include "HobbyScene.h"

namespace Hobbies {

// An allotment at the edge of the fields through the year: vegetable beds,
// sunflowers along the fence, a garden shed and a scarecrow, with a field
// beyond that is ploughed, green, golden and frosted in turn. Crows land now
// and then and a small tractor crosses the far field. Follows the real time
// of day, weather and season.
class AllotmentScene final : public HobbySceneBase {
    struct Crow {
        float x, y, tx, ty;
        float timer, phase;
        int state;  // 0 away, 1 flying in, 2 on the ground or a perch, 3 flying off
        bool left;  // facing left
    };
    struct Puff {
        float x, y, age;
    };
    std::vector<float> backdrop_;  // sky, field, shed, beds and crops; rebuilt when the look changes
    std::vector<uint8_t> sky_;     // 1 where open sky shows (clouds paint there)
    std::vector<uint8_t> field_;   // 1 where the far field shows (the tractor drives there)
    std::array<float, 12> backdropKey_{};
    std::array<Crow, 3> crows_{};
    float crowWait_ = 0.0f;
    float tractor_ = 0.0f;  // seconds into the tractor's cycle
    std::vector<Puff> smoke_;
    float smokeClock_ = 0.0f;
    std::vector<float> nearRows_;  // the foreground before the fog, to thin it there
    Environment::SeasonalDrift drift_;

    void build_backdrop();
    void draw_scarecrow(float alpha = 1.0f);
    void draw_sunflowers();
    void draw_tractor(float dt);
    void update_crows(float dt);
    void draw_crow(const Crow& c);
    void draw_smoke(float dt);

public:
    AllotmentScene();
    void initialize(int width, int height) override;
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    [[nodiscard]] std::string get_name() const override { return "allotment"; }
    [[nodiscard]] std::string get_category() const override { return "Ambient"; }
    [[nodiscard]] Scenes::SceneDescriptor get_descriptor() const override;
    [[nodiscard]] Previews::SceneSpec get_preview_spec() const override
    {
        return Previews::SceneSpec::with_inputs({Previews::Inputs::Audio});
    }
};

}  // namespace Hobbies
