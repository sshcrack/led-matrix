#pragma once

#include "shared/matrix/Scene.h"
#include "shared/matrix/plugin/main.h"
#include "HorseDraw.h"
#include <random>
#include <vector>

namespace HorseScenes {

// Shared plumbing: framebuffer, fps, output to the canvas.
class HorseSceneBase : public Scenes::Scene {
protected:
    Canvas cv_;
    float t_ = 0.0f;
    std::mt19937 rng_{std::random_device{}()};
    PropertyPointer<float> speed_ = MAKE_PROPERTY_MINMAX("speed", float, 1.0f, 0.2f, 2.5f);
    float rnd() { return std::uniform_real_distribution<float>(0.0f, 1.0f)(rng_); }
    void begin_frame(float &dt);
    void present(rgb_matrix::FrameCanvas *canvas);
    void fill_gradient(Rgb top, Rgb mid, Rgb bottom, float mid_at);
public:
    void initialize(int width, int height) override;
    void register_properties() override;
    [[nodiscard]] std::string get_category() const override { return "Horses"; }
    [[nodiscard]] bool supports_virtual_time() const override { return true; }
    tmillis_t get_default_duration() override { return 45000; }
    int get_default_weight() override { return 6; }
};

class SunsetGallopScene final : public HorseSceneBase {
    struct Dust { float x, y, vx, vy, life; };
    std::vector<Dust> dust_;
    float gait_phase_ = 0.0f, scroll_ = 0.0f;
public:
    bool render(rgb_matrix::FrameCanvas *canvas) override;
    [[nodiscard]] std::string get_name() const override { return "horse_sunset_gallop"; }
    [[nodiscard]] Scenes::SceneDescriptor get_descriptor() const override;
};

class MistyHerdScene final : public HorseSceneBase {
    struct Member { float x, depth, phase, speed; int dir; bool graze; };
    std::vector<Member> herd_;
    float mist_ = 0.0f;
public:
    void initialize(int width, int height) override;
    bool render(rgb_matrix::FrameCanvas *canvas) override;
    [[nodiscard]] std::string get_name() const override { return "horse_misty_herd"; }
    [[nodiscard]] Scenes::SceneDescriptor get_descriptor() const override;
};

class PegasusSkyScene final : public HorseSceneBase {
    struct Star { float x, y, size, phase, rate; };
    std::vector<Star> field_;
    float shoot_x_ = 0, shoot_y_ = 0, shoot_life_ = 0, next_shoot_ = 6.0f;
public:
    void initialize(int width, int height) override;
    bool render(rgb_matrix::FrameCanvas *canvas) override;
    [[nodiscard]] std::string get_name() const override { return "horse_pegasus_sky"; }
    [[nodiscard]] Scenes::SceneDescriptor get_descriptor() const override;
};

#define HORSE_WRAPPER(Name) \
class Name##Wrapper final : public Plugins::SceneWrapper { \
public: std::unique_ptr<Scenes::Scene> create() override { return std::make_unique<Name>(); } };
HORSE_WRAPPER(SunsetGallopScene)
HORSE_WRAPPER(MistyHerdScene)
HORSE_WRAPPER(PegasusSkyScene)
#undef HORSE_WRAPPER
}  // namespace HorseScenes
