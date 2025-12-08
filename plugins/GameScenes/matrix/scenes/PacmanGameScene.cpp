#include "PacmanGameScene.h"
#include <algorithm>
#include <cmath>

namespace Scenes {

    PacmanGameScene::PacmanGameScene() : rng(std::random_device{}()) {
        reset_game();
    }

    void PacmanGameScene::register_properties() {
        add_property(game_speed);
        add_property(show_grid);
    }

    string PacmanGameScene::get_name() const {
        return "Pacman";
    }

    void PacmanGameScene::initialize(RGBMatrixBase *matrix, rgb_matrix::FrameCanvas *l_offscreen_canvas) {
        Scene::initialize(matrix, l_offscreen_canvas);
        last_update = std::chrono::steady_clock::now();
    }

    void PacmanGameScene::reset_game() {
        // Simple map layout
        // # = Wall, . = Pellet, P = Pacman, G = Ghost Spawn, space = Empty
        map_layout = {
            "###################",
            "#P.......#.......G#",
            "#.##.###.#.###.##.#",
            "#.................#",
            "#.##.#.#####.#.##.#",
            "#....#...#...#....#",
            "####.### # ###.####",
            "   #.# G G G #.#   ",
            "####.#######.####",
            "#........#........#",
            "#.##.###.#.###.##.#",
            "#..#.....P.....#..#",
            "##.#.#.#####.#.#.##",
            "#....#...#...#....#",
            "###################"
        };
        
        // Convert map to internal state
        ghosts.clear();
        pellets.clear();
        pacman = {PACMAN, {1, 1}, {0, 0}, {255, 255, 0}}; // Default

        int y = 0;
        for (const auto& row : map_layout) {
            int x = 0;
            for (char c : row) {
                if (c == '.') {
                    pellets.push_back({x, y});
                } else if (c == 'P') {
                    pacman.pos = {x, y};
                    pacman.dir = {0, 0};
                } else if (c == 'G') {
                    EntityType type = GHOST_RED;
                    Color color = {255, 0, 0};
                    if (ghosts.size() == 1) { type = GHOST_PINK; color = {255, 182, 255}; }
                    else if (ghosts.size() == 2) { type = GHOST_CYAN; color = {0, 255, 255}; }
                    else if (ghosts.size() == 3) { type = GHOST_ORANGE; color = {255, 182, 0}; }
                    
                    ghosts.push_back({type, {x, y}, {0, 0}, color});
                }
                x++;
            }
            y++;
        }
        
        score = 0;
        game_over = false;
    }

    bool PacmanGameScene::is_wall(int x, int y) {
        if (y < 0 || y >= map_layout.size()) return true;
        if (x < 0 || x >= map_layout[y].size()) return true;
        return map_layout[y][x] == '#';
    }
    
    bool PacmanGameScene::is_valid_move(Point p) {
        return !is_wall(p.x, p.y);
    }

    void PacmanGameScene::move_pacman() {
        // Simple AI: Find nearest pellet and move towards it
        if (pellets.empty()) {
            reset_game(); // Win state -> Restart
            return;
        }

        // BFS to find path to nearest pellet is better, but expensive? 
        // Let's try simple direction picking that brings us closer to a pellet
        
        // Find nearest pellet
        Point target = pellets[0];
        int min_dist = 9999;
        
        for (const auto& p : pellets) {
            int dist = std::abs(p.x - pacman.pos.x) + std::abs(p.y - pacman.pos.y);
            if (dist < min_dist) {
                min_dist = dist;
                target = p;
            }
        }

        // Possible moves
        Point moves[] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
        Point best_move = {0, 0};
        int best_move_dist = 9999;
        
        std::vector<Point> valid_moves;

        for (const auto& move : moves) {
            Point next = {pacman.pos.x + move.x, pacman.pos.y + move.y};
            if (is_valid_move(next)) {
                 // Don't reverse immediately if possible
                if (move.x == -pacman.dir.x && move.y == -pacman.dir.y && pacman.dir.x != 0 && pacman.dir.y != 0) {
                   // only if no other choice
                } else {
                    valid_moves.push_back(move);
                    int dist = std::abs(next.x - target.x) + std::abs(next.y - target.y);
                    if (dist < best_move_dist) {
                        best_move_dist = dist;
                        best_move = move;
                    }
                }
            }
        }
        
        if (best_move.x != 0 || best_move.y != 0) {
             pacman.dir = best_move;
             pacman.pos.x += best_move.x;
             pacman.pos.y += best_move.y;
        } else if (!valid_moves.empty()) {
            // Stuck or at target? Random valid move
             std::uniform_int_distribution<int> dist(0, valid_moves.size() - 1);
             Point move = valid_moves[dist(rng)];
             pacman.dir = move;
             pacman.pos.x += move.x;
             pacman.pos.y += move.y;
        }

        // Eat pellet
        for (auto it = pellets.begin(); it != pellets.end();) {
            if (it->x == pacman.pos.x && it->y == pacman.pos.y) {
                it = pellets.erase(it);
                score += 10;
            } else {
                ++it;
            }
        }
    }

