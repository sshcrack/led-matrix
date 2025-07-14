#pragma once

#include "shared/matrix/Scene.h.h"
#include "shared/matrix/plugin/main.h"
#include <vector>
#include <random>
#include <chrono>

namespace Scenes {
    class MazeGameScene : public Scene {
    private:
        std::vector<bool> maze;
        size_t maze_size;
        std::vector<std::pair<int, int>> path;

        bool generation_complete = false;
        bool solving_complete = false;

        // Current positions for hunt and kill algorithm
        int current_x;
        int current_y;
        std::vector<std::pair<int, int>> hunt_stack;

        // Random number generation
        std::mt19937 rng;

        // Animation control
        int frame_counter = 0;
        int update_frequency = 3;
        float delay_solution_found = 5.0f;

        int scale_factor;
        int offset_x;
        int offset_y;

        void initialize_maze();

        bool hunt_and_kill_step();

        bool solve_step();

        void draw_maze();

        // A* algorithm structures
        struct Node {
            int x, y;
            float g_cost = INFINITY;
            float h_cost = 0;
            float f_cost = INFINITY;
            std::pair<int, int> parent = {-1, -1};

            [[nodiscard]] float calculate_f_cost() const {
                return g_cost + h_cost;
            }
        };

        std::vector<Node> nodes;
        std::vector<std::pair<int, int>> open_set;
        std::vector<std::pair<int, int>> closed_set;
        bool path_found = false;

        [[nodiscard]] float calculate_heuristic(int x1, int y1, int x2, int y2) const;

        [[nodiscard]] std::vector<std::pair<int, int>> get_neighbors(int x, int y) const;

    public:
        explicit MazeGameScene();
        ~MazeGameScene() override = default;

        void after_render_stop(RGBMatrixBase *matrix) override;

        bool render(RGBMatrixBase *matrix) override;

        void initialize(RGBMatrixBase *matrix, rgb_matrix::FrameCanvas *l_offscreen_canvas) override;

        [[nodiscard]] string get_name() const override;

        void register_properties() override {}

        using Scene::Scene;


        tmillis_t get_default_duration() override {
            // Will exit once maze is solved
            return 90000000;
        }

        int get_default_weight() override {
            return 1;
        }
    };

    class MazeGameSceneWrapper : public Plugins::SceneWrapper {
        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create() override;
    };
}
