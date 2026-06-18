#include "FallingSandScene.h"
#include <shared/matrix/utils/color.h>
#include <cmath>

namespace AmbientScenes {
    FallingSandScene::FallingSandScene() : Scene() {
    }

    void FallingSandScene::initialize(int width, int height) {
        Scene::initialize(width, height);
        grid.assign(matrix_width * matrix_height, 0);
        next_grid.assign(matrix_width * matrix_height, 0);
        spawner_x = matrix_width / 2;
        spawner_dir = 1;
        sand_count = 0;
        current_hue = 0.0f;
    }

    bool FallingSandScene::render(rgb_matrix::FrameCanvas *canvas) {
        // Spawn sand if under limit
        if (sand_count < max_sand->get()) {
            for (int i = 0; i < spawn_rate->get() && sand_count < max_sand->get(); i++) {
                int spawn_x_pos = spawner_x + (std::uniform_int_distribution<int>(0, 4)(rng)) - 2; // slight jitter
                if (spawn_x_pos >= 0 && spawn_x_pos < matrix_width) {
                    for (int spawn_y = 0; spawn_y < matrix_height; spawn_y++) {
                        int idx = spawn_y * matrix_width + spawn_x_pos;
                        if (grid[idx] == 0) {
                            uint8_t r, g, b;
                            float h = std::fmod(current_hue + i * 2, 360.0f);
                            color::hsl_to_rgb(h, 1.0f, 0.5f, r, g, b);
                            grid[idx] = pack_color(r, g, b);
                            sand_count++;
                            break;
                        }
                    }
                }
            }
        } else {
            // Once full, reset immediately or dissolve. Reset for now to loop.
            // Wait a bit then clear
            static int timeout = 0;
            timeout++;
            if (timeout > 60) {
                std::fill(grid.begin(), grid.end(), 0);
                sand_count = 0;
                timeout = 0;
            }
        }

        // Move Spawner
        spawner_x += spawner_dir * 2;
        if (spawner_x <= 2) { spawner_x = 2; spawner_dir = 1; }
        if (spawner_x >= matrix_width - 3) { spawner_x = matrix_width - 3; spawner_dir = -1; }

        current_hue += (float)hue_speed->get();
        if (current_hue >= 360.0f) current_hue -= 360.0f;

        std::fill(next_grid.begin(), next_grid.end(), 0);

        // Update Sand Logic from bottom up
        for (int y = matrix_height - 1; y >= 0; y--) {
            for (int x = 0; x < matrix_width; x++) {
                int idx = y * matrix_width + x;
                uint32_t val = grid[idx];
                if (val != 0) {
                    int down = (y + 1) * matrix_width + x;
                    int down_left = (y + 1) * matrix_width + (x - 1);
                    int down_right = (y + 1) * matrix_width + (x + 1);

                    // If sand hasn't been blocked and isn't falling out of bounds
                    if (y < matrix_height - 1) {
                        if (next_grid[down] == 0 && grid[down] == 0) {
                            next_grid[down] = val; // Move Down
                        } else {
                            bool can_left = x > 0 && next_grid[down_left] == 0 && grid[down_left] == 0;
                            bool can_right = x < matrix_width - 1 && next_grid[down_right] == 0 && grid[down_right] == 0;
                            
                            if (can_left && can_right) {
                                if (std::uniform_int_distribution<int>(0, 1)(rng)) next_grid[down_left] = val;
                                else next_grid[down_right] = val;
                            } else if (can_left) {
                                next_grid[down_left] = val;
                            } else if (can_right) {
                                next_grid[down_right] = val;
                            } else {
                                next_grid[idx] = val; // Stay
                            }
                        }
                    } else {
                        next_grid[idx] = val; // Hit bottom, stay
                    }
                }
            }
        }

        grid = next_grid; // swap grids

        canvas->Clear();
        for (int y = 0; y < matrix_height; y++) {
            for (int x = 0; x < matrix_width; x++) {
                uint32_t c = grid[y * matrix_width + x];
                if (c != 0) {
                    uint8_t r, g, b;
                    unpack_color(c, r, g, b);
                    canvas->SetPixel(x, y, r, g, b);
                }
            }
        }
        
        wait_until_next_frame();
        return true;
    }

    std::string FallingSandScene::get_name() const {
        return "fallingsand";
    }

    void FallingSandScene::register_properties() {
        add_property(spawn_rate);
        add_property(hue_speed);
        add_property(max_sand);
    }

    std::unique_ptr<Scenes::Scene> FallingSandSceneWrapper::create() {
        return std::make_unique<FallingSandScene>();
    }
}
