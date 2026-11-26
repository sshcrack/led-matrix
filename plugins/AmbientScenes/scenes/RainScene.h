#pragma once

#include <shared/matrix/audio_kit.h>
#include <shared/matrix/utils/AmbiancePalette.h>

#include <random>
#include <vector>

#include "shared/matrix/Scene.h"
#include "shared/matrix/plugin/main.h"

namespace AmbientScenes {

/// Calm rain in depth: streaks in three distances, splashes and ripples on a
/// puddle at the bottom. Draws into an AudioKit::Buffer, so drops move and
/// fade with sub-pixel precision.
class RainScene : public Scenes::Scene {
    PropertyPointer<Plugins::EnumProperty<Ambiance::Palette>> look_palette_ =
        MAKE_ENUM_PROPERTY("look_palette", Ambiance::Palette, Ambiance::Palette::ORIGINAL);
    PropertyPointer<float> look_brightness_ = MAKE_PROPERTY_MINMAX("look_brightness", float, 1.0f, 0.05f, 1.0f);
    PropertyPointer<int> density_ = MAKE_PROPERTY_MINMAX("density", int, 150, 20, 400);
    PropertyPointer<float> speed_ = MAKE_PROPERTY_MINMAX("speed", float, 1.0f, 0.3f, 2.5f);
    PropertyPointer<float> wind_ = MAKE_PROPERTY_MINMAX("wind", float, 0.15f, -1.0f, 1.0f);
    PropertyPointer<bool> splashes_ = MAKE_PROPERTY("splashes", bool, true);

    struct Drop {
        float x = 0, y = 0, z = 0;  // z: 0 far .. 1 near
    };
    struct Droplet {
        float x = 0, y = 0, vx = 0, vy = 0, life = 0, z = 0;
    };
    struct Ripple {
        float x = 0, y = 0, age = 0, z = 0;
    };

    AudioKit::Buffer buf_;
    std::vector<Drop> drops_;
    std::vector<Droplet> droplets_;
    std::vector<Ripple> ripples_;
    std::vector<AudioKit::RGB> backdrop_;
    std::vector<float> light_;  // street-lamp light per pixel, 0..1
    AudioKit::RGB lamp_;
    std::mt19937 rng_{0x5A1Bu};
    float time_ = 0;

    float rnd() { return std::uniform_real_distribution<float>(0.0f, 1.0f)(rng_); }
    [[nodiscard]] float ground_y(float z) const;
    [[nodiscard]] AudioKit::RGB tint(float position, float brightness) const;
    void spawn(Drop& d, bool anywhere);
    void build_backdrop();
    void draw_lamp_post();
    [[nodiscard]] float light_at(float x, float y) const;

public:
    RainScene();
    void register_properties() override;
    void initialize(int width, int height) override;
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    void set_random_seed(uint32_t seed) override { rng_.seed(seed); }
    [[nodiscard]] bool supports_virtual_time() const override { return true; }
    [[nodiscard]] int remote_contract_version() const override { return 2; }
    tmillis_t get_default_duration() override { return 20000; }
    int get_default_weight() override { return 2; }
    [[nodiscard]] std::string get_name() const override { return "rain"; }
    [[nodiscard]] std::string get_category() const override { return "Ambient"; }
    [[nodiscard]] Scenes::SceneDescriptor get_descriptor() const override;
};

class RainSceneWrapper : public Plugins::SceneWrapper {
public:
    std::unique_ptr<Scenes::Scene> create() override;
};

}  // namespace AmbientScenes
