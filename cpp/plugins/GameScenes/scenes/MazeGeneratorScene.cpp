#include "MazeGeneratorScene.h"
#include <algorithm>
#include <queue>
#include <spdlog/spdlog.h>

using namespace Scenes;

MazeGeneratorScene::MazeGeneratorScene(const nlohmann::json &config) : Scene(config, false) {
    step_delay = config.value("step_delay", 0.0001f);
    solve_delay = config.value("solve_delay", 0.01f);
    final_delay = config.value("final_delay", 2.0f);
    
    wall_r = config.value("wall_r", 255);
    wall_g = config.value("wall_g", 255);
    wall_b = config.value("wall_b", 255);
    
    highlight_r = config.value("highlight_r", 0);
    highlight_g = config.value("highlight_g", 0);
    highlight_b = config.value("highlight_b", 255);
    
    solution_r = config.value("solution_r", 0);
    solution_g = config.value("solution_g", 255);
    solution_b = config.value("solution_b", 0);

    visited_r = config.value("visited_r", 127);
    visited_g = config.value("visited_g", 0);
    visited_b = config.value("visited_b", 127);

    start_r = config.value("start_r", 255);
    start_g = config.value("start_g", 165);
    start_b = config.value("start_b", 0);
    
    goal_r = config.value("goal_r", 255);
    goal_g = config.value("goal_g", 0);
    goal_b = config.value("goal_b", 0);
}

void MazeGeneratorScene::initialize(rgb_matrix::RGBMatrix *matrix) {
    Scene::initialize(matrix);
    
    // Make sure we have odd dimensions for the maze, but half the size
    maze_width = (matrix->width() / 2 - 1) / 2 * 2 + 1;
    maze_height = (matrix->height() / 2 - 1) / 2 * 2 + 1;
    
    rng.seed(std::random_device()());
    last_update = std::chrono::steady_clock::now();
    
    initializeMaze();
}

void MazeGeneratorScene::initializeMaze() {
    // Initialize all cells as walls
    maze.resize(maze_height, std::vector<CellState>(maze_width, WALL));
    
    // Mark start position and ensure it has a path
    maze[1][1] = START;
    maze[1][2] = PATH;  // Create initial path from start
    
    // Mark goal position and ensure it has a path
    maze[maze_height-2][maze_width-2] = GOAL;
    maze[maze_height-2][maze_width-3] = PATH;  // Create initial path to goal
    
    generation_stack.push_back({1, 1});
}

bool MazeGeneratorScene::generateStep() {
    if (generation_stack.empty())
        return false;
        
    auto [x, y] = generation_stack.back();
    
    // Don't overwrite START, GOAL, or their adjacent paths
    if (maze[y][x] != START && maze[y][x] != GOAL &&
        !((x == 2 && y == 1) ||  // Path next to start
          (x == maze_width-3 && y == maze_height-2))) {  // Path next to goal
        maze[y][x] = PATH;
    }
    
    // Get all neighbors with distance 2 (potential paths)
    std::vector<std::pair<int, int>> neighbors;
    for (const auto& dir : std::vector<std::pair<int, int>>{{0, -2}, {2, 0}, {0, 2}, {-2, 0}}) {
        int nx = x + dir.first;
        int ny = y + dir.second;
        if (nx > 0 && nx < maze_width-1 && ny > 0 && ny < maze_height-1 
            && maze[ny][nx] == WALL) {
            // Don't allow blocking the fixed paths near start/goal
            if (!((nx == 2 && ny == 1) ||  // Path next to start
                  (nx == maze_width-3 && ny == maze_height-2))) {  // Path next to goal
                neighbors.push_back({nx, ny});
            }
        }
    }
    
    if (neighbors.empty()) {
        generation_stack.pop_back();
        return true;
    }
    
    // Randomly choose next cell
    int idx = std::uniform_int_distribution<>(0, neighbors.size()-1)(rng);
    auto [next_x, next_y] = neighbors[idx];
    
    // Mark the wall in between as path
    maze[(y + next_y)/2][(x + next_x)/2] = PATH;
    maze[next_y][next_x] = HIGHLIGHT;  // Highlight newest cell
    
    generation_stack.push_back({next_x, next_y});
    return true;
}

void MazeGeneratorScene::initializeSolver() {
    solver_state.cells.resize(maze_height, std::vector<Cell>(maze_width));
    
    // Initialize start cell (1,1)
    Cell* start = &solver_state.cells[1][1];
    start->x = 1;
    start->y = 1;
    start->cost = calculateDistance(1, 1, maze_width - 2, maze_height - 2);
    solver_state.open_set.push(start);
    solver_state.initialized = true;
}

