#include "MazeSolverScene.h"
#include <random>
#include <algorithm>
#include <spdlog/spdlog.h>

using namespace Scenes;

MazeSolverScene::MazeSolverScene(const nlohmann::json &config) : Scene(config) {
    // Initialize RNG
    std::random_device rd;
    rng.seed(rd());
    
    generation_delay = config.value("generation_delay", 0.1f);  // Slower default
    solving_delay = config.value("solving_delay", 0.05f);      // Slower default
}

void MazeSolverScene::cleanup(rgb_matrix::RGBMatrix *matrix) {
    // Clear maze data
    for (auto &row: maze) {
        row.clear();
    }
    maze.clear();

    // Reset pointers
    start_cell = nullptr;
    end_cell = nullptr;

    // Clear A* solving state
    while (!open_set.empty()) {
        open_set.pop();
    }
    open_set_lookup.clear();

    // Reset state flags
    solving_started = false;
    solution_found = false;
    accumulated_time = 0.0f;

    MazeSolverScene::initialize(matrix);
}

void MazeSolverScene::initialize(rgb_matrix::RGBMatrix *matrix) {
    Scene::initialize(matrix);
    last_update = std::chrono::steady_clock::now();
    accumulated_time = 0;
    solving_started = false;
    solution_found = false;

    // Calculate maze dimensions based on matrix size and cell size
    int maze_width = matrix->width() / CELL_SIZE;
    int maze_height = matrix->height() / CELL_SIZE;
    spdlog::debug("Initializing maze with dimensions {}x{}", maze_width, maze_height);

    initializeMaze(maze_width, maze_height);

    spdlog::debug("Finding random start position");
    start_cell = findRandomStartPosition();
    spdlog::debug("Start position set to ({}, {})", start_cell->x, start_cell->y);
    
    int min_distance = std::max(maze_width, maze_height) / 3;
    spdlog::debug("Finding goal position with min distance {}", min_distance);
    end_cell = findRandomGoalPosition(min_distance);
    spdlog::debug("Goal position set to ({}, {})", end_cell->x, end_cell->y);
    
    // Initialize cells and create path
    start_cell->wall = false;
    end_cell->wall = false;
    ensurePathExists(start_cell, end_cell);
    
    // Start solving immediately
    maze_generated = true;
}

void MazeSolverScene::initializeMaze(int width, int height) {
    maze.resize(height);
    for (int y = 0; y < height; y++) {
        maze[y].resize(width);
        for (int x = 0; x < width; x++) {
            maze[y][x].x = x;
            maze[y][x].y = y;
        }
    }
}

void MazeSolverScene::ensurePathExists(Cell* from, Cell* to) {
    spdlog::debug("Creating path from ({},{}) to ({},{})", 
                  from->x, from->y, to->x, to->y);
                  
    // Simple method: create straight paths
    int x = from->x;
    int y = from->y;
    
    // Move horizontally first
    while (x != to->x) {
        maze[y][x].wall = false;
        x += (x < to->x) ? 1 : -1;
    }
    
    // Then vertically
    while (y != to->y) {
        maze[y][x].wall = false;
        y += (y < to->y) ? 1 : -1;
    }
    
    // Add some random non-wall cells around the path for variety
    for (int i = 0; i < maze.size() * maze[0].size() / 4; i++) {
        int rx = std::uniform_int_distribution<>(1, maze[0].size()-2)(rng);
        int ry = std::uniform_int_distribution<>(1, maze.size()-2)(rng);
        spdlog::debug("Adding random cell at ({},{})", rx, ry);
        maze[ry][rx].wall = false;
    }
}

