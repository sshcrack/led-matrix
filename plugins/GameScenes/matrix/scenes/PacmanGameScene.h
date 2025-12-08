#pragma once

#include "shared/matrix/Scene.h"
#include "shared/matrix/plugin/main.h"
#include <vector>
#include <random>
#include <chrono>

namespace Scenes {
    class PacmanGameScene : public Scene {
    public:
        // Game State Structures (public for hash function)
        struct Point {
            int x, y;
            bool operator==(const Point& other) const { return x == other.x && y == other.y; }
        };

        struct Color {
            uint8_t r;
            uint8_t g;
            uint8_t b;
        };

        enum EntityType {
            PACMAN,
            GHOST_RED,
            GHOST_PINK,
            GHOST_CYAN,
            GHOST_ORANGE
        };
        
        enum GhostMode {
            CHASE,
            SCATTER,
            FRIGHTENED,
            EATEN
        };

        struct Entity {
            EntityType type;
            Point pos;
            Point prev_pos;  // For interpolation
            Point dir;
            Color color;
            GhostMode mode = CHASE;
            Point scatter_target = {0, 0};
            Point spawn_pos = {0, 0};
            int frightened_timer = 0;
        };

    private:
        // Game Properties
        PropertyPointer<int> game_speed = MAKE_PROPERTY("game_speed", int, 5); // Updates per second
        PropertyPointer<bool> show_grid = MAKE_PROPERTY("show_grid", bool, false);

        // Map dimensions (simplified for LED matrix)
        static const int MAP_WIDTH = 32;
        static const int MAP_HEIGHT = 16;   // Adjusted for typical matrix relative sizes or scrolling
                                            // Ideally we want it to fit, or we implement scrolling. 
                                            // Let's assume a fixed simplified map for now that fits or scales. 
                                            // Wait, standard matrix might be 64x32 or 32x32. 
                                            // Let's make it abstract and scale to canvas.
        
        std::vector<std::string> map_layout;
        std::vector<Entity> interactions;
        
        Entity pacman;
        std::vector<Entity> ghosts;
        std::vector<Point> pellets;
        std::vector<Point> power_pellets;
        
        int score = 0;
        bool game_over = false;
        int animation_frame = 0;
        int lives = 3;
        int death_timer = 0;
        bool is_dying = false;
        float interpolation = 0.0f;
        int no_score_steps = 0;              // Counts steps without scoring for stuck detection
        std::deque<Point> recent_positions;  // Track recent Pacman positions to detect oscillation
        
        // Timing
        std::chrono::steady_clock::time_point last_update;
        
        // Random
        std::mt19937 rng;

        void reset_game();
        void update_game();
        void move_pacman();
        void move_ghosts();
        void move_ghost(Entity& ghost);
        bool is_wall(int x, int y);
        bool is_valid_move(Point p);
        Point get_ghost_target(const Entity& ghost);
        std::vector<Point> find_path_bfs(Point start, Point goal, bool avoid_ghosts = false);
        void draw_pacman(RGBMatrixBase* matrix, int x, int y, int scale, int off_x, int off_y);
        void draw_ghost(RGBMatrixBase* matrix, const Entity& ghost, int x, int y, int scale, int off_x, int off_y);
        void draw_pellet(RGBMatrixBase* matrix, int x, int y, int scale, int off_x, int off_y, bool is_power);
        int manhattan_distance(Point a, Point b);

    public:
        explicit PacmanGameScene();
        ~PacmanGameScene() override = default;

        bool render(RGBMatrixBase *matrix) override;
        void initialize(RGBMatrixBase *matrix, rgb_matrix::FrameCanvas *l_offscreen_canvas) override;

        void register_properties() override;
        [[nodiscard]] string get_name() const override;

        using Scene::Scene;

        tmillis_t get_default_duration() override {
            return 30000;
        }

        int get_default_weight() override {
            return 1;
        }
    };

    class PacmanGameSceneWrapper : public Plugins::SceneWrapper {
        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create() override;
    };
}
