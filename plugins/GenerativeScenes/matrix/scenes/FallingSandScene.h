#pragma once

#include "shared/matrix/Scene.h"
#include <random>
#include <vector>

namespace GenerativeScenes {
class FallingSandScene final : public Scenes::Scene {
    struct Cell { uint8_t type=0, hue=0; };
    std::vector<Cell> cells_;
    std::mt19937 rng_{std::random_device{}()};
    uint32_t frame_=0;

    PropertyPointer<int> emitters_ = MAKE_PROPERTY("emitters", int, 5);
    PropertyPointer<int> spawn_rate_ = MAKE_PROPERTY("spawn_rate", int, 12);
    PropertyPointer<bool> water_ = MAKE_PROPERTY("water", bool, true);
    PropertyPointer<int> reset_fill_percent_ = MAKE_PROPERTY("reset_fill_percent", int, 72);

    int index(int x,int y) const { return y*matrix_width+x; }
    void reset();

public:
    void initialize(int width, int height) override;
    bool render(rgb_matrix::FrameCanvas *canvas) override;
    void register_properties() override;
    [[nodiscard]] std::string get_name() const override { return "falling_sand"; }
    [[nodiscard]] std::string get_category() const override { return "Generative"; }
    tmillis_t get_default_duration() override { return 30000; }
    int get_default_weight() override { return 6; }
};

class FallingSandSceneWrapper final : public Plugins::SceneWrapper {
    std::unique_ptr<Scenes::Scene> create() override;
};
}