    void PacmanGameScene::move_ghosts() {
        Point moves[] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
        
        for (auto& ghost : ghosts) {
             // Simple AI: Move towards Pacman, but with some randomness
             std::vector<Point> valid_moves;
             
             for (const auto& move : moves) {
                 Point next = {ghost.pos.x + move.x, ghost.pos.y + move.y};
                 // Ghosts can't reverse immediately (classic pacman rule roughly)
                 if (move.x == -ghost.dir.x && move.y == -ghost.dir.y && (ghost.dir.x != 0 || ghost.dir.y != 0)) continue;
                 
                 if (is_valid_move(next)) {
                     valid_moves.push_back(move);
                 }
             }

             if (valid_moves.empty()) {
                 // Dead end? allow reverse
                 for (const auto& move : moves) {
                     Point next = {ghost.pos.x + move.x, ghost.pos.y + move.y};
                     if (is_valid_move(next)) {
                         valid_moves.push_back(move);
                     }
                 }
             }
             
             if (valid_moves.empty()) continue; // trapped

             // Pick best move (chase pacman)
             Point best_move = valid_moves[0];
             int min_dist = 9999;
             
             // 20% chance to move randomly (dumb ghosts)
             std::uniform_real_distribution<float> rand_dist(0.0f, 1.0f);
             if (rand_dist(rng) < 0.2f) {
                 std::uniform_int_distribution<int> idx_dist(0, valid_moves.size() - 1);
                 best_move = valid_moves[idx_dist(rng)];
             } else {
                 for (const auto& move : valid_moves) {
                     Point next = {ghost.pos.x + move.x, ghost.pos.y + move.y};
                     int dist = std::abs(next.x - pacman.pos.x) + std::abs(next.y - pacman.pos.y);
                     if (dist < min_dist) {
                         min_dist = dist;
                         best_move = move;
                     }
                 }
             }
             
             ghost.dir = best_move;
             ghost.pos.x += best_move.x;
             ghost.pos.y += best_move.y;
             
             // Collision
             if (ghost.pos == pacman.pos) {
                 game_over = true;
             }
        }
    }

    void PacmanGameScene::update_game() {
        if (game_over) {
            reset_game();
            return;
        }
        
        move_pacman();
        move_ghosts();
    }

    bool PacmanGameScene::render(RGBMatrixBase *matrix) {
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update).count();
        
        int speed_delay = 1000 / *game_speed;
        if (duration > speed_delay) {
            update_game();
            last_update = now;
        }

        // Draw Map
        int w = matrix->width();
        int h = matrix->height();
        
        int map_w = map_layout[0].size();
        int map_h = map_layout.size();
        
        // Calculate scale
        int scale_x = w / map_w;
        int scale_y = h / map_h;
        int scale = std::min(scale_x, scale_y);
        if (scale < 1) scale = 1;

        int final_map_w = map_w * scale;
        int final_map_h = map_h * scale;
        
        // Center map
        int off_x = (w - final_map_w) / 2;
        int off_y = (h - final_map_h) / 2;

        auto draw_rect = [&](int gx, int gy, int r, int g, int b) {
            for(int dy=0; dy<scale; dy++) {
                for(int dx=0; dx<scale; dx++) {
                    matrix->SetPixel(off_x + gx*scale + dx, off_y + gy*scale + dy, r, g, b);
                }
            }
        };

        for (int y = 0; y < map_h; y++) {
            for (int x = 0; x < map_w; x++) {
                char c = map_layout[y][x];
                if (c == '#') {
                    draw_rect(x, y, 0, 0, 255); // Blue walls
                }
            }
        }
        
        // Draw Pellets
        for (const auto& p : pellets) {
            if (scale > 2) {
                int size = scale / 3;
                if (size < 1) size = 1;
                int center = scale / 2;
                int start = center - size/2;
                
                for(int dy=0; dy<size; dy++) {
                    for(int dx=0; dx<size; dx++) {
                        matrix->SetPixel(off_x + p.x*scale + start + dx, off_y + p.y*scale + start + dy, 255, 183, 174);
                    }
                }
            } else {
                 matrix->SetPixel(off_x + p.x*scale + scale/2, off_y + p.y*scale + scale/2, 255, 183, 174);
            }
        }
        
        // Draw Pacman
        draw_rect(pacman.pos.x, pacman.pos.y, pacman.color.r, pacman.color.g, pacman.color.b);
        
        // Draw Ghosts
        for (const auto& ghost : ghosts) {
            draw_rect(ghost.pos.x, ghost.pos.y, ghost.color.r, ghost.color.g, ghost.color.b);
        }

        return true;
    }

    std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> PacmanGameSceneWrapper::create() {
        return {new PacmanGameScene(), [](Scenes::Scene *s) { delete s; }};
    }
}
