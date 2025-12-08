#include "PacmanGameScene.h"
#include <algorithm>
#include <cmath>
#include <queue>
#include <unordered_set>
#include <unordered_map>
#include <deque>

namespace Scenes {

    // Hash function for Point to use in unordered_set/map
    struct PointHash {
        std::size_t operator()(const PacmanGameScene::Point& p) const {
            return std::hash<int>()(p.x) ^ (std::hash<int>()(p.y) << 1);
        }
    };

    PacmanGameScene::PacmanGameScene() : rng(std::random_device{}()) {
        reset_game();
    }

    void PacmanGameScene::register_properties() {
        add_property(game_speed);
        add_property(show_grid);
    }

    string PacmanGameScene::get_name() const {
        return "pacman";
    }

    void PacmanGameScene::initialize(RGBMatrixBase *matrix, rgb_matrix::FrameCanvas *l_offscreen_canvas) {
        Scene::initialize(matrix, l_offscreen_canvas);
        last_update = std::chrono::steady_clock::now();
    }

    void PacmanGameScene::reset_game() {
        // Improved map layout with power pellets (O)
        map_layout = {
            "#####################",
            "#O........#........O#",
            "#.##.###.#.###.##.#",
            "#...................#",
            "#.##.#.#####.#.##.#",
            "#....#...#...#......#",
            "####.### # ###.####",
            "   #.# GGG #.#   ",
            "####.#######.####",
            "#........#........#",
            "#.##.###.#.###.##.#",
            "#..#.....P.....#..#",
            "##.#.#.#####.#.#.##",
            "#O...#...#...#...O#",
            "#####################"
        };
        
        // Convert map to internal state
        ghosts.clear();
        pellets.clear();
        power_pellets.clear();
        pacman = {PACMAN, {1, 1}, {1, 1}, {0, 0}, {255, 255, 0}};

        int y = 0;
        for (const auto& row : map_layout) {
            int x = 0;
            for (char c : row) {
                if (c == '.') {
                    pellets.push_back({x, y});
                } else if (c == 'O') {
                    power_pellets.push_back({x, y});
                } else if (c == 'P') {
                    pacman.pos = {x, y};
                    pacman.prev_pos = {x, y};
                    pacman.dir = {0, 0};
                } else if (c == 'G') {
                    EntityType type = GHOST_RED;
                    Color color = {255, 0, 0};
                    Point scatter = {18, 0};
                    
                    if (ghosts.size() == 1) { 
                        type = GHOST_PINK; 
                        color = {255, 184, 255}; 
                        scatter = {2, 0};
                    } else if (ghosts.size() == 2) { 
                        type = GHOST_CYAN; 
                        color = {0, 255, 255}; 
                        scatter = {18, 14};
                    } else if (ghosts.size() >= 3) { 
                        type = GHOST_ORANGE; 
                        color = {255, 184, 82}; 
                        scatter = {2, 14};
                    }
                    
                    Entity ghost = {type, {x, y}, {x, y}, {0, 0}, color};
                    ghost.scatter_target = scatter;
                    ghost.spawn_pos = {x, y};
                    ghost.mode = SCATTER;
                    ghosts.push_back(ghost);
                }
                x++;
            }
            y++;
        }
        
        score = 0;
        game_over = false;
        animation_frame = 0;
        death_timer = 0;
        is_dying = false;
        no_score_steps = 0;
        recent_positions.clear();
    }

    bool PacmanGameScene::is_wall(int x, int y) {
        if (y < 0 || y >= map_layout.size()) return true;
        if (x < 0 || x >= map_layout[y].size()) return true;
        return map_layout[y][x] == '#';
    }
    
    bool PacmanGameScene::is_valid_move(Point p) {
        return !is_wall(p.x, p.y);
    }
    
    int PacmanGameScene::manhattan_distance(Point a, Point b) {
        return std::abs(a.x - b.x) + std::abs(a.y - b.y);
    }

