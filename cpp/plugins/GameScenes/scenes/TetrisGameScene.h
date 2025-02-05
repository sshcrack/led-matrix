#pragma once

#include "Scene.h"
#include "wrappers.h"
#include <chrono>
#include <array>
#include <random>
#include <vector>

namespace Scenes {
    class TetrisGameScene : public Scene {
    private:
        static constexpr int BOARD_WIDTH = 10;  // Standard Tetris width
        static constexpr int BOARD_HEIGHT = 20; // Standard Tetris height
        static constexpr int BLOCK_COLORS = 7;

        int scale;       // Single scale factor to maintain aspect ratio
        int offset_x;    // Offset to center the board
        int offset_y;    // Offset for vertical alignment
        std::array<std::array<int, BOARD_WIDTH>, BOARD_HEIGHT> board;  // Back to fixed size
        std::array<std::array<bool, 4>, 4> current_piece;
        std::vector<std::array<std::array<bool, 4>, 4>> tetrominos;
        int current_x, current_y, current_color;
        float accumulated_time;
        float drop_time;
        bool game_over;

        std::chrono::steady_clock::time_point last_update;
        std::mt19937 rng;

        float restart_delay{3.0f};  // Delay before restarting
        float restart_timer{0.0f};  // Timer for restart

        // Border parameters
        static constexpr uint8_t BORDER_BRIGHTNESS = 64;
        
        // Line clear animation
        std::vector<int> clearing_lines;
        float clear_animation_time = 0.0f;
        static constexpr float CLEAR_ANIMATION_DURATION = 0.5f;

        bool isAnimating() const { return isLineAnimating(); }

        void initializeTetrominos();

        void spawnNewPiece();

        bool canMove(int new_x, int new_y, const std::array<std::array<bool, 4>, 4> &piece);

        void mergePiece();

        void clearLines();

        std::array<std::array<bool, 4>, 4> rotatePiece(const std::array<std::array<bool, 4>, 4> &piece);

        void getColorComponents(int color, uint8_t &r, uint8_t &g, uint8_t &b);

        // AI decision making
        void makeAIMove();

        int evaluatePosition(int x, int y, const std::array<std::array<bool, 4>, 4> &piece);

        int evaluateLines(const std::vector<std::vector<int>> &temp_board);

        int evaluateSmoothness(const std::vector<std::vector<int>> &temp_board);

        int evaluateWells(const std::vector<std::vector<int>> &temp_board);

        int evaluateSideAttachment(int x, int y, const std::array<std::array<bool, 4>, 4> &piece);

        // New helper functions
        void drawBorder(rgb_matrix::FrameCanvas* canvas);
        void updateLineClearAnimation(float delta_time);
        void applyLineClears();
        bool isLineAnimating() const { return !clearing_lines.empty(); }

        // AI constants
        static constexpr int HOLE_WEIGHT = -50;
        static constexpr int HEIGHT_WEIGHT = -10;
        static constexpr int LINE_CLEAR_WEIGHT = 100;
        static constexpr int ROUGHNESS_WEIGHT = -20;
        static constexpr int OVERHANG_WEIGHT = -30;

        // AI evaluation methods
        struct AIMove {
            int x;
            int rotation;
            double score;
        };

        AIMove findBestMove();
        double evaluateBoard(const std::vector<std::vector<int>>& board);
        std::vector<std::vector<int>> simulateMove(int x, int y, const std::array<std::array<bool, 4>, 4>& piece);
        int calculateHoles(const std::vector<std::vector<int>>& board);
        int calculateAggregateHeight(const std::vector<std::vector<int>>& board);
        int calculateCompleteLines(const std::vector<std::vector<int>>& board);
        int calculateRoughness(const std::vector<std::vector<int>>& board);
        int calculateOverhangs(const std::vector<std::vector<int>>& board);
        std::vector<int> getColumnHeights(const std::vector<std::vector<int>>& board);

    public:
        explicit TetrisGameScene(const nlohmann::json &config);

        bool render(rgb_matrix::RGBMatrix *matrix) override;

        void initialize(rgb_matrix::RGBMatrix *matrix) override;

        [[nodiscard]] string get_name() const override;
    };

    class TetrisGameSceneWrapper : public Plugins::SceneWrapper {
        Scenes::Scene *create_default() override;

        Scenes::Scene *from_json(const nlohmann::json &args) override;
    };
}
