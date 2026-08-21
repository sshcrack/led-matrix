#include "PacmanGameScene.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <queue>

namespace {
using Cell = Scenes::PacmanGameScene::Cell;

int manhattan(Cell a, Cell b) { return std::abs(a.x - b.x) + std::abs(a.y - b.y); }

Scenes::PacmanGameScene::Direction opposite(Scenes::PacmanGameScene::Direction d) {
    using D = Scenes::PacmanGameScene::Direction;
    switch (d) {
        case D::Up: return D::Down;
        case D::Down: return D::Up;
        case D::Left: return D::Right;
        case D::Right: return D::Left;
        default: return D::None;
    }
}

const std::array<uint8_t, 5> &digit(int n) {
    static const std::array<std::array<uint8_t, 5>, 10> g{{
        {{7,5,5,5,7}}, {{2,6,2,2,7}}, {{7,1,7,4,7}}, {{7,1,7,1,7}}, {{5,5,7,1,1}},
        {{7,4,7,1,7}}, {{7,4,7,5,7}}, {{7,1,1,1,1}}, {{7,5,7,5,7}}, {{7,5,7,1,7}}
    }};
    return g[std::clamp(n, 0, 9)];
}
}

namespace Scenes {
PacmanGameScene::PacmanGameScene() = default;

std::unique_ptr<Scenes::Scene> PacmanGameSceneWrapper::create() {
    return std::make_unique<PacmanGameScene>();
}

void PacmanGameScene::build_maze() {
    static const std::array<const char *, kH> rows{{
        "############################",
        "#............##............#",
        "#.####.#####.##.#####.####.#",
        "#o####.#####.##.#####.####o#",
        "#.####.#####.##.#####.####.#",
        "#..........................#",
        "#.####.##.########.##.####.#",
        "#.####.##.########.##.####.#",
        "#......##....##....##......#",
        "######.##### ## #####.######",
        "     #.##### ## #####.#     ",
        "     #.##          ##.#     ",
        "     #.## ###--### ##.#     ",
        "######.## #      # ##.######",
        "      .   #      #   .      ",
        "######.## #      # ##.######",
        "     #.## ######## ##.#     ",
        "     #.##          ##.#     ",
        "     #.## ######## ##.#     ",
        "######.## ######## ##.######",
        "#............##............#",
        "#.####.#####.##.#####.####.#",
        "#.####.#####.##.#####.####.#",
        "#o..##.......  .......##..o#",
        "###.##.##.########.##.##.###",
        "###.##.##.########.##.##.###",
        "#......##....##....##......#",
        "#.##########.##.##########.#",
        "#.##########.##.##########.#",
        "#..........................#",
        "############################"
    }};
    pellets_left = 0;
    for (int y = 0; y < kH; ++y) {
        for (int x = 0; x < kW; ++x) {
            maze[y][x] = rows[y][x];
            pellets[y][x] = rows[y][x] == '.' || rows[y][x] == 'o';
            if (pellets[y][x]) ++pellets_left;
        }
    }
}

void PacmanGameScene::initialize(int width, int height) {
    Scene::initialize(width, height);
    build_maze();
    score = 0;
    lives = starting_lives->get();
    reset_round(false);
    last_update = std::chrono::steady_clock::now();
    accumulator = 0.0f;
}

void PacmanGameScene::reset_round(bool reset_level) {
    if (reset_level) build_maze();
    pacman = {13, 23};
    pacman_dir = Direction::Left;
    ghosts = {{
        Ghost{{13, 11}, {13, 11}, {26, 1}, Direction::Left, GhostMode::Chase, 255, 30, 30},
        Ghost{{14, 14}, {14, 14}, {1, 1}, Direction::Up, GhostMode::Chase, 255, 105, 180},
        Ghost{{12, 14}, {12, 14}, {26, 29}, Direction::Up, GhostMode::Chase, 0, 220, 255},
        Ghost{{15, 14}, {15, 14}, {1, 29}, Direction::Up, GhostMode::Chase, 255, 145, 30}
    }};
    frightened_ticks = 0;
    mode_ticks = 0;
    scatter_phase = false;
}

PacmanGameScene::Cell PacmanGameScene::moved(Cell p, Direction dir) const {
    switch (dir) {
        case Direction::Up: --p.y; break;
        case Direction::Down: ++p.y; break;
        case Direction::Left: --p.x; break;
        case Direction::Right: ++p.x; break;
        default: break;
    }
    if (p.x < 0) p.x = kW - 1;
    if (p.x >= kW) p.x = 0;
    return p;
}

bool PacmanGameScene::walkable(Cell c) const {
    if (c.y < 0 || c.y >= kH || c.x < 0 || c.x >= kW) return false;
    return maze[c.y][c.x] != '#';
}

std::vector<PacmanGameScene::Direction> PacmanGameScene::legal_moves(Cell from, Direction current, bool allow_reverse) const {
    static constexpr std::array<Direction, 4> dirs{Direction::Up, Direction::Left, Direction::Down, Direction::Right};
    std::vector<Direction> out;
    for (Direction d : dirs) {
        if (!allow_reverse && d == opposite(current)) continue;
        if (walkable(moved(from, d))) out.push_back(d);
    }
    if (out.empty() && current != Direction::None && walkable(moved(from, opposite(current)))) out.push_back(opposite(current));
    return out;
}

int PacmanGameScene::bfs_distance(Cell start, Cell goal, const std::array<std::array<int, kW>, kH> *danger) const {
    if (!walkable(start) || !walkable(goal)) return 100000;
    std::array<std::array<int, kW>, kH> dist;
    for (auto &r : dist) r.fill(100000);
    struct Node { int cost; Cell cell; };
    struct Cmp { bool operator()(const Node &a, const Node &b) const { return a.cost > b.cost; } };
    std::priority_queue<Node, std::vector<Node>, Cmp> q;
    dist[start.y][start.x] = 0;
    q.push({0, start});
    while (!q.empty()) {
        const Node node = q.top(); q.pop();
        const int cost = node.cost;
        const Cell p = node.cell;
        if (cost != dist[p.y][p.x]) continue;
        if (p.x == goal.x && p.y == goal.y) return cost;
        for (Direction d : {Direction::Up, Direction::Down, Direction::Left, Direction::Right}) {
            Cell n = moved(p, d);
            if (!walkable(n)) continue;
            int step = 1 + (danger ? (*danger)[n.y][n.x] : 0);
            if (cost + step < dist[n.y][n.x]) {
                dist[n.y][n.x] = cost + step;
                q.push({cost + step, n});
            }
        }
    }
    return 100000;
}

std::array<std::array<int, PacmanGameScene::kW>, PacmanGameScene::kH> PacmanGameScene::danger_map() const {
    std::array<std::array<int, kW>, kH> danger{};
    for (auto &r : danger) r.fill(0);
    for (const Ghost &g : ghosts) {
        if (g.mode == GhostMode::Frightened || g.mode == GhostMode::Eyes) continue;
        for (int y = 0; y < kH; ++y) for (int x = 0; x < kW; ++x) {
            const int d = std::abs(x - g.pos.x) + std::abs(y - g.pos.y);
            if (d <= 1) danger[y][x] += 1000;
            else if (d <= 3) danger[y][x] += (4 - d) * 25;
        }
    }
    return danger;
}

PacmanGameScene::Direction PacmanGameScene::choose_pacman_move() const {
    const auto danger = danger_map();
    const auto moves = legal_moves(pacman, pacman_dir, false);
    if (moves.empty()) return opposite(pacman_dir);

    Direction best = moves.front();
    int best_score = std::numeric_limits<int>::max();
    for (Direction d : moves) {
        Cell next = moved(pacman, d);
        int nearest_pellet = 100000;
        int nearest_power = 100000;
        for (int y = 0; y < kH; ++y) for (int x = 0; x < kW; ++x) {
            if (!pellets[y][x]) continue;
            const int distance = bfs_distance(next, {x, y}, &danger);
            nearest_pellet = std::min(nearest_pellet, distance);
            if (maze[y][x] == 'o') nearest_power = std::min(nearest_power, distance);
        }
        int ghost_bonus = 0;
        if (frightened_ticks > 0) {
            int edible = 100000;
            for (const Ghost &g : ghosts) if (g.mode == GhostMode::Frightened)
                edible = std::min(edible, bfs_distance(next, g.pos));
            ghost_bonus = edible < 100000 ? -std::max(0, 45 - edible * 4) : 0;
        }
        const int risk = danger[next.y][next.x];
        const int power_bias = (risk > 0 && nearest_power < 100000) ? nearest_power / 2 : 100000;
        const int value = std::min(nearest_pellet, power_bias) + risk + ghost_bonus;
        if (value < best_score) { best_score = value; best = d; }
    }
    return best;
}

PacmanGameScene::Cell PacmanGameScene::ghost_target(const Ghost &ghost, std::size_t index) const {
    if (ghost.mode == GhostMode::Eyes) return ghost.home;
    if (scatter_phase) return ghost.scatter;
    if (index == 0) return pacman;
    if (index == 1) {
        Cell t = pacman;
        for (int i = 0; i < 4; ++i) t = moved(t, pacman_dir);
        return t;
    }
    if (index == 2) {
        Cell ahead = pacman;
        for (int i = 0; i < 2; ++i) ahead = moved(ahead, pacman_dir);
        Cell red = ghosts[0].pos;
        return {std::clamp(ahead.x + (ahead.x - red.x), 0, kW - 1), std::clamp(ahead.y + (ahead.y - red.y), 0, kH - 1)};
    }
    return manhattan(ghost.pos, pacman) > 8 ? pacman : ghost.scatter;
}

PacmanGameScene::Direction PacmanGameScene::choose_ghost_move(const Ghost &ghost, std::size_t index) const {
    auto moves = legal_moves(ghost.pos, ghost.dir, ghost.mode == GhostMode::Eyes);
    if (moves.empty()) return opposite(ghost.dir);
    if (ghost.mode == GhostMode::Frightened) {
        std::uniform_int_distribution<std::size_t> pick(0, moves.size() - 1);
        return moves[pick(rng)];
    }
    const Cell target = ghost_target(ghost, index);
    return *std::min_element(moves.begin(), moves.end(), [&](Direction a, Direction b) {
        return bfs_distance(moved(ghost.pos, a), target) < bfs_distance(moved(ghost.pos, b), target);
    });
}

void PacmanGameScene::update_pacman() {
    pacman_dir = choose_pacman_move();
    Cell next = moved(pacman, pacman_dir);
    if (walkable(next)) pacman = next;
    consume_cell();
}

void PacmanGameScene::update_ghosts() {
    for (std::size_t i = 0; i < ghosts.size(); ++i) {
        Ghost &g = ghosts[i];
        if (g.mode == GhostMode::Eyes && g.pos.x == g.home.x && g.pos.y == g.home.y)
            g.mode = frightened_ticks > 0 ? GhostMode::Frightened : GhostMode::Chase;
        else if (g.mode != GhostMode::Eyes)
            g.mode = frightened_ticks > 0 ? GhostMode::Frightened : GhostMode::Chase;
        g.dir = choose_ghost_move(g, i);
        Cell next = moved(g.pos, g.dir);
        if (walkable(next)) g.pos = next;
    }
}

void PacmanGameScene::consume_cell() {
    if (!pellets[pacman.y][pacman.x]) return;
    pellets[pacman.y][pacman.x] = false;
    --pellets_left;
    if (maze[pacman.y][pacman.x] == 'o') {
        score += 50;
        frightened_ticks = 65;
        for (Ghost &g : ghosts) if (g.mode != GhostMode::Eyes) g.mode = GhostMode::Frightened;
    } else score += 10;
}

void PacmanGameScene::handle_collisions() {
    for (Ghost &g : ghosts) {
        if (g.pos.x != pacman.x || g.pos.y != pacman.y) continue;
        if (g.mode == GhostMode::Frightened) {
            score += 200;
            g.mode = GhostMode::Eyes;
        } else if (g.mode != GhostMode::Eyes) {
            --lives;
            if (lives <= 0) {
                score = 0;
                lives = starting_lives->get();
                reset_round(true);
            } else reset_round(false);
            return;
        }
    }
}

void PacmanGameScene::update_game() {
    ++mode_ticks;
    if (mode_ticks >= (scatter_phase ? 55 : 145)) { scatter_phase = !scatter_phase; mode_ticks = 0; }
    if (frightened_ticks > 0) --frightened_ticks;
    mouth_open = !mouth_open;
    update_pacman();
    handle_collisions();
    update_ghosts();
    handle_collisions();
    if (pellets_left <= 0) reset_round(true);
}

void PacmanGameScene::draw_score_text(rgb_matrix::FrameCanvas *canvas, int x, int y, int value) const {
    const std::string s = std::to_string(std::max(0, value));
    for (char c : s) {
        const auto &rows = digit(c - '0');
        for (int gy = 0; gy < 5; ++gy) for (int gx = 0; gx < 3; ++gx)
            if (rows[gy] & (1 << (2 - gx))) canvas->SetPixel(x + gx, y + gy, 255, 255, 255);
        x += 4;
    }
}

void PacmanGameScene::draw(rgb_matrix::FrameCanvas *canvas) const {
    canvas->Clear();
    const int hud = show_score->get() && matrix_height >= 40 ? 7 : 0;
    const float sx = static_cast<float>(matrix_width) / kW;
    const float sy = static_cast<float>(matrix_height - hud) / kH;
    const float scale = std::max(1.0f, std::min(sx, sy));
    const int ox = (matrix_width - static_cast<int>(kW * scale)) / 2;
    const int oy = hud + (matrix_height - hud - static_cast<int>(kH * scale)) / 2;
    auto rect = [&](Cell c, uint8_t r, uint8_t g, uint8_t b, float inset) {
        int x0 = ox + static_cast<int>(c.x * scale + inset);
        int y0 = oy + static_cast<int>(c.y * scale + inset);
        int x1 = ox + static_cast<int>((c.x + 1) * scale - inset);
        int y1 = oy + static_cast<int>((c.y + 1) * scale - inset);
        for (int y = y0; y < y1; ++y) for (int x = x0; x < x1; ++x)
            if (x >= 0 && x < matrix_width && y >= 0 && y < matrix_height) canvas->SetPixel(x, y, r, g, b);
    };
    for (int y = 0; y < kH; ++y) for (int x = 0; x < kW; ++x) {
        if (maze[y][x] == '#') rect({x,y}, 20, 55, 210, 0.0f);
        else if (pellets[y][x]) {
            const bool power = maze[y][x] == 'o';
            rect({x,y}, 255, 220, 180, power ? scale * 0.22f : scale * 0.39f);
        }
    }
    rect(pacman, 255, 225, 0, mouth_open ? scale * 0.12f : scale * 0.05f);
    for (const Ghost &g : ghosts) {
        if (g.mode == GhostMode::Eyes) rect(g.pos, 220, 220, 255, scale * 0.28f);
        else if (g.mode == GhostMode::Frightened) rect(g.pos, 25, 55, 255, scale * 0.08f);
        else rect(g.pos, g.r, g.g, g.b, scale * 0.08f);
    }
    if (show_ai_targets->get()) {
        for (std::size_t i = 0; i < ghosts.size(); ++i) {
            Cell t = ghost_target(ghosts[i], i);
            if (walkable(t)) rect(t, ghosts[i].r / 2, ghosts[i].g / 2, ghosts[i].b / 2, scale * 0.34f);
        }
    }
    if (show_score->get() && hud > 0) {
        draw_score_text(canvas, 1, 1, score);
        for (int i = 0; i < lives && i < 6; ++i) {
            int x = matrix_width - 4 * (i + 1);
            for (int yy = 1; yy < 5; ++yy) for (int xx = 0; xx < 3; ++xx) canvas->SetPixel(x + xx, yy, 255, 225, 0);
        }
    }
}

bool PacmanGameScene::render(rgb_matrix::FrameCanvas *canvas) {
    const auto now = std::chrono::steady_clock::now();
    accumulator += std::min(0.25f, std::chrono::duration<float>(now - last_update).count());
    last_update = now;
    const float step = 1.0f / std::max(1.0f, game_speed->get());
    int updates = 0;
    while (accumulator >= step && updates++ < 4) { update_game(); accumulator -= step; }
    draw(canvas);
    wait_until_next_frame();
    return true;
}

void PacmanGameScene::register_properties() {
    add_property(game_speed);
    add_property(show_score);
    add_property(show_ai_targets);
    add_property(starting_lives);
}

void PacmanGameScene::load_properties(const nlohmann::json &j) {
    Scene::load_properties(j);
    lives = std::clamp(lives, 1, starting_lives->get());
}
} // namespace Scenes