    std::vector<PacmanGameScene::Point> PacmanGameScene::find_path_bfs(Point start, Point goal, bool avoid_ghosts) {
        std::queue<Point> queue;
        std::unordered_map<Point, Point, PointHash> came_from;
        std::unordered_set<Point, PointHash> visited;
        
        queue.push(start);
        visited.insert(start);
        came_from[start] = start;
        
        Point moves[] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
        
        while (!queue.empty()) {
            Point current = queue.front();
            queue.pop();
            
            if (current == goal) {
                // Reconstruct path
                std::vector<Point> path;
                Point p = goal;
                while (!(p == start)) {
                    path.push_back(p);
                    p = came_from[p];
                }
                std::reverse(path.begin(), path.end());
                return path;
            }
            
            for (const auto& move : moves) {
                Point next = {current.x + move.x, current.y + move.y};
                
                if (!is_valid_move(next) || visited.count(next)) continue;
                
                // Avoid ghosts if requested
                if (avoid_ghosts) {
                    bool too_close = false;
                    for (const auto& ghost : ghosts) {
                        if (ghost.mode != FRIGHTENED && ghost.mode != EATEN) {
                            if (manhattan_distance(next, ghost.pos) < 3) {
                                too_close = true;
                                break;
                            }
                        }
                    }
                    if (too_close) continue;
                }
                
                visited.insert(next);
                came_from[next] = current;
                queue.push(next);
            }
        }
        
        return {}; // No path found
    }

    PacmanGameScene::Point PacmanGameScene::get_ghost_target(const Entity& ghost) {
        if (ghost.mode == EATEN) {
            return ghost.spawn_pos;
        }
        if (ghost.mode == SCATTER) {
            return ghost.scatter_target;
        }
        
        if (ghost.mode == FRIGHTENED) {
            // Random movement handled elsewhere
            return ghost.pos;
        }
        
        // CHASE mode - different behavior per ghost
        switch (ghost.type) {
            case GHOST_RED: // Blinky - direct chase
                return pacman.pos;
                
            case GHOST_PINK: { // Pinky - target 4 tiles ahead of Pacman
                Point target = pacman.pos;
                target.x += pacman.dir.x * 4;
                target.y += pacman.dir.y * 4;
                return target;
            }
            
            case GHOST_CYAN: { // Inky - complex targeting
                Point two_ahead = pacman.pos;
                two_ahead.x += pacman.dir.x * 2;
                two_ahead.y += pacman.dir.y * 2;
                
                // Find red ghost
                for (const auto& g : ghosts) {
                    if (g.type == GHOST_RED) {
                        Point target;
                        target.x = two_ahead.x + (two_ahead.x - g.pos.x);
                        target.y = two_ahead.y + (two_ahead.y - g.pos.y);
                        return target;
                    }
                }
                return pacman.pos;
            }
            
            case GHOST_ORANGE: { // Clyde - chase if far, scatter if close
                int dist = manhattan_distance(ghost.pos, pacman.pos);
                if (dist > 8) {
                    return pacman.pos;
                } else {
                    return ghost.scatter_target;
                }
            }
            
            default:
                return pacman.pos;
        }
    }