void MazeSolverScene::solveMazeStep() {
    if (!solving_started) {
        spdlog::debug("Starting maze solving from ({}, {})", start_cell->x, start_cell->y);
        start_cell->g_score = 0;
        start_cell->f_score = heuristic(start_cell, end_cell);
        open_set.push(start_cell);
        open_set_lookup.insert(start_cell);
        solving_started = true;
        return;
    }

    if (open_set.empty()) {
        spdlog::debug("No solution found - open set is empty");
        solution_found = true;
        return;
    }

    Cell *current = open_set.top();
    spdlog::debug("Exploring node ({}, {}) with f_score {}", current->x, current->y, current->f_score);
    open_set.pop();
    open_set_lookup.erase(current);

    if (current == end_cell) {
        spdlog::debug("Solution found!");
        // Reconstruct path
        Cell *path = current;
        while (path != nullptr) {
            path->in_solution = true;
            path = path->came_from;
        }
        solution_found = true;
        return;
    }

    current->in_closed_set = true;

    auto neighbors = getNeighbors(current, false);
    spdlog::debug("Found {} valid neighbors for solving", neighbors.size());

    for (Cell *neighbor: neighbors) {
        if (neighbor->in_closed_set || neighbor->wall) continue;

        float tentative_g_score = current->g_score + 1;

        if (tentative_g_score < neighbor->g_score) {
            spdlog::debug("Better path found to ({}, {}) with g_score {}", 
                         neighbor->x, neighbor->y, tentative_g_score);
            neighbor->came_from = current;
            neighbor->g_score = tentative_g_score;
            neighbor->f_score = tentative_g_score + heuristic(neighbor, end_cell);

            if (open_set_lookup.find(neighbor) == open_set_lookup.end()) {
                neighbor->in_open_set = true;
                open_set.push(neighbor);
                open_set_lookup.insert(neighbor);
            }
        }
    }
}

float MazeSolverScene::heuristic(Cell *a, Cell *b) {
    return std::abs(a->x - b->x) + std::abs(a->y - b->y);
}

std::vector<MazeSolverScene::Cell *> MazeSolverScene::getNeighbors(Cell *cell, bool include_walls) {
    std::vector<Cell *> neighbors;
    
    // Use different deltas for generation vs pathfinding
    int dx[4], dy[4];
    if (include_walls) {
        // For maze generation: move 2 cells at a time
        dx[0] = 0; dx[1] = 0; dx[2] = -2; dx[3] = 2;
        dy[0] = -2; dy[1] = 2; dy[2] = 0; dy[3] = 0;
    } else {
        // For pathfinding: move 1 cell at a time
        dx[0] = 0; dx[1] = 0; dx[2] = -1; dx[3] = 1;
        dy[0] = -1; dy[1] = 1; dy[2] = 0; dy[3] = 0;
    }

    for (int i = 0; i < 4; i++) {
        int new_x = cell->x + dx[i];
        int new_y = cell->y + dy[i];

        if (new_x >= 0 && new_x < maze[0].size() && 
            new_y >= 0 && new_y < maze.size()) {
            
            if (include_walls) {
                // For maze generation
                neighbors.push_back(&maze[new_y][new_x]);
            } else {
                // For pathfinding: check if the path is clear
                // Don't add neighbor if it's a wall
                if (!maze[new_y][new_x].wall) {
                    neighbors.push_back(&maze[new_y][new_x]);
                }
            }
        }
    }
    return neighbors;
}

void MazeSolverScene::getColorForCell(const Cell& cell, uint8_t& r, uint8_t& g, uint8_t& b) {
    if (cell.wall) {
        r = g = b = 255;  // Walls are always white
        return;
    }
    
    if (!maze_generated) {
        // During generation, show the process
        r = g = b = (cell.visited ? 128 : 0);  // Gray for visited, black for unvisited
    } else if (cell.in_solution) {
        // Solution path in bright cyan
        r = 0; g = 255; b = 255;
    } else if (&cell == start_cell) {
        r = 0; g = 255; b = 0;  // Green start
    } else if (&cell == end_cell) {
        r = 255; g = 0; b = 255;  // Purple end
    } else {
        r = g = b = 0;  // Black for empty spaces after generation
    }
}

