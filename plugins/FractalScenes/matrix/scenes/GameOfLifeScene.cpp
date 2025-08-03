#include "GameOfLifeScene.h"
#include <random>
#include <algorithm>

using namespace Scenes;

GameOfLifeScene::GameOfLifeScene() : Scene() {
}

void GameOfLifeScene::initialize(RGBMatrixBase *matrix, rgb_matrix::FrameCanvas *l_offscreen_canvas) {
    Scene::initialize(matrix, l_offscreen_canvas);
    width = matrix->width();
    height = matrix->height();
    
    // Setup grid sizes
    current_grid.resize(width * height);
    next_grid.resize(width * height);
    cell_ages.resize(width * height);
    
    last_update = std::chrono::steady_clock::now();
    accumulated_time = 0.0f;
    steps_since_reset = 0;
    steps_since_change = 0;
    has_changed = false;
    
    reset_simulation();
}

bool GameOfLifeScene::render(RGBMatrixBase *matrix) {
    auto current_time = std::chrono::steady_clock::now();
    float delta_time = std::chrono::duration<float>(current_time - last_update).count();
    last_update = current_time;
    
    accumulated_time += delta_time;
    update_interval = 1.0f / update_rate->get();
    
    if (accumulated_time >= update_interval) {
        accumulated_time -= update_interval;
        update_simulation();
        
        steps_since_reset++;
        
        // Check if we need to reset based on stability or max steps
        if ((steps_since_change > 10 && is_stable()) || 
            (auto_reset->get() > 0 && steps_since_reset >= auto_reset->get())) {
            reset_simulation();
        }
    }
    
    // Render the current state
    offscreen_canvas->Clear();
    
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = y * width + x;
            if (current_grid[idx]) {
                uint8_t r, g, b;
                get_cell_color(cell_ages[idx], r, g, b);
                offscreen_canvas->SetPixel(x, y, r, g, b);
            }
        }
    }
    
    return true;
}

void GameOfLifeScene::update_simulation() {
    has_changed = false;
    
    // Apply Game of Life rules
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = y * width + x;
            int neighbors = count_neighbors(x, y);
            bool current_state = current_grid[idx];
            
            // Conway's Game of Life rules
            if (current_state) {
                // Live cell
                if (neighbors < 2 || neighbors > 3) {
                    next_grid[idx] = false; // Cell dies
                    has_changed = true;
                } else {
                    next_grid[idx] = true; // Cell survives
                    cell_ages[idx]++; // Increase cell age
                }
            } else {
                // Dead cell
                if (neighbors == 3) {
                    next_grid[idx] = true; // Cell becomes alive
                    cell_ages[idx] = 0; // Reset age for new cell
                    has_changed = true;
                } else {
                    next_grid[idx] = false; // Cell remains dead
                }
            }
        }
    }
    
    // Swap grids
    current_grid.swap(next_grid);
    
    if (has_changed) {
        steps_since_change = 0;
    } else {
        steps_since_change++;
    }
}

int GameOfLifeScene::count_neighbors(int x, int y) {
    int count = 0;
    
    // Check 8 surrounding cells
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue; // Skip self
            
            // Use modulo for wrapping around edges (toroidal grid)
            int nx = (x + dx + width) % width;
            int ny = (y + dy + height) % height;
            
            if (current_grid[ny * width + nx]) {
                count++;
            }
        }
    }
    
    return count;
}

void GameOfLifeScene::reset_simulation(bool randomize) {
    std::fill(current_grid.begin(), current_grid.end(), false);
    std::fill(next_grid.begin(), next_grid.end(), false);
    std::fill(cell_ages.begin(), cell_ages.end(), 0);
    
    if (randomize) {
        // Randomly seed the grid
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(0.0, 1.0);
        
        float fill_probability = random_fill->get();
        
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int idx = y * width + x;
                if (dis(gen) < fill_probability) {
                    current_grid[idx] = true;
                }
            }
        }
    } else {
        // Add a few interesting patterns
        
        // Add a glider
        int center_x = width / 4;
        int center_y = height / 4;
        
        // R-pentomino
        if (width > 5 && height > 5) {
            current_grid[(center_y - 1) * width + center_x] = true;
            current_grid[(center_y - 1) * width + center_x + 1] = true;
            current_grid[center_y * width + center_x - 1] = true;
            current_grid[center_y * width + center_x] = true;
            current_grid[(center_y + 1) * width + center_x] = true;
        }
        
        // Glider
        center_x = 3 * width / 4;
        center_y = 3 * height / 4;
        if (width > 3 && height > 3) {
            current_grid[(center_y - 1) * width + center_x] = true;
            current_grid[center_y * width + center_x + 1] = true;
            current_grid[(center_y + 1) * width + center_x - 1] = true;
            current_grid[(center_y + 1) * width + center_x] = true;
            current_grid[(center_y + 1) * width + center_x + 1] = true;
        }
    }
    
    steps_since_reset = 0;
    steps_since_change = 0;
    has_changed = true;
}

bool GameOfLifeScene::is_stable() {
    // If nothing changed in the last update, consider it stable
    return !has_changed;
}

void GameOfLifeScene::get_cell_color(int age, uint8_t& r, uint8_t& g, uint8_t& b) const {
    if (age_coloring->get()) {
        // Color based on cell age
        float normalized_age = std::min(1.0f, age / 50.0f);
        
        // Younger cells are blue/white, older cells transition to red/yellow
        r = static_cast<uint8_t>(255.0f * std::min(1.0f, normalized_age * 2.0f));
        g = static_cast<uint8_t>(255.0f * std::max(0.0f, 1.0f - normalized_age));
        b = static_cast<uint8_t>(255.0f * std::max(0.0f, 1.0f - normalized_age));
    } else {
        // Default color - white
        r = g = b = 255;
    }
}

string GameOfLifeScene::get_name() const {
    return "game_of_life";
}

void GameOfLifeScene::register_properties() {
    add_property(update_rate);
    add_property(random_fill);
    add_property(auto_reset);
    add_property(age_coloring);
}

void GameOfLifeScene::load_properties(const json &j) {
    Scene::load_properties(j);
    reset_simulation();
}

std::unique_ptr<Scene, void (*)(Scene *)> GameOfLifeSceneWrapper::create() {
    return {
        new GameOfLifeScene(), [](Scene *scene) {
            delete dynamic_cast<GameOfLifeScene*>(scene);
        }
    };
}