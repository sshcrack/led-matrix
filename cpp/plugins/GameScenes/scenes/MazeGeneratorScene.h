#pragma once

#include "Scene.h"
#include "plugin.h"
#include <vector>
#include <queue>
#include <chrono>
#include <random>

namespace Scenes {
    class MazeGeneratorScene : public Scene {
    private:
        enum CellState {
            WALL = 0,
            PATH = 1,
            HIGHLIGHT = 2,
            SOLUTION = 3,
            VISITED = 4,
            START = 5,
            GOAL = 6
        };

        struct Cell {
            int x, y;
            int cost = 0;    // Total score (g + h)
            int steps = 0;    // Cost from start
            Cell* parent = nullptr;
            
            bool operator>(const Cell& other) const {
                return cost > other.cost;
            }
        };

        // A* solver state
        struct SolverState {
            std::priority_queue<Cell*, std::vector<Cell*>, std::greater<>> open_set;  // Keep greater here
            std::vector<std::vector<Cell>> cells;
            bool initialized = false;
        } solver_state;

        int maze_width = 0;
        int maze_height = 0;
        std::vector<std::vector<CellState>> maze;
        std::vector<std::pair<int, int>> generation_stack;
        
        // Configuration parameters with defaults
        float step_delay = 0.0001f;      // Faster generation
        float solve_delay = 0.01f;       // Slower solving for visibility
        float final_delay = 2.0f;        // How long to show the final solution
        int wall_r = 255;                // Wall color (white default)
        int wall_g = 255;
        int wall_b = 255;
        int highlight_r = 0;             // Highlight color (red default) 
        int highlight_g = 0;
        int highlight_b = 255;
        int solution_r = 0;              // Solution path color (green default)
        int solution_g = 255;
        int solution_b = 0;
        int visited_r = 64;              // Visited nodes color (dim purple default)
        int visited_g = 0;
        int visited_b = 64;
        uint8_t start_r, start_g, start_b;
        uint8_t goal_r, goal_g, goal_b;

        bool generation_complete = false;
        bool solving_complete = false;
        bool solution_found = false;
        float accumulated_time = 0;
        float completion_time = 0;

        std::chrono::steady_clock::time_point last_update;
        std::mt19937 rng;

        void initializeMaze();
        bool generateStep();
        void initializeSolver();
        bool solveStep();
        void after_render_stop(rgb_matrix::RGBMatrix *matrix) override;
        int calculateDistance(int x1, int y1, int x2, int y2);
        void getNeighbors(int x, int y, std::vector<std::pair<int, int>>& neighbors);
        void updateCanvas(rgb_matrix::Canvas* canvas);

    public:
        explicit MazeGeneratorScene(const nlohmann::json &config);
        bool render(rgb_matrix::RGBMatrix *matrix) override;
        void initialize(rgb_matrix::RGBMatrix *matrix) override;
        [[nodiscard]] string get_name() const override;
        using Scene::Scene;
    };

    class MazeGeneratorSceneWrapper : public Plugins::SceneWrapper {
        Scenes::Scene *create_default() override;
        Scenes::Scene *from_json(const nlohmann::json &args) override;
    };
}