    void PacmanGameScene::move_pacman() {
        // Store previous position for interpolation
        pacman.prev_pos = pacman.pos;
        // Track recent positions for stuck detection
        recent_positions.push_back(pacman.pos);
        if (recent_positions.size() > 8) recent_positions.pop_front();
        
        // Check if any ghosts are dangerous
        bool ghost_nearby = false;
        for (const auto& ghost : ghosts) {
            if (ghost.mode != FRIGHTENED && ghost.mode != EATEN) {
                if (manhattan_distance(pacman.pos, ghost.pos) < 5) {
                    ghost_nearby = true;
                    break;
                }
            }
        }
        
        // Priority 1: Eat power pellets if ghosts nearby
        if (ghost_nearby && !power_pellets.empty()) {
            Point nearest_power = power_pellets[0];
            int min_dist = 9999;
            
            for (const auto& pp : power_pellets) {
                int dist = manhattan_distance(pacman.pos, pp);
                if (dist < min_dist) {
                    min_dist = dist;
                    nearest_power = pp;
                }
            }
            
            auto path = find_path_bfs(pacman.pos, nearest_power, false);
            if (!path.empty()) {
                Point next = path[0];
                pacman.dir = {next.x - pacman.pos.x, next.y - pacman.pos.y};
                pacman.pos = next;
                return;
            }
        }
        
        // Priority 2: Avoid ghosts if in danger
        if (ghost_nearby) {
            // Find nearest safe pellet
            Point best_target = pacman.pos;
            int best_score = -9999;
            
            for (const auto& p : pellets) {
                int ghost_dist_sum = 0;
                for (const auto& ghost : ghosts) {
                    if (ghost.mode != FRIGHTENED && ghost.mode != EATEN) {
                        ghost_dist_sum += manhattan_distance(p, ghost.pos);
                    }
                }
                
                int pellet_dist = manhattan_distance(pacman.pos, p);
                int score = ghost_dist_sum - pellet_dist * 2;
                
                if (score > best_score) {
                    best_score = score;
                    best_target = p;
                }
            }
            
            if (!(best_target == pacman.pos)) {
                auto path = find_path_bfs(pacman.pos, best_target, true);
                if (!path.empty()) {
                    Point next = path[0];
                    pacman.dir = {next.x - pacman.pos.x, next.y - pacman.pos.y};
                    pacman.pos = next;
                    return;
                }
            }
        }
        
        // Priority 3: Chase frightened ghosts
        for (const auto& ghost : ghosts) {
            if (ghost.mode == FRIGHTENED) {
                auto path = find_path_bfs(pacman.pos, ghost.pos, false);
                if (!path.empty()) {
                    Point next = path[0];
                    pacman.dir = {next.x - pacman.pos.x, next.y - pacman.pos.y};
                    pacman.pos = next;
                    return;
                }
            }
        }
        
        // Priority 4: Collect nearest pellet
        if (!pellets.empty()) {
            Point nearest_pellet = pellets[0];
            int min_dist = 9999;
            
            for (const auto& p : pellets) {
                int dist = manhattan_distance(p, pacman.pos);
                if (dist < min_dist) {
                    min_dist = dist;
                    nearest_pellet = p;
                }
            }
            
            auto path = find_path_bfs(pacman.pos, nearest_pellet, ghost_nearby);
            if (!path.empty()) {
                Point next = path[0];
                pacman.dir = {next.x - pacman.pos.x, next.y - pacman.pos.y};
                pacman.pos = next;
            }
        } else if (pellets.empty() && power_pellets.empty()) {
            // Win state
            reset_game();
            return;
        }

        // Fallback: never stand still
        if (pacman.dir.x == 0 && pacman.dir.y == 0) {
            Point moves[] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
            std::vector<Point> valid_moves;
            for (const auto& mv : moves) {
                Point nxt = {pacman.pos.x + mv.x, pacman.pos.y + mv.y};
                if (is_valid_move(nxt)) {
                    valid_moves.push_back(mv);
                }
            }
            if (!valid_moves.empty()) {
                std::uniform_int_distribution<int> idx(0, valid_moves.size() - 1);
                Point mv = valid_moves[idx(rng)];
                pacman.dir = mv;
                pacman.pos.x += mv.x;
                pacman.pos.y += mv.y;
            }
        }

        // Detect oscillation (A-B-A-B) with no scoring
        if (no_score_steps > 30 && recent_positions.size() >= 4) {
            const Point& p1 = recent_positions[recent_positions.size() - 1];
            const Point& p2 = recent_positions[recent_positions.size() - 2];
            const Point& p3 = recent_positions[recent_positions.size() - 3];
            const Point& p4 = recent_positions[recent_positions.size() - 4];
            bool oscillating = (p1 == p3) && (p2 == p4);
            if (oscillating) {
                // Break the loop: pick a random far pellet target
                Point target = pacman.pos;
                int best = -1;
                for (const auto& p : pellets) {
                    int dist = manhattan_distance(pacman.pos, p);
                    if (dist > best) {
                        best = dist;
                        target = p;
                    }
                }
                auto path = find_path_bfs(pacman.pos, target, false);
                if (!path.empty()) {
                    Point next = path[0];
                    pacman.dir = {next.x - pacman.pos.x, next.y - pacman.pos.y};
                    pacman.pos = next;
                    no_score_steps = 0; // reset to avoid repeated triggers
                }
            }
        }
    }

