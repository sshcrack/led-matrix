#pragma once

#include "shared/matrix/Scene.h"
#include "shared/matrix/plugin/main.h"
#include <vector>
#include <set>
#include <random>

namespace AmbientScenes {
    class CrystalGrowthScene : public Scenes::Scene {
    private:
        struct Cell {
            int x, y;
            bool operator<(const Cell &other) const {
                return x < other.x || (x == other.x && y < other.y);
            }
            bool operator==(const Cell &other) const {
                return x == other.x && y == other.y;
            }
        };

        std::set<Cell> crystal;
        std::set<Cell> frontier;
        std::mt19937 rng;
        int growth_frame = 0;
        uint8_t base_r = 100, base_g = 200, base_b = 255;

        PropertyPointer<int> growth_speed = MAKE_PROPERTY("growth_speed", int, 5);
        PropertyPointer<bool> symmetric_growth = MAKE_PROPERTY("symmetric_growth", bool, true);

    public:
        explicit CrystalGrowthScene();
        ~CrystalGrowthScene() override = default;

        void register_properties() override;
        bool render(RGBMatrixBase *matrix) override;
        void initialize(RGBMatrixBase *matrix, rgb_matrix::FrameCanvas *l_offscreen_canvas) override;

        tmillis_t get_default_duration() override { return 60000; }
        int get_default_weight() override { return 2; }
        [[nodiscard]] std::string get_name() const override;

        using Scene::Scene;

    private:
        void grow_crystal();
        void add_to_frontier(int x, int y);
        void apply_symmetry(int x, int y, int center_x, int center_y);
    };

    class CrystalGrowthSceneWrapper : public Plugins::SceneWrapper {
        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create();
    };
}
