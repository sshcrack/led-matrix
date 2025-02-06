#include <vector>
#include <chrono>
#include "piece.hpp"

using namespace std;

#ifndef GRID_H
#define GRID_H

class Grid {
    public:
        Piece piece;
        bool gameOver;
        float clearedLines;
        vector<vector<int>> matrix;
        int score;
        bool isAnimating;
        vector<int> animatingLines;
        std::chrono::steady_clock::time_point last_anim_time;
        int animationStep;

        Grid(); 
        void clearLine(); 
        void rotatePiece();
        void movePiece(int dir, int ind);
        void gravity(int val);
        void fixPiece();
        void update();
        void updateAnimation();
};

#endif