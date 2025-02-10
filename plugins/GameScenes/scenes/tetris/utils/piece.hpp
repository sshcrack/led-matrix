#include <vector>
#include <string>
#include <random>

using namespace std;

#ifndef PIECE_H
#define PIECE_H

class Piece {
    public:
        vector<int> pieceBag;
        int n, m, next;
        string shape;
        int position[2];
        bool fixed;
        vector<vector<string>> shapeList;
        vector<int> shapeRotations;

        Piece(); 
        void newShape();
        void newNext();
};

#endif