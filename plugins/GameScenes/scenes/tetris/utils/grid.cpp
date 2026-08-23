#include "grid.hpp"

#include <algorithm>
#include <array>

namespace {
bool isSettled(int value) {
    return value >= 2 && value <= 9;
}

bool canPlace(const Grid &grid, const std::string &shape, int row, int col) {
    for (int index = 0; index < 16; ++index) {
        if (shape[index] != 'o') continue;
        const int y = row + index / 4;
        const int x = col + index % 4;
        if (x < 0 || x >= 10 || y >= 24) return false;
        if (y >= 0 && isSettled(grid.matrix[y][x])) return false;
    }
    return true;
}

void removeActiveCells(Grid &grid) {
    for (auto &row : grid.matrix) {
        for (int &cell : row) {
            if (cell == 1) cell = 0;
        }
    }
}
}

Grid::Grid() {
    gameOver = false;
    clearedLines = 0.0f;
    score = 0;
    isAnimating = false;
    animationStep = 0;
    last_anim_time = std::chrono::steady_clock::now();
    matrix.assign(24, std::vector<int>(10, 0));
}

void Grid::clearLine() {
    if (isAnimating || piece.fixed == false) return;

    animatingLines.clear();
    for (int row = 4; row < 24; ++row) {
        const bool full = std::all_of(matrix[row].begin(), matrix[row].end(), [](int value) {
            return isSettled(value);
        });
        if (full) animatingLines.push_back(row);
    }

    if (!animatingLines.empty()) {
        isAnimating = true;
        animationStep = 0;
        last_anim_time = std::chrono::steady_clock::now();
    }
}

void Grid::updateAnimation() {
    if (!isAnimating) return;

    const auto current_time = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - last_anim_time).count();
    if (elapsed < 75) return;

    last_anim_time = current_time;
    ++animationStep;

    if (animationStep < 6) {
        for (int row : animatingLines) {
            for (int col = 0; col < 10; ++col) matrix[row][col] = (animationStep % 2 == 0) ? 0 : 9;
        }
        return;
    }

    const int line_count = static_cast<int>(animatingLines.size());
    std::array<bool, 24> remove{};
    for (int row : animatingLines) remove[row] = true;

    int write_row = 23;
    for (int read_row = 23; read_row >= 0; --read_row) {
        if (!remove[read_row]) matrix[write_row--] = matrix[read_row];
    }
    while (write_row >= 0) matrix[write_row--].assign(10, 0);

    clearedLines += static_cast<float>(line_count);
    static constexpr std::array<int, 5> line_scores{0, 1, 3, 5, 8};
    score += line_scores[std::clamp(line_count, 0, 4)];

    animatingLines.clear();
    animationStep = 0;
    isAnimating = false;
}

void Grid::rotatePiece() {
    if (piece.fixed) return;
    const int next_rotation = (piece.m + 1) % 4;
    const std::string &next_shape = piece.shapeList[piece.n][next_rotation];

    // Small wall kicks keep rotations legal near either edge.
    static constexpr std::array<int, 5> kicks{0, -1, 1, -2, 2};
    for (int kick : kicks) {
        if (canPlace(*this, next_shape, piece.position[0], piece.position[1] + kick)) {
            piece.position[1] += kick;
            piece.m = next_rotation;
            piece.shape = next_shape;
            update();
            return;
        }
    }
}

void Grid::movePiece(int dir, int /*ind*/) {
    if (piece.fixed || dir == 0) return;
    if (canPlace(*this, piece.shape, piece.position[0], piece.position[1] + dir)) {
        piece.position[1] += dir;
        update();
    }
}

void Grid::gravity(int val) {
    if (piece.fixed) return;

    if (val == 2) {
        gravity(1);
        if (!piece.fixed) gravity(1);
        return;
    }

    if (val == 3) {
        while (!piece.fixed) gravity(1);
        return;
    }

    if (canPlace(*this, piece.shape, piece.position[0] + 1, piece.position[1])) {
        ++piece.position[0];
        update();
    } else {
        fixPiece();
    }
}

void Grid::fixPiece() {
    if (piece.fixed) return;
    removeActiveCells(*this);

    bool above_visible_board = false;
    for (int index = 0; index < 16; ++index) {
        if (piece.shape[index] != 'o') continue;
        const int y = piece.position[0] + index / 4;
        const int x = piece.position[1] + index % 4;
        if (y < 4) above_visible_board = true;
        if (y >= 0 && y < 24 && x >= 0 && x < 10) matrix[y][x] = piece.n + 2;
    }

    piece.fixed = true;
    gameOver = above_visible_board;
}

void Grid::update() {
    removeActiveCells(*this);
    if (piece.fixed) return;

    if (!canPlace(*this, piece.shape, piece.position[0], piece.position[1])) {
        gameOver = true;
        return;
    }

    for (int index = 0; index < 16; ++index) {
        if (piece.shape[index] != 'o') continue;
        const int y = piece.position[0] + index / 4;
        const int x = piece.position[1] + index % 4;
        if (y >= 0 && y < 24 && x >= 0 && x < 10) matrix[y][x] = 1;
    }
}