bool MazeSolverScene::render(rgb_matrix::RGBMatrix *matrix) {
    auto current_time = std::chrono::steady_clock::now();
    float delta_time = std::chrono::duration<float>(current_time - last_update).count();
    last_update = current_time;
    accumulated_time += delta_time;

    // Update maze state - now only solving
    if (!solution_found && accumulated_time >= solving_delay) {
        spdlog::debug("Solving step with accumulated time {:.3f}", accumulated_time);
        solveMazeStep();
        accumulated_time = 0;
    } else if (solution_found) {
        // Add 3 second delay after solution is found before ending
        if (accumulated_time >= 3.0f) {
            return false;
        }
    }

    // Render maze
    offscreen_canvas->Fill(0, 0, 0);

    // First pass: Draw non-wall cells
    for (int y = 0; y < maze.size(); y++) {
        for (int x = 0; x < maze[y].size(); x++) {
            if (!maze[y][x].wall) {
                uint8_t r, g, b;
                getColorForCell(maze[y][x], r, g, b);
                for (int dy = 0; dy < CELL_SIZE; dy++) {
                    for (int dx = 0; dx < CELL_SIZE; dx++) {
                        offscreen_canvas->SetPixel(x * CELL_SIZE + dx, y * CELL_SIZE + dy, r, g, b);
                    }
                }
            }
        }
    }

    // Second pass: Draw walls (always on top)
    for (int y = 0; y < maze.size(); y++) {
        for (int x = 0; x < maze[y].size(); x++) {
            if (maze[y][x].wall) {
                uint8_t r, g, b;
                getColorForCell(maze[y][x], r, g, b);
                for (int dy = 0; dy < CELL_SIZE; dy++) {
                    for (int dx = 0; dx < CELL_SIZE; dx++) {
                        offscreen_canvas->SetPixel(x * CELL_SIZE + dx, y * CELL_SIZE + dy, r, g, b);
                    }
                }
            }
        }
    }

    offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas);
    return true;
}

string MazeSolverScene::get_name() const {
    return "maze_solver";
}

Scene *MazeSolverSceneWrapper::create_default() {
    return new MazeSolverScene(Scene::create_default(1, 30 * 1000));
}

Scene *MazeSolverSceneWrapper::from_json(const nlohmann::json &args) {
    return new MazeSolverScene(args);
}

MazeSolverScene::Cell* MazeSolverScene::findRandomGoalPosition(int min_distance) {
    int height = maze.size();
    int width = maze[0].size();
    
    // Generate list of valid positions
    std::vector<Cell*> valid_positions;
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Check if position is far enough from start
            int distance = std::abs(x - start_cell->x) + std::abs(y - start_cell->y);
            if (distance >= min_distance) {
                valid_positions.push_back(&maze[y][x]);
            }
        }
    }

    spdlog::debug("Found {} valid positions", valid_positions.size());
    if (valid_positions.empty()) {
        // Fallback to corner if no valid position found
        return &maze[height - 2][width - 2];
    }
    
    // Pick random position from valid ones
    std::uniform_int_distribution<> dis(0, valid_positions.size() - 1);
    spdlog::debug("Accessing array for random stuff in goal");
    return valid_positions[dis(rng)];
}

MazeSolverScene::Cell* MazeSolverScene::findRandomStartPosition() {
    int height = maze.size();
    int width = maze[0].size();
    
    // Generate list of valid positions
    std::vector<Cell*> valid_positions;
    
    // Keep start position away from edges for better mazes
    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            valid_positions.push_back(&maze[y][x]);
        }
    }

    spdlog::debug("Found {} valid positions", valid_positions.size());
    if (valid_positions.empty()) {
        // Fallback to a safe position if no valid positions found
        auto* cell = &maze[1][1];
        cell->wall = false;
        return cell;
    }
    
    // Pick random position from valid ones
    std::uniform_int_distribution<> dis(0, valid_positions.size() - 1);
    spdlog::debug("Acessing array for random stuff in start");
    auto* cell = valid_positions[dis(rng)];
    cell->wall = false;
    return cell;
}
