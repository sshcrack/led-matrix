#include "MazeGameScene.h"

namespace Scenes {
    MazeGameScene::MazeGameScene() : Scene() {
        rng = std::mt19937(std::random_device()());
    }

    void MazeGameScene::initialize(rgb_matrix::RGBMatrix *matrix, rgb_matrix::FrameCanvas *l_offscreen_canvas) {
        Scene::initialize(matrix, l_offscreen_canvas);
        // Calculate maze size to maintain odd dimensions
        int minSize = std::min(matrix->width(), matrix->height()) / 2;
        if (minSize % 2 == 0) {
            minSize--;
        }
        maze_size = minSize;

        // Calculate scale factor and offsets
        scale_factor = std::min(matrix->width() / maze_size, matrix->height() / maze_size);
        offset_x = (matrix->width() - (maze_size * scale_factor)) / 2;
        offset_y = (matrix->height() - (maze_size * scale_factor)) / 2;

        initialize_maze();
        last_update = std::chrono::steady_clock::now();
    }

    void MazeGameScene::initialize_maze() {
        maze = std::vector<bool>(maze_size * maze_size, false); // false = wall
        current_x = 1;
        current_y = 1;
        hunt_stack.clear();
        hunt_stack.push_back({current_x, current_y});
        maze[current_y * maze_size + current_x] = true;

        generation_complete = false;
        solving_complete = false;
        path.clear();

        nodes.clear();
        open_set.clear();
        closed_set.clear();
        path_found = false;

        // Initialize nodes
        for (int y = 0; y < maze_size; y++) {
            for (int x = 0; x < maze_size; x++) {
                Node node;
                node.x = x;
                node.y = y;
                nodes.push_back(node);
            }
        }
    }

    bool MazeGameScene::render(rgb_matrix::RGBMatrix *matrix) {
        auto now = std::chrono::steady_clock::now();
        float delta = std::chrono::duration<float>(now - last_update).count();

        if (delta >= target_frame_time) {
            if (!generation_complete) {
                generation_complete = hunt_and_kill_step();
            } else if (!solving_complete) {
                solving_complete = solve_step();
            }

            draw_maze();
            offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas);

            if (solving_complete) {
                if (delta >= delay_solution_found) {
                    return false;
                }
            } else {
                last_update = now;
            }
        }

