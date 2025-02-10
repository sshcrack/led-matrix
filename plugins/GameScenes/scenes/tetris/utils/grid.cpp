#include <algorithm>
#include "grid.hpp"


bool inRange(int n, int a, int b) {
    return (a <= n && n < b);
}

bool contains(int n, vector<int> mat) {
    return (find(mat.begin(), mat.end(), n) != mat.end());
}

Grid::Grid() {
    gameOver = false;
    clearedLines = 0.0;
    score = 0;
    isAnimating = false;
    animationStep = 0;
    for (int i = 0; i < 24; i++) matrix.push_back({0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
}

void Grid::clearLine() {
    animatingLines.clear();
    for (int j = 4; j < 24; j++) {
        if (!contains(0, matrix[j]) && !contains(1, matrix[j])) {
            animatingLines.push_back(j);
        }
    }

    if (!animatingLines.empty()) {
        isAnimating = true;
        animationStep = 0;
    }
}

void Grid::updateAnimation() {
    if (!isAnimating) return;

    auto current_time = std::chrono::steady_clock::now();
    auto time_since_last_fall = std::chrono::duration_cast<std::chrono::milliseconds>(
            current_time - last_anim_time).count();

    if (time_since_last_fall < 75)
        return;

    last_anim_time = current_time;
    animationStep++;
    if (animationStep >= 6) { // Animation complete after 6 steps
        isAnimating = false;
        animationStep = 0;

        // Actually clear the lines
        int nLines = animatingLines.size();
        for (int j: animatingLines) {
            for (int k = 1; k <= j; k++) {
                matrix[j - k + 1] = matrix[j - k];
            }
            matrix[0] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
            piece.position[0]++;
            clearedLines++;
        }

        // Update score
        if (nLines == 1) score += 1;
        else if (nLines == 2) score += 3;
        else if (nLines == 3) score += 5;
        else if (nLines == 4) score += 8;
    } else {
        // During animation, flash the lines
        for (int j: animatingLines) {
            for (int i = 0; i < 10; i++) {
                matrix[j][i] = (animationStep % 2 == 0) ? 0 : 9;
            }
        }
    }
}

void Grid::rotatePiece() {
    int a, b, t = 0;
    bool possible = true;

    while (t++ < 16 && possible) {
        a = piece.position[0] + (t / 4) + 1;
        b = piece.position[1] + (t % 4);

        if (piece.shapeList[piece.n][(piece.m + 1) % 4][t] == 'o' &&
            (a > 23 || b < 0 || b > 9 || inRange(matrix[a][b], 2, 9)))
            possible = false;
    }

    if (possible) {
        piece.m = (piece.m + 1) % 4;
        piece.shape = piece.shapeList[piece.n][piece.m];
    }
}

void Grid::movePiece(int dir, int ind) {
    int a, b, t = 0;
    bool limit = false;

    while (t++ < 16 && !limit) {
        a = piece.position[0] + (t / 4);
        b = piece.position[1] + (t % 4);

        if (piece.shape[t] == 'o') {
            if (b == ind) limit = true;
            else if (0 <= b + dir && b + dir <= 9 && 0 <= a && a <= 23) {
                if ((a + 1 <= 23 && inRange(matrix[a + 1][b + dir], 2, 9)) || inRange(matrix[a][b + dir], 2, 9))
                    limit = true;
            }
        }
    }
    if (!limit) piece.position[1] += dir;
}

void Grid::gravity(int val) {
    if (val == 1) piece.position[0]++;
    else if (val == 2) {
        gravity(1);
        update();
        gravity(1);
    } else {
        while (!piece.fixed) {
            gravity(1);
            update();
        }
    }
}

void Grid::fixPiece() {
    int a, b;
    piece.fixed = true;
    for (int k = 0; k < 16; k++) {
        a = piece.position[0] + (k / 4);
        b = piece.position[1] + (k % 4);

        if (piece.shape[k] == 'o') {
            if (a < 4) gameOver = true;
            else if (a < 24) matrix[a][b] = piece.n + 2;
        }
    }
    if (a < 4) gameOver = true;
}

void Grid::update() {
    int a, b, i = 0;
    bool fixed = false;

    for (int j = 0; j < 24; j++) {
        for (int l = 0; l < 10; l++) {
            if (matrix[j][l] == 1) matrix[j][l] = 0;
        }
    }

    while (i++ < 16 && !fixed) {
        a = piece.position[0] + (i / 4);
        b = piece.position[1] + (i % 4);

        if (piece.shape[i] == 'o' && 0 <= a && a < 24 && 0 <= b && b < 10) {
            if (a == 23 || inRange(matrix[a + 1][b], 2, 9)) {
                fixed = true;
                fixPiece();
            } else {
                matrix[a][b] = 1;
            }
        }
    }
}
