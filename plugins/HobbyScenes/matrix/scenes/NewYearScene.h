#pragma once

#include "HobbyScene.h"

namespace Hobbies {

// How many fireworks go up. AUTO follows the clock on New Year's Eve: a few
// early rockets in the evening, a dense finale right after midnight, then it
// thins out. On any other date AUTO behaves like STEADY.
enum class NewYearShow { AUTO, STEADY, FINALE };

// New Year's Eve on a city rooftop: people watch fireworks over the skyline
// while smoke from earlier rockets drifts across the sky.
class NewYearRooftopScene final : public HobbySceneBase {
    struct Rocket {
        float x, y, vy, burstY;
        int kind;
        Rgb color, second;
        float trail;  // time until the next trail spark
    };
    struct Spark {
        float x, y, vx, vy, life, maxLife;
        Rgb color;
        float drag, gravity;
        uint8_t kind;  // 0 plain, 1 willow (leaves embers), 2 crackle (twinkles at the end), 3 ember, 4 strobe
        float trail;
    };
    struct Puff {
        float x, y, r, life, maxLife;
        Rgb tint;
    };
    struct Bloom {
        float x, y, age, r;
        Rgb color;
        float strength;
    };
    std::vector<float> backdrop_;  // sky and skyline, rebuilt when the look changes
    std::vector<uint8_t> sky_;     // 1 where open sky shows (flash and smoke paint only there)
    std::array<float, 12> backdropKey_{};
    std::vector<Rocket> rockets_;
    std::vector<Spark> sparks_;
    std::vector<Puff> smoke_;
    std::vector<Bloom> bursts_;
    float launchClock_ = 0.0f;
    float salvoLeft_ = 0.0f;
    float flash_ = 0.0f;  // combined light of the current bursts, eased
    float lull_ = 0.0f;   // pause after a salvo
    Rgb flashColor_{1, 1, 1};
    float sparkler_ = 0.0f;

    PropertyPointer<Plugins::EnumProperty<NewYearShow>> show_ = MAKE_ENUM_PROPERTY("show", NewYearShow, NewYearShow::AUTO);

    [[nodiscard]] float launch_rate() const;
    void build_backdrop();
    void launch(float speedScale);
    void burst(const Rocket& r);
    void update_fireworks(float dt);
    void draw_sky_effects();
    void draw_fireworks();
    void draw_rooftop(float dt);

public:
    NewYearRooftopScene();
    void register_properties() override;
    void initialize(int width, int height) override;
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    [[nodiscard]] std::string get_name() const override { return "new_year_rooftop"; }
    [[nodiscard]] std::string get_category() const override { return "Holidays"; }
    [[nodiscard]] Scenes::SceneDescriptor get_descriptor() const override;
};

}  // namespace Hobbies