        return true;
    }

    bool MazeGameScene::hunt_and_kill_step() {
        if (hunt_stack.empty()) return true;

        // Get available directions
        std::vector<std::pair<int, int>> directions;
        if (current_x > 1 && !maze[(current_y) * maze_size + (current_x - 2)])
            directions.push_back({-2, 0});
        if (current_x < maze_size - 2 && !maze[(current_y) * maze_size + (current_x + 2)])
            directions.push_back({2, 0});
        if (current_y > 1 && !maze[(current_y - 2) * maze_size + current_x])
            directions.push_back({0, -2});
        if (current_y < maze_size - 2 && !maze[(current_y + 2) * maze_size + current_x])
            directions.push_back({0, 2});

        if (!directions.empty()) {
            // Choose random direction
            std::uniform_int_distribution<> dist(0, directions.size() - 1);
            auto [dx, dy] = directions[dist(rng)];

            // Carve passage
            maze[(current_y + dy / 2) * maze_size + (current_x + dx / 2)] = true;
            maze[(current_y + dy) * maze_size + (current_x + dx)] = true;

            current_x += dx;
            current_y += dy;
            hunt_stack.push_back({current_x, current_y});
        } else {
            hunt_stack.pop_back();
            if (!hunt_stack.empty()) {
                auto [x, y] = hunt_stack.back();
                current_x = x;
                current_y = y;
            }
        }

        return false;
    }

    float MazeGameScene::calculate_heuristic(int x1, int y1, int x2, int y2) const {
        return std::sqrt(std::pow(x2 - x1, 2) + std::pow(y2 - y1, 2));
    }

    std::vector<std::pair<int, int>> MazeGameScene::get_neighbors(int x, int y) const {
        std::vector<std::pair<int, int>> neighbors;
        std::vector<std::pair<int, int>> directions = {{0,  1},
                                                       {1,  0},
                                                       {0,  -1},
                                                       {-1, 0}};

        for (const auto &[dx, dy]: directions) {
            int new_x = x + dx;
            int new_y = y + dy;
            if (new_x >= 0 && new_x < maze_size && new_y >= 0 && new_y < maze_size) {
                if (maze[new_y * maze_size + new_x]) {
                    neighbors.push_back({new_x, new_y});
                }
            }
        }
        return neighbors;
    }

    bool MazeGameScene::solve_step() {
        if (path_found) return true;

        if (open_set.empty() && path.empty()) {
            // Initialize A* algorithm
            open_set.push_back({1, 1});
            nodes[1 * maze_size + 1].g_cost = 0;
            nodes[1 * maze_size + 1].h_cost = calculate_heuristic(1, 1, maze_size - 2, maze_size - 2);
            nodes[1 * maze_size + 1].f_cost = nodes[1 * maze_size + 1].calculate_f_cost();
            return false;
        }

        if (!open_set.empty()) {
            // Find node with lowest f_cost
            auto current_it = std::min_element(open_set.begin(), open_set.end(),
                                               [this](const auto &a, const auto &b) {
                                                   return nodes[a.second * maze_size + a.first].f_cost <
                                                          nodes[b.second * maze_size + b.first].f_cost;
                                               });

            auto [current_x, current_y] = *current_it;

            if (current_x == maze_size - 2 && current_y == maze_size - 2) {
                // Path found, reconstruct it
                path.clear();
                auto current = std::make_pair(current_x, current_y);
                while (current.first != -1) {
                    path.push_back(current);
                    current = nodes[current.second * maze_size + current.first].parent;
                }
                std::reverse(path.begin(), path.end());
                path_found = true;
                return true;
            }

            open_set.erase(current_it);
            closed_set.push_back({current_x, current_y});

            for (const auto &[neighbor_x, neighbor_y]: get_neighbors(current_x, current_y)) {
                if (std::find(closed_set.begin(), closed_set.end(),
                              std::make_pair(neighbor_x, neighbor_y)) != closed_set.end()) {
                    continue;
                }

                float tentative_g_cost = nodes[current_y * maze_size + current_x].g_cost + 1;

                auto &neighbor_node = nodes[neighbor_y * maze_size + neighbor_x];
                if (tentative_g_cost < neighbor_node.g_cost) {
                    neighbor_node.parent = {current_x, current_y};
                    neighbor_node.g_cost = tentative_g_cost;
                    neighbor_node.h_cost = calculate_heuristic(neighbor_x, neighbor_y, maze_size - 2, maze_size - 2);
                    neighbor_node.f_cost = neighbor_node.calculate_f_cost();

                    if (std::find(open_set.begin(), open_set.end(),
                                  std::make_pair(neighbor_x, neighbor_y)) == open_set.end()) {
                        open_set.push_back({neighbor_x, neighbor_y});
                    }
                }
            }
        }

        return false;
    }

    void MazeGameScene::draw_maze() {
        offscreen_canvas->Clear();

        // Draw maze with scaling
        for (int y = 0; y < maze_size; y++) {
            for (int x = 0; x < maze_size; x++) {
                if (maze[y * maze_size + x]) {
                    // Draw scaled pixel as a filled rectangle
                    for (int dy = 0; dy < scale_factor; dy++) {
                        for (int dx = 0; dx < scale_factor; dx++) {
                            offscreen_canvas->SetPixel(
                                offset_x + x * scale_factor + dx,
                                offset_y + y * scale_factor + dy,
                                50, 50, 50
                            );
                        }
                    }
                }
            }
        }

        // Draw current position during generation
        if (!generation_complete) {
            for (int dy = 0; dy < scale_factor; dy++) {
                for (int dx = 0; dx < scale_factor; dx++) {
                    offscreen_canvas->SetPixel(
                        offset_x + current_x * scale_factor + dx,
                        offset_y + current_y * scale_factor + dy,
                        0, 255, 0
                    );
                }
            }
        }

        // Draw A* visualization
        if (generation_complete && !path_found) {
            // Draw closed set
            for (const auto &[x, y]: closed_set) {
                for (int dy = 0; dy < scale_factor; dy++) {
                    for (int dx = 0; dx < scale_factor; dx++) {
                        offscreen_canvas->SetPixel(
                            offset_x + x * scale_factor + dx,
                            offset_y + y * scale_factor + dy,
                            255, 0, 0
                        );
                    }
                }
            }

            // Draw open set
            for (const auto &[x, y]: open_set) {
                for (int dy = 0; dy < scale_factor; dy++) {
                    for (int dx = 0; dx < scale_factor; dx++) {
                        offscreen_canvas->SetPixel(
                            offset_x + x * scale_factor + dx,
                            offset_y + y * scale_factor + dy,
                            0, 255, 0
                        );
                    }
                }
            }
        }

        // Draw solution path
        if (path_found && !path.empty()) {
            for (const auto &[x, y]: path) {
                for (int dy = 0; dy < scale_factor; dy++) {
                    for (int dx = 0; dx < scale_factor; dx++) {
                        offscreen_canvas->SetPixel(
                            offset_x + x * scale_factor + dx,
                            offset_y + y * scale_factor + dy,
                            0, 0, 255
                        );
                    }
                }
            }
        }
    }

    string MazeGameScene::get_name() const {
        return "maze";
    }

    void MazeGameScene::after_render_stop(rgb_matrix::RGBMatrix *matrix) {
        if (solving_complete)
            initialize_maze();
    }

    std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> MazeGameSceneWrapper::create() {
        return std::make_unique<MazeGameScene>();
    }
}