bool MazeGeneratorScene::solveStep() {
    if (solver_state.open_set.empty()) return false;
    
    Cell* current = solver_state.open_set.top();
    solver_state.open_set.pop();
    
    // Check if we reached the goal
    if (maze[current->y][current->x] == GOAL) {  // Changed goal check to use cell state
        // Reconstruct path
        while (current) {
            if (maze[current->y][current->x] != START && maze[current->y][current->x] != GOAL) {
                maze[current->y][current->x] = SOLUTION;
            }
            current = current->parent;
        }
        return false; // Done solving
    }
    
    std::vector<std::pair<int, int>> neighbors;
    getNeighbors(current->x, current->y, neighbors);
    
    for (const auto& [nx, ny] : neighbors) {
        if (maze[ny][nx] != WALL) {
            int steps = current->steps + 1;
            Cell* neighbor = &solver_state.cells[ny][nx];

            if (steps < neighbor->steps || neighbor->steps == 0) {
                neighbor->x = nx;
                neighbor->y = ny;

                neighbor->parent = current;
                neighbor->steps = steps;
                neighbor->cost = steps + calculateDistance(nx, ny, maze_width - 2, maze_height - 2);
                
                if (maze[ny][nx] != START && maze[ny][nx] != GOAL) {
                    maze[ny][nx] = VISITED;
                }
                solver_state.open_set.push(neighbor);
            }
        }
    }
    
    return true;
}

int MazeGeneratorScene::calculateDistance(int x1, int y1, int x2, int y2) {
    return std::abs(x1 - x2) + std::abs(y1 - y2);
}

void MazeGeneratorScene::getNeighbors(int x, int y, std::vector<std::pair<int, int>>& neighbors) {
    for (const auto& dir : std::vector<std::pair<int, int>>{{0, -1}, {1, 0}, {0, 1}, {-1, 0}}) {
        int nx = x + dir.first;
        int ny = y + dir.second;
        if (nx > 0 && nx < maze_width -1 && ny > 0 && ny < maze_height -1) {
            neighbors.push_back({nx, ny});
        }
    }
}

void MazeGeneratorScene::updateCanvas(rgb_matrix::Canvas* canvas) {
    // Calculate offset to center the smaller maze
    int offset_x = (canvas->width() - maze_width) / 2;
    int offset_y = (canvas->height() - maze_height) / 2;
    
    // Clear the entire canvas first
    for (int y = 0; y < canvas->height(); ++y) {
        for (int x = 0; x < canvas->width(); ++x) {
            canvas->SetPixel(x, y, 0, 0, 0);
        }
    }
    
    // Draw the maze with offset
    for (int y = 0; y < maze_height; ++y) {
        for (int x = 0; x < maze_width; ++x) {
            switch (maze[y][x]) {
                case WALL:
                    canvas->SetPixel(x + offset_x, y + offset_y, wall_r, wall_g, wall_b);
                    break;
                case PATH:
                    canvas->SetPixel(x + offset_x, y + offset_y, 0, 0, 0);
                    break;
                case HIGHLIGHT:
                    canvas->SetPixel(x + offset_x, y + offset_y, highlight_r, highlight_g, highlight_b);
                    break;
                case SOLUTION:
                    canvas->SetPixel(x + offset_x, y + offset_y, solution_r, solution_g, solution_b);
                    break;
                case VISITED:
                    canvas->SetPixel(x + offset_x, y + offset_y, visited_r, visited_g, visited_b);
                    break;
                case START:
                    canvas->SetPixel(x + offset_x, y + offset_y, start_r, start_g, start_b);
                    break;
                case GOAL:
                    canvas->SetPixel(x + offset_x, y + offset_y, goal_r, goal_g, goal_b);
                    break;
            }
        }
    }
}

bool MazeGeneratorScene::render(rgb_matrix::RGBMatrix *matrix) {
    auto now = std::chrono::steady_clock::now();
    float delta = std::chrono::duration<float>(now - last_update).count();
    last_update = now;
    
    accumulated_time += delta;

    if (!generation_complete) {
        // Process multiple generation steps per frame for speed
        for (int i = 0; i < 5 && !generation_complete; i++) {
            if (accumulated_time >= step_delay) {
                accumulated_time = 0;
                // Convert previous highlight back to path
                for (int y = 0; y < maze_height; ++y) {
                    for (int x = 0; x < maze_width; ++x) {
                        if (maze[y][x] == HIGHLIGHT) {
                            maze[y][x] = PATH;
                        }
                    }
                }
                generation_complete = !generateStep();
            }
        }
    } else if (!solving_complete) {
        if (!solver_state.initialized) {
            initializeSolver();
        }
        
        if (accumulated_time >= solve_delay) {
            accumulated_time = 0;
            solving_complete = !solveStep();
        }
    } else {
        completion_time += delta;
        if (completion_time >= final_delay) {
            return false;  // End scene
        }
    }

    updateCanvas(matrix);
    return true;
}

string MazeGeneratorScene::get_name() const {
    return "maze";
}

void MazeGeneratorScene::after_render_stop(rgb_matrix::RGBMatrix *matrix) {
    if(!solving_complete)
        return;

    // Reset the scene for next time
    generation_complete = false;
    solving_complete = false;
    solution_found = false;
    accumulated_time = 0;
    completion_time = 0;
    solver_state = {};
    generation_stack.clear();
    maze.clear();

    initializeMaze();
}

Scenes::Scene *MazeGeneratorSceneWrapper::create_default() {
    return new MazeGeneratorScene(Scene::create_default(3, 10 * 1000));
}
Scenes::Scene* MazeGeneratorSceneWrapper::from_json(const nlohmann::json &args) {
    return new MazeGeneratorScene(args);
}