    void PacmanGameScene::move_ghost(Entity& ghost) {
        // Store previous position for interpolation
        ghost.prev_pos = ghost.pos;
        
        // Update frightened timer
        if (ghost.mode == FRIGHTENED) {
            ghost.frightened_timer--;
            if (ghost.frightened_timer <= 0) {
                ghost.mode = CHASE;
            }
        }
        
        // Eaten ghosts return to spawn
        if (ghost.mode == EATEN) {
            if (ghost.pos == ghost.spawn_pos) {
                // Reached spawn, respawn
                ghost.mode = SCATTER;
                return;
            }
            // Move towards spawn
            auto path = find_path_bfs(ghost.pos, ghost.spawn_pos, false);
            if (!path.empty()) {
                Point next = path[0];
                ghost.dir = {next.x - ghost.pos.x, next.y - ghost.pos.y};
                ghost.pos = next;
                return;
            }
        }
        
        Point moves[] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
        std::vector<Point> valid_moves;
        
        for (const auto& move : moves) {
            Point next = {ghost.pos.x + move.x, ghost.pos.y + move.y};
            // Ghosts can't reverse immediately (classic rule)
            if (move.x == -ghost.dir.x && move.y == -ghost.dir.y && (ghost.dir.x != 0 || ghost.dir.y != 0)) 
                continue;
            
            if (is_valid_move(next)) {
                valid_moves.push_back(move);
            }
        }

        if (valid_moves.empty()) {
            // Dead end, allow reverse
            for (const auto& move : moves) {
                Point next = {ghost.pos.x + move.x, ghost.pos.y + move.y};
                if (is_valid_move(next)) {
                    valid_moves.push_back(move);
                }
            }
        }
        
        if (valid_moves.empty()) return; // Trapped
        
        Point best_move = valid_moves[0];
        
        if (ghost.mode == FRIGHTENED) {
            // Random movement when frightened
            std::uniform_int_distribution<int> idx_dist(0, valid_moves.size() - 1);
            best_move = valid_moves[idx_dist(rng)];
        } else {
            // Chase or scatter to target
            Point target = get_ghost_target(ghost);
            int min_dist = 9999;
            
            for (const auto& move : valid_moves) {
                Point next = {ghost.pos.x + move.x, ghost.pos.y + move.y};
                int dist = manhattan_distance(next, target);
                if (dist < min_dist) {
                    min_dist = dist;
                    best_move = move;
                }
            }
        }
        
        ghost.dir = best_move;
        ghost.pos.x += best_move.x;
        ghost.pos.y += best_move.y;
        
        // Collision check
        if (ghost.pos == pacman.pos) {
            if (ghost.mode == FRIGHTENED) {
                // Eat ghost
                score += 200;
                ghost.mode = EATEN;
                ghost.frightened_timer = 0;
            } else if (ghost.mode != EATEN) {
                // Pacman dies
                is_dying = true;
                death_timer = 20; // Pause for ~2 seconds at 10fps
            }
        }
    }

    void PacmanGameScene::move_ghosts() {
        for (auto& ghost : ghosts) {
            move_ghost(ghost);
        }
    }

