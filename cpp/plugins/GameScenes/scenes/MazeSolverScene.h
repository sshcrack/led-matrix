#pragma once

#include "Scene.h"
#include "wrappers.h"
#include <chrono>
#include <vector>
#include <queue>
#include <set>
#include <random>

namespace Scenes {
    class MazeSolverScene : public Scene {
    private:
        // Add random number generator members
        std::mt19937 rng;

        static constexpr int CELL_SIZE = 2;  // Size of each maze cell in pixels
        float generation_delay = 0.1f;  // Slower generation speed
        float solving_delay = 0.05f;    // Slower solving speed

        struct Cell {
            bool visited = false;
            bool wall = true;
            bool in_solution = false;
            bool in_open_set = false;
            bool in_closed_set = false;
            int x, y;
            float f_score = INFINITY;
            float g_score = INFINITY;
            Cell* came_from = nullptr;
        };

        struct CellComparator {
            bool operator()(const Cell* a, const Cell* b) const {
                return a->f_score > b->f_score;
            }
        };

        std::vector<std::vector<Cell>> maze;
        Cell *start_cell = nullptr;
        Cell *end_cell = nullptr;
        bool maze_generated = false;
        bool solving_started = false;
        bool solution_found = false;
        
        std::chrono::steady_clock::time_point last_update;
        float accumulated_time = 0.0f;
        
        // A* solving state
        std::priority_queue<Cell*, std::vector<Cell*>, CellComparator> open_set;
        std::set<Cell*> open_set_lookup;

        void initializeMaze(int width, int height);
        void generateMazeStep();
        void solveMazeStep();
        std::vector<Cell*> getNeighbors(Cell* cell, bool include_walls);
        float heuristic(Cell* a, Cell* b);
        void getColorForCell(const Cell& cell, uint8_t& r, uint8_t& g, uint8_t& b);
        bool isComplete() const { return solution_found; }

        Cell* findRandomStartPosition();
        Cell* findRandomGoalPosition(int min_distance);
        
        // Add new method
        void ensurePathExists(Cell* from, Cell* to);

    public:
        explicit MazeSolverScene(const nlohmann::json &config);
        bool render(rgb_matrix::RGBMatrix *matrix) override;
        void initialize(rgb_matrix::RGBMatrix *matrix) override;
        void cleanup(rgb_matrix::RGBMatrix *matrix) override;

        [[nodiscard]] string get_name() const override;
    };

    class MazeSolverSceneWrapper : public Plugins::SceneWrapper {
        Scenes::Scene *create_default() override;
        Scenes::Scene *from_json(const nlohmann::json &args) override;
    };
}
