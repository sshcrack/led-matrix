#include "GameOfLifeScene.h"
#include <random>
#include <algorithm>
#include <cmath>

using namespace Scenes;

GameOfLifeScene::GameOfLifeScene() : Scene() {
}

void GameOfLifeScene::initialize(int width, int height) {
    Scene::initialize(width, height);
    this->width = matrix_width;
    this->height = matrix_height;
    
    // Setup grid sizes
    current_grid.resize(width * height);
    next_grid.resize(width * height);
    cell_ages.resize(width * height);
    afterglow.resize(width * height);
    
    last_update = std::chrono::steady_clock::now();
    accumulated_time = 0.0f;
    steps_since_reset = 0;
    steps_since_change = 0;
    has_changed = false;
    previous_population = 0;

    reset_simulation();
}

bool GameOfLifeScene::render(rgb_matrix::FrameCanvas *canvas) {
    auto current_time = std::chrono::steady_clock::now();
    float delta_time = std::chrono::duration<float>(current_time - last_update).count();
    last_update = current_time;
    
    delta_time = std::min(delta_time, 0.25f);
    accumulated_time += delta_time;
    update_interval = 1.0f / update_rate->get();
    
    int updates = 0;
    while (accumulated_time >= update_interval && updates < 4) {
        accumulated_time -= update_interval;
        update_simulation();
        ++updates;
        ++steps_since_reset;

        if ((steps_since_change > 10 && is_stable()) ||
            (auto_reset->get() > 0 && steps_since_reset >= auto_reset->get())) {
            reset_simulation();
            accumulated_time = 0.0f;
            break;
        }
    }
    if (updates == 4) accumulated_time = 0.0f;

    const float glow_decay = std::pow(std::clamp(afterglow_decay->get(), 0.0f, 1.0f), delta_time * 60.0f);
    
    // Render living cells plus a fading history of recently dead generations.
    canvas->Clear();

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int idx = y * width + x;
            uint8_t r = 0, g = 0, b = 0;
            if (current_grid[idx]) {
                get_cell_color(cell_ages[idx], r, g, b);
                afterglow[idx] = 1.0f;
            } else if (afterglow_enabled->get() && afterglow[idx] > 0.015f) {
                const float glow = afterglow[idx];
                r = static_cast<uint8_t>(45.0f * glow);
                g = static_cast<uint8_t>(85.0f * glow);
                b = static_cast<uint8_t>(255.0f * glow);
                afterglow[idx] *= glow_decay;
            }
            if (r || g || b) canvas->SetPixel(x, y, r, g, b);
        }
    }
    
    wait_until_next_frame();
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

    // Occasionally inject a compact moving pattern into long-running worlds.
    // The pattern and orientation vary, preventing a 128x128 board from
    // becoming a static collection of blocks and blinkers.
    const int injection_rate = std::max(20, pattern_injection_rate->get());
    if (inject_patterns->get() && steps_since_reset > 30 &&
        steps_since_reset % injection_rate == 0 && width > 8 && height > 8) {
        std::uniform_int_distribution<int> xdist(3, width - 4);
        std::uniform_int_distribution<int> ydist(3, height - 4);
        std::uniform_int_distribution<int> pattern_dist(0, 2);
        const int x = xdist(rng);
        const int y = ydist(rng);
        switch (pattern_dist(rng)) {
            case 0: inject_glider(x, y, static_cast<int>(rng() % 4)); break;
            case 1: inject_r_pentomino(x, y); break;
            default: inject_acorn(x, y); break;
        }
        has_changed = true;
    }

    int population = 0;
    for (bool alive : current_grid) population += alive ? 1 : 0;
    if (population == previous_population && !has_changed) ++steps_since_change;
    else steps_since_change = 0;
    previous_population = population;
    
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
    std::fill(afterglow.begin(), afterglow.end(), 0.0f);
    
    if (randomize && seeded_resets->get() && (rng() % 3u == 0u)) {
        // Structured seeds create recognizable expanding organisms among the
        // fully random resets.
        if (width > 16 && height > 16) {
            inject_r_pentomino(width / 3, height / 2);
            inject_acorn(2 * width / 3, height / 2);
            inject_glider(width / 2, height / 4, static_cast<int>(rng() % 4));
        }
    } else if (randomize) {
        // Randomly seed the grid
        std::uniform_real_distribution<float> dis(0.0f, 1.0f);
        
        float fill_probability = random_fill->get();
        
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int idx = y * width + x;
                if (dis(rng) < fill_probability) {
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


void GameOfLifeScene::inject_glider(int x, int y, int rotation) {
    static constexpr int points[4][5][2] = {
        {{0,-1},{1,0},{-1,1},{0,1},{1,1}},
        {{-1,-1},{-1,0},{-1,1},{0,-1},{1,0}},
        {{-1,-1},{0,-1},{1,-1},{-1,0},{0,1}},
        {{-1,0},{0,1},{1,-1},{1,0},{1,1}}
    };
    rotation &= 3;
    for (const auto& point : points[rotation]) {
        const int px = (x + point[0] + width) % width;
        const int py = (y + point[1] + height) % height;
        current_grid[py * width + px] = true;
        cell_ages[py * width + px] = 0;
    }
}

void GameOfLifeScene::inject_r_pentomino(int x, int y) {
    static constexpr int points[5][2] = {{0,-1},{1,-1},{-1,0},{0,0},{0,1}};
    for (const auto& point : points) {
        const int px = (x + point[0] + width) % width;
        const int py = (y + point[1] + height) % height;
        current_grid[py * width + px] = true;
        cell_ages[py * width + px] = 0;
    }
}

void GameOfLifeScene::inject_acorn(int x, int y) {
    static constexpr int points[7][2] = {{-3,0},{-2,0},{-2,-2},{0,-1},{1,0},{2,0},{3,0}};
    for (const auto& point : points) {
        const int px = (x + point[0] + width) % width;
        const int py = (y + point[1] + height) % height;
        current_grid[py * width + px] = true;
        cell_ages[py * width + px] = 0;
    }
}

string GameOfLifeScene::get_name() const {
    return "game_of_life";
}

Scenes::SceneDescriptor GameOfLifeScene::get_descriptor() const {
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "cellular";
    d.tags = {"generative", "cellular", "organic", "texture"};
    d.intensity = 0.42f;
    d.motion = 0.38f;
    d.music_affinity = 0.12f;
    d.performance_cost = 0.50f;
    d.variants = {
        {"embers", "Slow colony", "Sparse slowly evolving cells with long afterglow",
         {{"update_rate", 3}, {"random_fill", 0.16f}, {"afterglow", true}, {"afterglow_decay", 0.92f}, {"inject_patterns", true}, {"pattern_injection_rate", 120}},
         {"calm", "texture", "minimal"}, 0.24f, 0.24f, 0.06f, 0.42f},
        {"colony", "Living colony", "Balanced cellular motion with recognizable injected patterns",
         {{"update_rate", 6}, {"random_fill", 0.25f}, {"afterglow", true}, {"afterglow_decay", 0.84f}, {"inject_patterns", true}, {"pattern_injection_rate", 73}},
         {"organic", "generative"}, 0.46f, 0.44f, 0.10f, 0.50f},
        {"chaos", "Cellular bloom", "Faster denser evolution for vivid ambient sections",
         {{"update_rate", 11}, {"random_fill", 0.34f}, {"afterglow", true}, {"afterglow_decay", 0.72f}, {"inject_patterns", true}, {"pattern_injection_rate", 40}},
         {"dense", "energetic", "texture"}, 0.72f, 0.68f, 0.16f, 0.62f},
    };
    return d;
}

void GameOfLifeScene::register_properties() {
    add_property(update_rate);
    add_property(random_fill);
    add_property(auto_reset);
    add_property(age_coloring);
    add_property(afterglow_enabled);
    add_property(afterglow_decay);
    add_property(inject_patterns);
    add_property(seeded_resets);
    add_property(pattern_injection_rate);
}

void GameOfLifeScene::load_properties(const json &j) {
    Scene::load_properties(j);
    reset_simulation();
}

std::unique_ptr<Scene> GameOfLifeSceneWrapper::create() {
    return std::make_unique<GameOfLifeScene>();
}