    void PacmanGameScene::update_game() {
        if (game_over) {
            reset_game();
            return;
        }
        
        // Handle death timer
        if (is_dying) {
            death_timer--;
            if (death_timer <= 0) {
                lives--;
                if (lives <= 0) {
                    game_over = true;
                } else {
                    // Reset positions
                    is_dying = false;
                    pacman.pos = {10, 11};
                    pacman.prev_pos = pacman.pos;
                    pacman.dir = {0, 0};
                    
                    for (auto& g : ghosts) {
                        g.pos = g.spawn_pos;
                        g.prev_pos = g.pos;
                        g.dir = {0, 0};
                        g.mode = SCATTER;
                        g.frightened_timer = 0;
                    }
                }
            }
            return; // Don't update while dying
        }
        
        animation_frame++;
        move_pacman();
        move_ghosts();
        
        bool scored = false;
        // Eat pellets after movement
        for (auto it = pellets.begin(); it != pellets.end();) {
            if (it->x == pacman.pos.x && it->y == pacman.pos.y) {
                it = pellets.erase(it);
                score += 10;
                scored = true;
            } else {
                ++it;
            }
        }
        
        // Eat power pellets after movement
        for (auto it = power_pellets.begin(); it != power_pellets.end();) {
            if (it->x == pacman.pos.x && it->y == pacman.pos.y) {
                it = power_pellets.erase(it);
                score += 50;
                scored = true;
                
                // Make all ghosts frightened
                for (auto& ghost : ghosts) {
                    if (ghost.mode != EATEN) {
                        ghost.mode = FRIGHTENED;
                        ghost.frightened_timer = 40; // 40 updates
                    }
                }
            } else {
                ++it;
            }
        }

        if (scored) {
            no_score_steps = 0;
        } else {
            no_score_steps++;
        }
    }
    
    void PacmanGameScene::draw_pacman(RGBMatrixBase* matrix, int x, int y, int scale, int off_x, int off_y) {
        // Animated Pacman sprite with mouth opening/closing
        int mouth_phase = (animation_frame / 3) % 4; // 0-3 animation cycle
        
        // Death animation
        if (is_dying) {
            mouth_phase = std::min(3, death_timer / 5); // Close mouth
        }
        
        for(int dy = 0; dy < scale; dy++) {
            for(int dx = 0; dx < scale; dx++) {
                int px = off_x + x + dx;
                int py = off_y + y + dy;
                
                // Calculate relative position from center
                float cx = (dx - scale/2.0f) / (scale/2.0f);
                float cy = (dy - scale/2.0f) / (scale/2.0f);
                float dist = std::sqrt(cx*cx + cy*cy);
                
                // Circle shape
                if (dist <= 1.0f) {
                    // Determine if this pixel is in the "mouth" area
                    bool in_mouth = false;
                    
                    if (mouth_phase > 0) {
                        float angle = std::atan2(cy, cx) * 180.0f / 3.14159f;
                        
                        // Adjust angle based on direction
                        float mouth_angle = 0;
                        if (pacman.dir.x > 0) mouth_angle = 0;       // Right
                        else if (pacman.dir.x < 0) mouth_angle = 180; // Left
                        else if (pacman.dir.y < 0) mouth_angle = -90; // Up
                        else if (pacman.dir.y > 0) mouth_angle = 90;  // Down
                        
                        float relative_angle = angle - mouth_angle;
                        while (relative_angle < -180) relative_angle += 360;
                        while (relative_angle > 180) relative_angle -= 360;
                        
                        float mouth_width = 30.0f * mouth_phase / 3.0f; // 0-30 degrees
                        in_mouth = (std::abs(relative_angle) < mouth_width);
                    }
                    
                    if (!in_mouth) {
                        matrix->SetPixel(px, py, 255, 255, 0); // Yellow
                    }
                }
            }
        }
    }
    
