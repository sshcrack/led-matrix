#include <vector>
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

        Grid(); 
        void clearLine(); 
        void rotatePiece();
        void movePiece(int dir, int ind);
        void gravity(int val);
        void fixPiece();
        void update();
};

#endif