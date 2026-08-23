#include "brain.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace {
constexpr int kRows = 20;
constexpr int kCols = 10;
using Board = std::array<std::array<bool, kCols>, kRows>;

struct PlacementResult {
    Board board{};
    int lines = 0;
    int landing_row = 0;
    bool valid = false;
};

struct Features {
    int aggregate_height = 0;
    int completed_lines = 0;
    int holes = 0;
    int bumpiness = 0;
    int wells = 0;
    int row_transitions = 0;
    int column_transitions = 0;
    int max_height = 0;
    int landing_height = 0;
};

Board settledBoard(const Grid &grid) {
    Board board{};
    for (int row = 0; row < kRows; ++row) {
        for (int col = 0; col < kCols; ++col) {
            board[row][col] = grid.matrix[row + 4][col] >= 2;
        }
    }
    return board;
}

std::vector<std::pair<int, int>> cellsFor(const Piece &piece, int piece_index, int rotation) {
    std::vector<std::pair<int, int>> cells;
    const std::string &shape = piece.shapeList[piece_index][rotation];
    for (int index = 0; index < 16; ++index) {
        if (shape[index] == 'o') cells.emplace_back(index / 4, index % 4);
    }
    return cells;
}

bool fits(const Board &board, const std::vector<std::pair<int, int>> &cells, int row, int col) {
    for (const auto &[dy, dx] : cells) {
        const int y = row + dy;
        const int x = col + dx;
        if (x < 0 || x >= kCols || y >= kRows) return false;
        if (y >= 0 && board[y][x]) return false;
    }
    return true;
}

PlacementResult place(const Board &source, const std::vector<std::pair<int, int>> &cells, int col) {
    PlacementResult result;
    result.board = source;

    int row = -4;
    if (!fits(source, cells, row, col)) return result;
    while (fits(source, cells, row + 1, col)) ++row;

    for (const auto &[dy, dx] : cells) {
        const int y = row + dy;
        const int x = col + dx;
        if (y < 0) return result; // topped out
        result.board[y][x] = true;
    }

    Board compacted{};
    int write_row = kRows - 1;
    for (int y = kRows - 1; y >= 0; --y) {
        const bool full = std::all_of(result.board[y].begin(), result.board[y].end(), [](bool v) { return v; });
        if (full) {
            ++result.lines;
        } else {
            compacted[write_row--] = result.board[y];
        }
    }
    while (write_row >= 0) compacted[write_row--].fill(false);

    result.board = compacted;
    result.landing_row = row;
    result.valid = true;
    return result;
}

Features analyze(const Board &board, int lines, int landing_row) {
    Features f;
    std::array<int, kCols> heights{};

    for (int x = 0; x < kCols; ++x) {
        bool block_seen = false;
        for (int y = 0; y < kRows; ++y) {
            if (board[y][x]) {
                if (!block_seen) {
                    heights[x] = kRows - y;
                    block_seen = true;
                }
            } else if (block_seen) {
                ++f.holes;
            }
        }
        f.aggregate_height += heights[x];
        f.max_height = std::max(f.max_height, heights[x]);
    }

    for (int x = 0; x < kCols - 1; ++x) f.bumpiness += std::abs(heights[x] - heights[x + 1]);

    for (int x = 0; x < kCols; ++x) {
        for (int y = 0; y < kRows; ++y) {
            if (board[y][x]) continue;
            const int left_height = (x == 0) ? kRows : heights[x - 1];
            const int right_height = (x == kCols - 1) ? kRows : heights[x + 1];
            const int cell_height = kRows - y;
            if (cell_height <= std::min(left_height, right_height)) ++f.wells;
        }
    }

    for (int y = 0; y < kRows; ++y) {
        bool previous = true; // walls count as filled
        for (int x = 0; x < kCols; ++x) {
            if (board[y][x] != previous) ++f.row_transitions;
            previous = board[y][x];
        }
        if (!previous) ++f.row_transitions;
    }

    for (int x = 0; x < kCols; ++x) {
        bool previous = true;
        for (int y = 0; y < kRows; ++y) {
            if (board[y][x] != previous) ++f.column_transitions;
            previous = board[y][x];
        }
        if (!previous) ++f.column_transitions;
    }

    f.completed_lines = lines;
    f.landing_height = kRows - landing_row;
    return f;
}

double evaluate(const PlacementResult &placement) {
    const Features f = analyze(placement.board, placement.lines, placement.landing_row);

    // A Dellacherie-style board evaluation with additional height protection.
    // Line clears are strongly rewarded; holes and transitions dominate all
    // cosmetic surface improvements so the AI remains stable for long runs.
    return
        12.0 * f.completed_lines
        - 0.510066 * f.aggregate_height
        - 7.50 * f.holes
        - 0.184483 * f.bumpiness
        - 0.35 * f.wells
        - 0.32 * f.row_transitions
        - 0.28 * f.column_transitions
        - 0.75 * f.max_height
        - 0.08 * f.landing_height;
}

std::pair<int, int> horizontalBounds(const std::vector<std::pair<int, int>> &cells) {
    int min_x = 4;
    int max_x = -1;
    for (const auto &[dy, dx] : cells) {
        (void)dy;
        min_x = std::min(min_x, dx);
        max_x = std::max(max_x, dx);
    }
    return {-min_x, kCols - 1 - max_x};
}

