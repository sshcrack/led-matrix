#include "MazeGeneratorScene.h"
#include <algorithm>
#include <queue>

using namespace Scenes;

MazeGeneratorScene::MazeGeneratorScene(const nlohmann::json &config) : Scene(config, false) {
    step_delay = config.value("step_delay", 0.00025f);
    solve_delay = config.value("solve_delay", 0.00025f);
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

    visited_r = config.value("visited_r", 64);
    visited_g = config.value("visited_g", 0);
    visited_b = config.value("visited_b", 64);
}

void MazeGeneratorScene::initialize(rgb_matrix::RGBMatrix *matrix) {
    Scene::initialize(matrix);
    
    // Make sure we have odd dimensions for the maze
    maze_width = (matrix->width() - 1) / 2 * 2 + 1;
    maze_height = (matrix->height() - 1) / 2 * 2 + 1;
    
    rng.seed(std::random_device()());
    last_update = std::chrono::steady_clock::now();
    
    initializeMaze();
}

void MazeGeneratorScene::initializeMaze() {
    // Initialize all cells as walls
    maze.resize(maze_height, std::vector<CellState>(maze_width, WALL));
    
    // Start from top-left corner (1,1)
    maze[1][1] = PATH;
    generation_stack.push_back({1, 1});
}

bool MazeGeneratorScene::generateStep() {
    if (generation_stack.empty())
        return false;
        
    auto [x, y] = generation_stack.back();
    maze[y][x] = PATH;
    
    // Get all neighbors with distance 2 (potential paths)
    std::vector<std::pair<int, int>> neighbors;
    for (const auto& dir : std::vector<std::pair<int, int>>{{0, -2}, {2, 0}, {0, 2}, {-2, 0}}) {
        int nx = x + dir.first;
        int ny = y + dir.second;
        if (nx > 0 && nx < maze_width-1 && ny > 0 && ny < maze_height-1 
            && maze[ny][nx] == WALL) {
            neighbors.push_back({nx, ny});
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

void MazeGeneratorScene::solveMaze() {
    std::priority_queue<Cell*, std::vector<Cell*>, std::greater<>> open_set;
    std::vector<std::vector<Cell>> cells(maze_height, std::vector<Cell>(maze_width));
    
    // Initialize start cell (1,1)
    Cell* start = &cells[1][1];
    start->x = 1;
    start->y = 1;
    start->f_score = calculateHScore(1, 1, maze_width-2, maze_height-2);
    open_set.push(start);
    
    while (!open_set.empty()) {
        Cell* current = open_set.top();
        open_set.pop();
        
        // Check if we reached the goal
        if (current->x == maze_width-2 && current->y == maze_height-2) {
            // Reconstruct path
            while (current) {
                maze[current->y][current->x] = SOLUTION;
                current = current->parent;
            }
            solving_complete = true;
            return;
        }
        
        std::vector<std::pair<int, int>> neighbors;
        getNeighbors(current->x, current->y, neighbors);
        
        for (const auto& [nx, ny] : neighbors) {
            if (maze[ny][nx] != WALL) {
                int tentative_g = current->g_score + 1;
                Cell* neighbor = &cells[ny][nx];
                
                if (tentative_g < neighbor->g_score || neighbor->g_score == 0) {
                    neighbor->x = nx;
                    neighbor->y = ny;
                    neighbor->parent = current;
                    neighbor->g_score = tentative_g;
                    neighbor->f_score = tentative_g + calculateHScore(nx, ny, maze_width-2, maze_height-2);
                    
                    // Mark as visited unless it's part of the solution or currently highlighted
                    if (maze[ny][nx] != SOLUTION && maze[ny][nx] != HIGHLIGHT) {
                        maze[ny][nx] = VISITED;
                    }
                    open_set.push(neighbor);
                }
            }
        }
    }
}

int MazeGeneratorScene::calculateHScore(int x1, int y1, int x2, int y2) {
    return std::abs(x1 - x2) + std::abs(y1 - y2);
}

void MazeGeneratorScene::getNeighbors(int x, int y, std::vector<std::pair<int, int>>& neighbors) {
    for (const auto& dir : std::vector<std::pair<int, int>>{{0, -1}, {1, 0}, {0, 1}, {-1, 0}}) {
        int nx = x + dir.first;
        int ny = y + dir.second;
        if (nx > 0 && nx < maze_width-1 && ny > 0 && ny < maze_height-1) {
            neighbors.push_back({nx, ny});
        }
    }
}

void MazeGeneratorScene::updateCanvas(rgb_matrix::Canvas* canvas) {
    for (int y = 0; y < maze_height; ++y) {
        for (int x = 0; x < maze_width; ++x) {
            switch (maze[y][x]) {
                case WALL:
                    canvas->SetPixel(x, y, wall_r, wall_g, wall_b);
                    break;
                case PATH:
                    canvas->SetPixel(x, y, 0, 0, 0);  // Empty path
                    break;
                case HIGHLIGHT:
                    canvas->SetPixel(x, y, highlight_r, highlight_g, highlight_b);
                    break;
                case SOLUTION:
                    canvas->SetPixel(x, y, solution_r, solution_g, solution_b);
                    break;
                case VISITED:
                    canvas->SetPixel(x, y, visited_r, visited_g, visited_b);
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
    } else if (!solving_complete) {
        if (accumulated_time >= solve_delay) {
            accumulated_time = 0;
            solveMaze();
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

Scenes::Scene *MazeGeneratorSceneWrapper::create_default() {
    return new MazeGeneratorScene(Scene::create_default(3, 10 * 1000));
}
Scenes::Scene* MazeGeneratorSceneWrapper::from_json(const nlohmann::json &args) {
    return new MazeGeneratorScene(args);
}
