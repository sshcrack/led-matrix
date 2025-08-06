#pragma once

#include "shared/matrix/Scene.h"
#include "shared/matrix/wrappers.h"
#include "shared/matrix/utils/FrameTimer.h"
#include "graphics.h"
#include <vector>
#include <random>
#include <deque>

namespace Scenes {
    
    enum class Direction {
        UP, DOWN, LEFT, RIGHT
    };
    
    struct Position {
        int x, y;
        Position(int x = 0, int y = 0) : x(x), y(y) {}
        bool operator==(const Position& other) const {
            return x == other.x && y == other.y;
        }
    };
    
    class SnakeGameScene : public Scene {
    private:
        FrameTimer frameTimer;
        
        // Game state
        std::deque<Position> snake;
        Position food;
        Direction current_direction;
        Direction next_direction;
        bool game_over;
        bool game_won;
        int score;
        int level;
        float game_speed;
        bool food_eaten;
        
        // Animation state
        int death_animation_frame;
        int win_animation_frame;
        float food_pulse_phase;
        int game_over_flash_timer;
        int frame_counter = 0;
        
        // AI state
        std::vector<std::vector<int>> distance_map;
        std::mt19937 rng;
        
        // Game properties
        PropertyPointer<bool> ai_enabled = MAKE_PROPERTY("ai_enabled", bool, true);
        PropertyPointer<float> base_speed = MAKE_PROPERTY_MINMAX("base_speed", float, 0.2f, 0.05f, 1.0f);
        PropertyPointer<bool> rainbow_snake = MAKE_PROPERTY("rainbow_snake", bool, true);
        PropertyPointer<bool> enable_wrap = MAKE_PROPERTY("enable_wrap", bool, false);
        PropertyPointer<bool> show_score = MAKE_PROPERTY("show_score", bool, true);
        PropertyPointer<bool> animated_food = MAKE_PROPERTY("animated_food", bool, true);
        
        // Game logic methods
        void initializeGame();
        void updateGame();
        void moveSnake();
        bool checkCollision(const Position& pos) const;
        void generateFood();
        void calculateLevel();
        
        // AI methods
        void updateAI();
        Direction findPathToFood();
        bool isValidMove(Direction dir) const;
        int calculateDistance(const Position& from, const Position& to) const;
        void buildDistanceMap(const Position& target);
        Direction getDirectionTo(const Position& from, const Position& to) const;
        bool willCauseSelfTrap(Direction dir) const;
        
        // Rendering methods
        void renderGame();
        void renderGameOver();
        void renderWin();
        void renderSnake();
        void renderFood();
        void renderScore();
        void renderUI();
        
        // Utility methods
        rgb_matrix::Color getSnakeColor(int segment_index) const;
        rgb_matrix::Color getFoodColor() const;
        Position getNextPosition(const Position& pos, Direction dir) const;
        
    public:
        SnakeGameScene();
        ~SnakeGameScene() override = default;

        bool render(rgb_matrix::RGBMatrixBase *matrix) override;
        std::string get_name() const override;
        void register_properties() override;
        void initialize(rgb_matrix::RGBMatrixBase *matrix, rgb_matrix::FrameCanvas *canvas) override;
        
        tmillis_t get_default_duration() override {
            return 60000; // 1 minute game sessions
        }
        
        int get_default_weight() override {
            return 8;
        }
    };

    class SnakeGameSceneWrapper : public Plugins::SceneWrapper {
    public:
        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create() override;
    };
}