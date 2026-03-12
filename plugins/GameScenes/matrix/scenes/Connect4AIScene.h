#pragma once

#include "shared/matrix/Scene.h"
#include "shared/matrix/plugin/main.h"
#include <vector>
#include <array>
#include <random>

namespace Scenes {
    class Connect4AIScene : public Scene {
    private:
        static constexpr int COLS = 7;
        static constexpr int ROWS = 6;
        static constexpr uint8_t EMPTY = 0;
        static constexpr uint8_t PLAYER1 = 1;
        static constexpr uint8_t PLAYER2 = 2;

        std::array<std::array<uint8_t, COLS>, ROWS> board;
        uint8_t current_player;
        uint8_t game_state; // 0 = playing, 1 = p1 won, 2 = p2 won, 3 = draw
        int move_count;
        int animation_frame;
        int last_drop_col;
        std::mt19937 rng;

        PropertyPointer<float> ai_speed = MAKE_PROPERTY("ai_speed", float, 1.0f);
        PropertyPointer<int> difficulty = MAKE_PROPERTY("difficulty", int, 2); // 1-3

    public:
        explicit Connect4AIScene();
        ~Connect4AIScene() override = default;

        void register_properties() override;
        bool render(RGBMatrixBase *matrix) override;
        void initialize(RGBMatrixBase *matrix, rgb_matrix::FrameCanvas *l_offscreen_canvas) override;

        tmillis_t get_default_duration() override { return 120000; }
        int get_default_weight() override { return 2; }
        [[nodiscard]] std::string get_name() const override;

        using Scene::Scene;

    private:
        void init_game();
        bool drop_piece(int col);
        bool check_win(uint8_t player);
        int evaluate_board(uint8_t player);
        int ai_minimax(int col, int depth, bool is_maximizing, int alpha, int beta);
        int ai_get_best_move();
        void render_board(RGBMatrixBase *matrix);
    };

    class Connect4AISceneWrapper : public Plugins::SceneWrapper {
    public:
        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create() override;
    };
}