double bestFutureScore(const Board &board, const Piece &piece, int piece_index) {
    double best = -std::numeric_limits<double>::infinity();
    const int rotations = piece.shapeRotations[piece_index];
    for (int rotation = 0; rotation < rotations; ++rotation) {
        const auto cells = cellsFor(piece, piece_index, rotation);
        const auto [min_col, max_col] = horizontalBounds(cells);
        for (int col = min_col; col <= max_col; ++col) {
            const PlacementResult candidate = place(board, cells, col);
            if (candidate.valid) best = std::max(best, evaluate(candidate));
        }
    }
    return best;
}
} // namespace

Brain::Brain() : score(0) {}

Brain::Brain(NeuralNetwork params) : params(std::move(params)), score(0) {}

std::string Brain::getBestMove(Grid grid) {
    const Board board = settledBoard(grid);
    const int current_piece = grid.piece.n;
    const int next_piece = grid.piece.next;

    double best_score = -std::numeric_limits<double>::infinity();
    int best_rotation = 0;
    int best_col = grid.piece.position[1];

    const int rotations = grid.piece.shapeRotations[current_piece];
    for (int rotation = 0; rotation < rotations; ++rotation) {
        const auto cells = cellsFor(grid.piece, current_piece, rotation);
        const auto [min_col, max_col] = horizontalBounds(cells);

        for (int col = min_col; col <= max_col; ++col) {
            const PlacementResult first = place(board, cells, col);
            if (!first.valid) continue;

            const double immediate = evaluate(first);
            const double future = bestFutureScore(first.board, grid.piece, next_piece);
            const double total = immediate + (std::isfinite(future) ? 0.72 * future : -10000.0);

            // Deterministic tie-breaking: fewer rotations, then less travel.
            const bool better = total > best_score + 1e-9;
            const bool tied = std::abs(total - best_score) <= 1e-9;
            const int travel = std::abs(col - grid.piece.position[1]);
            const int best_travel = std::abs(best_col - grid.piece.position[1]);
            if (better || (tied && (rotation < best_rotation ||
                                   (rotation == best_rotation && travel < best_travel)))) {
                best_score = total;
                best_rotation = rotation;
                best_col = col;
            }
        }
    }

    const int delta = best_col - grid.piece.position[1];
    std::string movement;
    if (delta < 0) movement.assign(static_cast<std::size_t>(-delta), 'l');
    else movement.assign(static_cast<std::size_t>(delta), 'r');
    return movement + std::to_string(best_rotation);
}

Brain Brain::crossover(Brain partner) {
    Brain offspring;
    NeuralNetwork p1 = params;
    NeuralNetwork p2 = partner.params;
    const float f1 = static_cast<float>(score);
    const float f2 = static_cast<float>(partner.score);

    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 4; ++j) {
            offspring.params.layer1[i][j] = (f1 * p1.layer1[i][j] + f2 * p2.layer1[i][j]) /
                                             (f1 + f2 + 1e-10f);
        }
        offspring.params.biases1[i] = (f1 * p1.biases1[i] + f2 * p2.biases1[i]) /
                                       (f1 + f2 + 1e-10f);
    }
    for (int i = 0; i < 3; ++i) {
        offspring.params.layer2[i] = (f1 * p1.layer2[i] + f2 * p2.layer2[i]) /
                                      (f1 + f2 + 1e-10f);
    }
    offspring.params.bias2 = (f1 * p1.bias2 + f2 * p2.bias2) / (f1 + f2 + 1e-10f);
    return offspring;
}

std::vector<int> Brain::getColumnHeights(std::vector<std::vector<int>> grid) {
    std::vector<int> heights(kCols, 0);
    for (int col = 0; col < kCols; ++col) {
        for (int row = 4; row < 24; ++row) {
            if (grid[row][col] >= 2) {
                heights[col] = 24 - row;
                break;
            }
        }
    }
    return heights;
}

int Brain::getCompletedLines(std::vector<std::vector<int>> grid) {
    int lines = 0;
    for (int row = 4; row < 24; ++row) {
        if (std::all_of(grid[row].begin(), grid[row].end(), [](int v) { return v >= 2; })) ++lines;
    }
    return lines;
}

int Brain::getAggregateHeight(std::vector<std::vector<int>> grid) {
    const auto heights = getColumnHeights(std::move(grid));
    int total = 0;
    for (int height : heights) total += height;
    return total;
}

int Brain::getBumpiness(std::vector<std::vector<int>> grid) {
    const auto heights = getColumnHeights(std::move(grid));
    int bumpiness = 0;
    for (int i = 0; i < kCols - 1; ++i) bumpiness += std::abs(heights[i] - heights[i + 1]);
    return bumpiness;
}

int Brain::getHoles(std::vector<std::vector<int>> grid) {
    int holes = 0;
    for (int col = 0; col < kCols; ++col) {
        bool covered = false;
        for (int row = 4; row < 24; ++row) {
            if (grid[row][col] >= 2) covered = true;
            else if (covered && grid[row][col] == 0) ++holes;
        }
    }
    return holes;
}
