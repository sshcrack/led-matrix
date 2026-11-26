#pragma once

#include "HobbyScene.h"

namespace Hobbies {

// A sea of sand dunes under an open sky, seen from a low rise. The dunes are a
// height field rendered in perspective with sun or moon light and cast
// shadows, so crests curve, slip faces hang off them and distant ridges fade
// into the haze. Sand blows off the crests, heat shimmers over the horizon by
// day, a camel caravan crosses now and then, and stormy weather turns into a
// sandstorm.
class DesertDunesScene final : public HobbySceneBase {
    struct Grain {
        float x, y, vx, vy, life, max_life;  // pixels, pixels per second
    };
    struct Crest {
        float x, y, z;  // pixel position of a crest seen against farther sand, and its distance
    };
    // A backdrop under construction. It is built a few columns per frame so a
    // change of light never stalls a frame, then crossfades in.
    struct Build {
        std::vector<float> px, skyStage;
        std::vector<float> ground, cover, skyline;
        std::vector<Crest> crests;
        float mirror = 1.0f;           // the dune field is mirrored so the light rakes across the long slopes
        float lx = 0, ly = 1, lz = 0;  // light direction in dune-field coordinates
        float tanElevation = 1.0f;
        float ambient = 0.5f, direct = 1.0f, fog = 1.0f;
        Rgb sand{}, shade{};
        int next = -1;  // next column to build; -1 while idle
    };

    std::vector<float> backdrop_, previous_;  // current and outgoing backdrop (sky and dunes)
    float fade_ = 0.0f;                       // weight of the outgoing backdrop during a crossfade
    std::vector<uint8_t> sky_;                // 1 where open sky shows through the backdrop
    std::array<float, 12> backdropKey_{};
    Build build_;
    // Per column, in pixels: the sand under the caravan's path, the top of the
    // nearer dunes that can hide it, and the skyline.
    std::vector<float> groundRow_, coverRow_, skyline_;
    std::vector<Crest> crests_;
    float windDir_ = 1.0f;  // screen direction the sand blows in
    std::vector<Grain> grains_;
    std::vector<float> row_;         // scratch row for the heat shimmer
    std::vector<float> columnTops_;  // sandstorm, per column: gust strengths, sky glow
    float caravan_ = -1000.0f;       // design x of the lead camel; -1000 while no caravan is out
    float caravanWait_ = 6.0f;
    float spawn_ = 0.0f;

    void start_build();
    void build_columns(int count);
    void finish_build();
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