    void PacmanGameScene::draw_ghost(RGBMatrixBase* matrix, const Entity& ghost, int x, int y, int scale, int off_x, int off_y) {
        Color color = ghost.color;
        
        // Frightened ghosts are blue
        if (ghost.mode == FRIGHTENED) {
            color = {33, 33, 255};
            // Flashing when timer is low
            if (ghost.frightened_timer < 10 && (animation_frame / 2) % 2 == 0) {
                color = {255, 255, 255};
            }
        } else if (ghost.mode == EATEN) {
            // Just eyes
            color = {255, 255, 255};
        }
        
        for(int dy = 0; dy < scale; dy++) {
            for(int dx = 0; dx < scale; dx++) {
                int px = off_x + x + dx;
                int py = off_y + y + dy;
                
                float cx = (dx - scale/2.0f) / (scale/2.0f);
                float cy = (dy - scale/2.0f) / (scale/2.0f);
                
                // Ghost body shape (rounded top, wavy bottom)
                bool draw = false;
                
                if (cy < 0) {
                    // Top half - circle
                    float dist = std::sqrt(cx*cx + cy*cy);
                    draw = (dist <= 1.0f);
                } else {
                    // Bottom half - wavy skirt
                    if (std::abs(cx) <= 1.0f) {
                        float wave = 0.1f * std::sin(cx * 6.28f + animation_frame * 0.3f);
                        draw = (cy <= 0.8f + wave);
                    }
                }
                
                if (draw) {
                    if (ghost.mode == EATEN) {
                        // Draw only eyes for eaten ghosts
                        bool is_eye = false;
                        if (scale >= 4) {
                            int eye_y = scale / 3;
                            int left_eye_x = scale / 3;
                            int right_eye_x = 2 * scale / 3;
                            
                            if (dy == eye_y && (dx == left_eye_x || dx == right_eye_x)) {
                                is_eye = true;
                            }
                        }
                        
                        if (is_eye) {
                            matrix->SetPixel(px, py, 255, 255, 255);
                        }
                    } else {
                        matrix->SetPixel(px, py, color.r, color.g, color.b);
                        
                        // Draw eyes
                        if (scale >= 4 && ghost.mode != FRIGHTENED) {
                            int eye_y = scale / 3;
                            int left_eye_x = scale / 3;
                            int right_eye_x = 2 * scale / 3;
                            
                            if (dy == eye_y && (dx == left_eye_x || dx == right_eye_x)) {
                                matrix->SetPixel(px, py, 255, 255, 255); // White eyes
                            }
                            // Pupils
                            int pupil_offset_x = 0, pupil_offset_y = 0;
                            if (ghost.dir.x > 0) pupil_offset_x = 1;
                            else if (ghost.dir.x < 0) pupil_offset_x = -1;
                            if (ghost.dir.y > 0) pupil_offset_y = 1;
                            else if (ghost.dir.y < 0) pupil_offset_y = -1;
                            
                            if ((dy == eye_y + pupil_offset_y) && 
                                (dx == left_eye_x + pupil_offset_x || dx == right_eye_x + pupil_offset_x)) {
                                matrix->SetPixel(px, py, 0, 0, 0); // Black pupils
                            }
                        }
                    }
                }
            }
        }
    }
    
    void PacmanGameScene::draw_pellet(RGBMatrixBase* matrix, int x, int y, int scale, int off_x, int off_y, bool is_power) {
        if (is_power) {
            // Power pellet - larger and blinking
            int size = scale / 2;
            if (size < 2) size = 2;
            int brightness = 255;
            
            if ((animation_frame / 5) % 2 == 0) {
                brightness = 180;
            }
            
            int start = (scale - size) / 2;
            for(int dy = 0; dy < size; dy++) {
                for(int dx = 0; dx < size; dx++) {
                    matrix->SetPixel(off_x + x*scale + start + dx, 
                                   off_y + y*scale + start + dy, 
                                   brightness, brightness, brightness);
                }
            }
        } else {
            // Regular pellet - small dot
            if (scale > 2) {
                int size = std::max(1, scale / 4);
                int start = (scale - size) / 2;
                
                for(int dy = 0; dy < size; dy++) {
                    for(int dx = 0; dx < size; dx++) {
                        matrix->SetPixel(off_x + x*scale + start + dx, 
                                       off_y + y*scale + start + dy, 
                                       255, 183, 174);
                    }
                }
            } else {
                matrix->SetPixel(off_x + x*scale + scale/2, 
                               off_y + y*scale + scale/2, 
                               255, 183, 174);
            }
        }
    }

