#pragma once

#include "shared/matrix/Scene.h"
#include "shared/matrix/plugin/main.h"

#include <array>
#include <chrono>
#include <random>
#include <vector>

namespace Scenes {
class PacmanGameScene : public Scene {
public:
    static constexpr int kW = 28;
    static constexpr int kH = 31;
    struct Cell { int x = 0; int y = 0; };
    enum class Direction { None, Up, Down, Left, Right };
    enum class GhostMode { Chase, Scatter, Frightened, Eyes };
    struct Ghost {
        Cell pos;
        Cell home;
        Cell scatter;
        Direction dir = Direction::Left;
        GhostMode mode = GhostMode::Chase;
        uint8_t r = 255, g = 0, b = 0;
    };

    PacmanGameScene();
    bool render(rgb_matrix::FrameCanvas *canvas) override;
    void initialize(int width, int height) override;
    void register_properties() override;
    void load_properties(const nlohmann::json &j) override;
    [[nodiscard]] std::string get_name() const override { return "pacman_ai"; }
    [[nodiscard]] std::string get_category() const override { return "Games"; }
    tmillis_t get_default_duration() override { return 30000; }
    int get_default_weight() override { return 1; }

private:
    std::array<std::array<char, kW>, kH> maze{};
    std::array<std::array<bool, kW>, kH> pellets{};
    Cell pacman;
    Direction pacman_dir = Direction::Left;
    std::array<Ghost, 4> ghosts{};
    int score = 0;
    int lives = 3;
    int pellets_left = 0;
    int frightened_ticks = 0;
    int mode_ticks = 0;
    bool scatter_phase = false;
    bool mouth_open = true;
    float accumulator = 0.0f;
    std::chrono::steady_clock::time_point last_update;
    mutable std::mt19937 rng{std::random_device{}()};

    PropertyPointer<float> game_speed = MAKE_PROPERTY_MINMAX("game_speed", float, 7.5f, 1.0f, 30.0f);
    PropertyPointer<bool> show_score = MAKE_PROPERTY("show_score", bool, true);
    PropertyPointer<bool> show_ai_targets = MAKE_PROPERTY("show_ai_targets", bool, false);
    PropertyPointer<int> starting_lives = MAKE_PROPERTY_MINMAX("starting_lives", int, 3, 1, 9);

    void build_maze();
    void reset_round(bool reset_level);
    void update_game();
    void update_pacman();
    void update_ghosts();
    Direction choose_pacman_move() const;
    Direction choose_ghost_move(const Ghost &ghost, std::size_t index) const;
    Cell ghost_target(const Ghost &ghost, std::size_t index) const;
    std::vector<Direction> legal_moves(Cell from, Direction current, bool allow_reverse) const;
    Cell moved(Cell from, Direction dir) const;
    bool walkable(Cell c) const;
    int bfs_distance(Cell start, Cell goal, const std::array<std::array<int, kW>, kH> *danger = nullptr) const;
    std::array<std::array<int, kW>, kH> danger_map() const;
    void consume_cell();
    void handle_collisions();
    void draw(rgb_matrix::FrameCanvas *canvas) const;
    void draw_score_text(rgb_matrix::FrameCanvas *canvas, int x, int y, int value) const;
};

class PacmanGameSceneWrapper : public Plugins::SceneWrapper {
    std::unique_ptr<Scenes::Scene> create() override;
};
} // namespace Scenes
