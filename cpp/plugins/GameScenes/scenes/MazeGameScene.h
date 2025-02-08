#pragma once

#include "Scene.h"
#include "plugin/main.h"
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
        std::chrono::steady_clock::time_point last_update;
        float target_frame_time = 1.0f / 30.0f;
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

            float calculate_f_cost() const {
                return g_cost + h_cost;
            }
        };

        std::vector<Node> nodes;
        std::vector<std::pair<int, int>> open_set;
        std::vector<std::pair<int, int>> closed_set;
        bool path_found = false;

        float calculate_heuristic(int x1, int y1, int x2, int y2) const;

        std::vector<std::pair<int, int>> get_neighbors(int x, int y) const;

    public:
        explicit MazeGameScene();

        void after_render_stop(rgb_matrix::RGBMatrix *matrix) override;

        bool render(rgb_matrix::RGBMatrix *matrix) override;

        void initialize(rgb_matrix::RGBMatrix *matrix) override;

        [[nodiscard]] string get_name() const override;

        void register_properties() override {}

        using Scene::Scene;
    };

    class MazeGameSceneWrapper : public Plugins::SceneWrapper {
        Scenes::Scene *create() override;
    };
}