    bool PacmanGameScene::render(RGBMatrixBase *matrix) {
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update).count();
        
        int speed_delay = 1000 / game_speed->get();
        
        // Calculate interpolation factor (0.0 to 1.0)
        interpolation = std::min(1.0f, static_cast<float>(duration) / static_cast<float>(speed_delay));
        
        if (duration > speed_delay) {
            update_game();
            last_update = now;
            interpolation = 0.0f;
        }

        // Clear screen
        matrix->Fill(0, 0, 0);

        // Draw Map
        int w = matrix->width();
        int h = matrix->height();
        
        int map_w = map_layout[0].size();
        int map_h = map_layout.size();
        
        // Calculate scale to fit matrix
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

        // Draw walls
        for (int y = 0; y < map_h; y++) {
            for (int x = 0; x < map_w; x++) {
                char c = map_layout[y][x];
                if (c == '#') {
                    // Blue walls with slight gradient
                    int brightness = 200 + (x + y) % 55;
                    draw_rect(x, y, 0, 0, std::min(255, brightness));
                }
            }
        }
        
        // Draw regular pellets
        for (const auto& p : pellets) {
            draw_pellet(matrix, p.x, p.y, scale, off_x, off_y, false);
        }
        
        // Draw power pellets
        for (const auto& p : power_pellets) {
            draw_pellet(matrix, p.x, p.y, scale, off_x, off_y, true);
        }
        
        // Draw Ghosts (behind Pacman) with interpolation
        for (const auto& ghost : ghosts) {
            // Interpolate position
            float interp_x = ghost.prev_pos.x + (ghost.pos.x - ghost.prev_pos.x) * interpolation;
            float interp_y = ghost.prev_pos.y + (ghost.pos.y - ghost.prev_pos.y) * interpolation;
            
            int pixel_x = static_cast<int>(interp_x * scale);
            int pixel_y = static_cast<int>(interp_y * scale);
            
            draw_ghost(matrix, ghost, pixel_x, pixel_y, scale, off_x, off_y);
        }
        
        // Draw Pacman with interpolation
        if (!is_dying) {
            float interp_x = pacman.prev_pos.x + (pacman.pos.x - pacman.prev_pos.x) * interpolation;
            float interp_y = pacman.prev_pos.y + (pacman.pos.y - pacman.prev_pos.y) * interpolation;
            
            int pixel_x = static_cast<int>(interp_x * scale);
            int pixel_y = static_cast<int>(interp_y * scale);
            
            draw_pacman(matrix, pixel_x, pixel_y, scale, off_x, off_y);
        } else {
            // Draw death animation
            draw_pacman(matrix, pacman.pos.x * scale, pacman.pos.y * scale, scale, off_x, off_y);
        }
        
        // Draw score in corner (if space permits)
        if (scale >= 3 && w >= 64) {
            // Very simple score display using colored pixels
            // Each 100 points = one colored dot
            int score_dots = std::min(10, score / 100);
            for (int i = 0; i < score_dots; i++) {
                matrix->SetPixel(w - 2 - i*3, 1, 255, 255, 0);
            }
        }
        
        // Draw lives indicator
        if (scale >= 3) {
            for (int i = 0; i < lives; i++) {
                // Small Pacman icons
                int lx = 2 + i * 4;
                int ly = h - 3;
                matrix->SetPixel(lx, ly, 255, 255, 0);
                matrix->SetPixel(lx+1, ly, 255, 255, 0);
                matrix->SetPixel(lx, ly+1, 255, 255, 0);
            }
        }

        return true;
    }

    std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> PacmanGameSceneWrapper::create() {
        return {new PacmanGameScene(), [](Scenes::Scene *s) { delete s; }};
    }
}
