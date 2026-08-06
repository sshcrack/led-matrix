#include "FallingSandScene.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace {
constexpr float kPi = 3.14159265358979323846f;

void hsv_to_rgb(float h, float s, float v, uint8_t &r, uint8_t &g, uint8_t &b) {
    h = std::fmod(h, 360.0f);
    if (h < 0) h += 360.0f;
    const float c = v * s;
    const float x = c * (1.0f - std::fabs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    const float m = v - c;
    float rf = 0, gf = 0, bf = 0;
    if (h < 60) { rf = c; gf = x; }
    else if (h < 120) { rf = x; gf = c; }
    else if (h < 180) { gf = c; bf = x; }
    else if (h < 240) { gf = x; bf = c; }
    else if (h < 300) { rf = x; bf = c; }
    else { rf = c; bf = x; }
    r = static_cast<uint8_t>((rf + m) * 255.0f);
    g = static_cast<uint8_t>((gf + m) * 255.0f);
    b = static_cast<uint8_t>((bf + m) * 255.0f);
}

float triangle_wave(float value) {
    const float phase = value - std::floor(value);
    return 1.0f - 4.0f * std::fabs(phase - 0.5f);
}
}

using namespace GenerativeScenes;

void FallingSandScene::register_properties() {
    add_property(emitters_);
    add_property(spawn_rate_);
    add_property(water_);
    add_property(reset_fill_percent_);
    add_property(wind_enabled_);
    add_property(wind_strength_);
    add_property(explosions_enabled_);
    add_property(explosion_frequency_);
}

void FallingSandScene::initialize(int width, int height) {
    Scene::initialize(width, height);
    set_target_fps(45);
    reset();
    last_update_ = std::chrono::steady_clock::now();
    simulation_accumulator_ = 0.0f;
}

void FallingSandScene::reset() {
    cells_.assign(static_cast<size_t>(matrix_width * matrix_height), {});
    emitter_state_.clear();
    debris_.clear();
    dust_.clear();
    frame_ = 0;
    draining_ = false;
    drain_frames_ = 0;
    wind_ = 0.0f;
    wind_target_ = 0.0f;
    next_wind_change_ = 90 + static_cast<uint32_t>(rng_() % 120);
    wind_end_frame_ = 0;
    next_explosion_ = 220 + static_cast<uint32_t>(rng_() % 220);
    effect_cooldown_until_ = 0;
    wind_active_ = false;
    flash_until_ = 0;
    flash_started_ = 0;
}

void FallingSandScene::update_emitters() {
    const int count = std::clamp(emitters_->get(), 1, 16);
    if (static_cast<int>(emitter_state_.size()) != count) {
        emitter_state_.resize(static_cast<size_t>(count));
        std::uniform_real_distribution<float> phase(0.0f, 2.0f * kPi);
        for (int i = 0; i < count; ++i) {
            auto &e = emitter_state_[static_cast<size_t>(i)];
            e.x = static_cast<float>((i + 1) * matrix_width) / static_cast<float>(count + 1);
            e.velocity = (i & 1) ? 0.16f : -0.16f;
            e.phase = phase(rng_);
            e.mode = static_cast<uint8_t>(i % 4);
            e.hue_offset = static_cast<uint8_t>(i * 41);
            e.mode_until = frame_ + 420 + static_cast<uint32_t>(i * 47);
        }
    }

    std::uniform_real_distribution<float> jitter(-0.055f, 0.055f);
    std::uniform_int_distribution<int> mode_pick(0, 3);
    for (size_t i = 0; i < emitter_state_.size(); ++i) {
        auto &e = emitter_state_[i];
        if (frame_ >= e.mode_until) {
            e.mode = static_cast<uint8_t>(mode_pick(rng_));
            e.mode_until = frame_ + 360 + static_cast<uint32_t>(rng_() % 540);
            e.velocity = (rng_() & 1U) ? 0.14f : -0.14f;
        }
        const float t = static_cast<float>(frame_) / 45.0f;
        const float lane_center = static_cast<float>((static_cast<int>(i) + 1) * matrix_width)
            / static_cast<float>(static_cast<int>(emitter_state_.size()) + 1);
        const float lane_width = std::max(4.0f, static_cast<float>(matrix_width)
            / static_cast<float>(emitter_state_.size() + 1) * 0.75f);
        switch (e.mode) {
            case 0: // smooth sine
                e.x = lane_center + std::sin(t * 0.32f + e.phase) * lane_width;
                break;
            case 1: // hard sweeping triangle
                e.x = lane_center + triangle_wave(t * 0.075f + e.phase) * lane_width;
                break;
            case 2: // bouncing nozzle
                e.x += e.velocity;
                if (e.x < 1.0f || e.x > matrix_width - 2.0f) {
                    e.velocity = -e.velocity;
                    e.x = std::clamp(e.x, 1.0f, static_cast<float>(matrix_width - 2));
                }
                break;
            default: // wandering nozzle
                e.velocity = std::clamp(e.velocity + jitter(rng_), -0.28f, 0.28f);
                e.x += e.velocity;
                if (e.x < 1.0f || e.x > matrix_width - 2.0f) e.velocity = -e.velocity;
                e.x = std::clamp(e.x, 1.0f, static_cast<float>(matrix_width - 2));
                break;
        }
    }
}

void FallingSandScene::update_wind() {
    if (!wind_enabled_->get() || draining_) {
        wind_active_ = false;
        wind_target_ = 0.0f;
        wind_ += (wind_target_ - wind_) * 0.08f;
        return;
    }

    // Wind is an occasional event, not a permanent layer. A gust starts only
    // when no other major effect is in its cooldown window.
    if (!wind_active_ && frame_ >= next_wind_change_ && frame_ >= effect_cooldown_until_) {
        const float strength = std::clamp(wind_strength_->get(), 0, 100) / 100.0f;
        if (strength > 0.01f && (rng_() % 100) < 72) {
            std::uniform_real_distribution<float> magnitude(0.35f * strength, strength);
            wind_target_ = magnitude(rng_) * ((rng_() & 1U) ? 1.0f : -1.0f);
            wind_active_ = true;
            wind_end_frame_ = frame_ + 90 + static_cast<uint32_t>(rng_() % 210);
        } else {
            next_wind_change_ = frame_ + 120 + static_cast<uint32_t>(rng_() % 300);
        }
    }

    if (wind_active_ && frame_ >= wind_end_frame_) {
        wind_active_ = false;
        wind_target_ = 0.0f;
        effect_cooldown_until_ = frame_ + 80 + static_cast<uint32_t>(rng_() % 160);
        next_wind_change_ = effect_cooldown_until_ + 120 + static_cast<uint32_t>(rng_() % 360);
    }

    wind_ += (wind_target_ - wind_) * (wind_active_ ? 0.035f : 0.075f);
}

void FallingSandScene::trigger_explosion() {
    if (!explosions_enabled_->get() || draining_ || matrix_width < 16 || matrix_height < 16) return;

    // Prefer a point embedded in the upper half of the pile so the blast is
    // visible and can throw material across most of the matrix.
    std::vector<std::pair<int, int>> candidates;
    for (int y = matrix_height / 5; y < matrix_height - 3; y += 2) {
        for (int x = 3; x < matrix_width - 3; x += 2) {
            if (cells_[index(x, y)].type != 0) candidates.emplace_back(x, y);
        }
    }
    if (candidates.empty()) return;
    const auto [cx, cy] = candidates[rng_() % candidates.size()];
    const int radius = std::clamp(std::min(matrix_width, matrix_height) / 6, 9, 26);
    const float influence = radius * 1.65f;
    std::uniform_real_distribution<float> unit(-1.0f, 1.0f);
    std::uniform_real_distribution<float> dust_angle(0.0f, 2.0f * kPi);

    for (int y = std::max(0, static_cast<int>(cy - influence));
         y <= std::min(matrix_height - 1, static_cast<int>(cy + influence)); ++y) {
        for (int x = std::max(0, static_cast<int>(cx - influence));
             x <= std::min(matrix_width - 1, static_cast<int>(cx + influence)); ++x) {
            const float dx = static_cast<float>(x - cx);
            const float dy = static_cast<float>(y - cy);
            const float dist = std::sqrt(dx * dx + dy * dy);
            if (dist > influence) continue;

            Cell &source = cells_[index(x, y)];
            if (source.type != 0) {
                const float normalized = dist / influence;
                const int launch_chance = static_cast<int>(96.0f - normalized * 50.0f);
                if (dist <= radius || static_cast<int>(rng_() % 100) < launch_chance) {
                    Cell cell = source;
                    source = {};
                    if (debris_.size() < 2600) {
                        const float inv = 1.0f / std::max(1.0f, dist);
                        const float force = 1.1f + (1.0f - normalized) * 4.6f;
                        debris_.push_back({static_cast<float>(x), static_cast<float>(y),
                            dx * inv * force + unit(rng_) * 0.65f,
                            dy * inv * force - 2.2f - (1.0f - normalized) * 2.8f + unit(rng_) * 0.35f,
                            cell, static_cast<uint16_t>(90 + rng_() % 150)});
                    }
                }
            }

            // Dense dust in the blast cavity and a lighter cloud beyond it.
            const int dust_chance = dist <= radius ? 58 : 18;
            if (dust_.size() < 1800 && static_cast<int>(rng_() % 100) < dust_chance) {
                const float angle = dust_angle(rng_);
                const float speed = 0.25f + (1.0f - std::min(1.0f, dist / influence)) * 1.65f
                    + static_cast<float>(rng_() % 100) / 130.0f;
                const uint16_t life = static_cast<uint16_t>(70 + rng_() % 150);
                dust_.push_back({static_cast<float>(x), static_cast<float>(y),
                    std::cos(angle) * speed + dx / std::max(3.0f, dist) * 0.8f,
                    std::sin(angle) * speed - 0.45f,
                    life, life, static_cast<uint8_t>(rng_() % 100)});
            }
        }
    }

    // Extra fine dust radiates far beyond the excavated material, making the
    // impact readable even when the pile is still fairly shallow.
    const int extra_dust = std::min(900, matrix_width * 5);
    for (int i = 0; i < extra_dust && dust_.size() < 2200; ++i) {
        const float angle = dust_angle(rng_);
        const float speed = 0.45f + static_cast<float>(rng_() % 100) / 32.0f;
        const uint16_t life = static_cast<uint16_t>(80 + rng_() % 190);
        dust_.push_back({static_cast<float>(cx) + unit(rng_) * 3.0f,
            static_cast<float>(cy) + unit(rng_) * 3.0f,
            std::cos(angle) * speed,
            std::sin(angle) * speed - 1.0f,
            life, life, static_cast<uint8_t>(rng_() % 100)});
    }

    flash_x_ = static_cast<float>(cx);
    flash_y_ = static_cast<float>(cy);
    flash_radius_ = static_cast<float>(radius);
    flash_started_ = frame_;
    flash_until_ = frame_ + 34;
}

void FallingSandScene::update_debris() {
    for (auto &p : debris_) {
        if (p.life == 0) continue;
        p.vx += wind_ * 0.025f;
        p.vy += 0.11f;
        const float nx = p.x + p.vx;
        const float ny = p.y + p.vy;
        const int ix = static_cast<int>(std::round(nx));
        const int iy = static_cast<int>(std::round(ny));
        if (!inside(ix, iy)) {
            if (iy >= matrix_height || ix < 0 || ix >= matrix_width) p.life = 0;
            else { p.vx *= -0.45f; p.vy *= -0.3f; }
        } else if (cells_[index(ix, iy)].type == 0) {
            p.x = nx; p.y = ny;
        } else if (std::fabs(p.vy) < 0.7f || p.life < 12) {
            const int px = std::clamp(static_cast<int>(std::round(p.x)), 0, matrix_width - 1);
            const int py = std::clamp(static_cast<int>(std::round(p.y)), 0, matrix_height - 1);
            if (cells_[index(px, py)].type == 0) cells_[index(px, py)] = p.cell;
            p.life = 0;
        } else {
            p.vy *= -0.34f;
            p.vx *= 0.72f;
        }
        if (p.life > 0) --p.life;
    }
    debris_.erase(std::remove_if(debris_.begin(), debris_.end(), [](const Debris &p) {
        return p.life == 0;
    }), debris_.end());
}

void FallingSandScene::update_dust() {
    for (auto &p : dust_) {
        if (p.life == 0) continue;
        p.vx += wind_ * 0.018f;
        p.vx *= 0.992f;
        p.vy = p.vy * 0.988f + 0.018f;
        p.x += p.vx;
        p.y += p.vy;
        if (p.x < -3.0f || p.x > matrix_width + 3.0f || p.y < -4.0f || p.y > matrix_height + 3.0f) {
            p.life = 0;
        } else {
            --p.life;
        }
    }
    dust_.erase(std::remove_if(dust_.begin(), dust_.end(), [](const Dust &p) {
        return p.life == 0;
    }), dust_.end());
}

void FallingSandScene::simulate_step() {
    ++frame_;
    if (draining_) ++drain_frames_;
    std::uniform_int_distribution<int> chance(0, 99);

    update_wind();
    update_emitters();

    const int frequency = std::clamp(explosion_frequency_->get(), 0, 100);
    // Explosions are rare punctuation. They are suppressed during gusts,
    // draining, and the shared effect cooldown so the scene never fires every
    // spectacle at once.
    if (explosions_enabled_->get() && frequency > 0 && !wind_active_ && !draining_ && frame_ >= next_explosion_
        && frame_ >= effect_cooldown_until_) {
        const int trigger_chance = std::clamp(18 + frequency / 2, 18, 68);
        if (static_cast<int>(rng_() % 100) < trigger_chance) {
            trigger_explosion();
            effect_cooldown_until_ = frame_ + 120 + static_cast<uint32_t>(rng_() % 220);
        }
        const uint32_t base_delay = static_cast<uint32_t>(std::max(240, 1050 - frequency * 7));
        next_explosion_ = frame_ + base_delay + static_cast<uint32_t>(rng_() % (base_delay + 1));
    }

    if (!draining_) {
        const int rate = std::clamp(spawn_rate_->get(), 1, 12);
        for (size_t e = 0; e < emitter_state_.size(); ++e) {
            const int cx = std::clamp(static_cast<int>(std::round(emitter_state_[e].x)), 0, matrix_width - 1);
            for (int n = 0; n < rate; ++n) {
                const int spread = emitter_state_[e].mode == 1 ? 5 : 3;
                const int x = std::clamp(cx + static_cast<int>(rng_() % (spread * 2 + 1)) - spread, 0, matrix_width - 1);
                const int y = static_cast<int>(rng_() % 2);
                Cell &c = cells_[index(x, y)];
                if (c.type == 0) {
                    c.type = (water_->get() && chance(rng_) < 18) ? 2 : 1;
                    c.hue = static_cast<uint8_t>((frame_ / 2 + emitter_state_[e].hue_offset + n * 3) % 255);
                }
            }
        }
    }

    const int drain_count = matrix_width >= 48 ? 3 : 1;
    const int base_half_width = std::max(1, matrix_width / 32);
    const int max_half_width = std::max(base_half_width, matrix_width / (drain_count * 5));
    const int drain_half_width = std::min(max_half_width,
        base_half_width + static_cast<int>(drain_frames_ / 50));
    auto nearest_drain_center = [&](int x) {
        int best = matrix_width / (drain_count + 1);
        int best_dist = std::abs(x - best);
        for (int d = 1; d < drain_count; ++d) {
            const int center = ((d + 1) * matrix_width) / (drain_count + 1);
            const int dist = std::abs(x - center);
            if (dist < best_dist) { best = center; best_dist = dist; }
        }
        return best;
    };
    auto is_drain_x = [&](int x) {
        return std::abs(x - nearest_drain_center(x)) <= drain_half_width;
    };
    if (draining_) {
        for (int y = std::max(0, matrix_height - 3); y < matrix_height; ++y)
            for (int x = 0; x < matrix_width; ++x)
                if (is_drain_x(x)) cells_[index(x, y)] = {};
    }

    for (int y = matrix_height - 2; y >= 0; --y) {
        const bool left_first = ((frame_ + y) & 1U) == 0;
        for (int xi = 0; xi < matrix_width; ++xi) {
            const int x = left_first ? xi : matrix_width - 1 - xi;
            Cell &c = cells_[index(x, y)];
            if (c.type == 0) continue;
            auto move = [&](int nx, int ny) { cells_[index(nx, ny)] = c; c = {}; };
            if (cells_[index(x, y + 1)].type == 0) { move(x, y + 1); continue; }

            const int wind_dir = wind_ > 0.03f ? 1 : (wind_ < -0.03f ? -1 : 0);
            const int wind_probability = static_cast<int>(std::fabs(wind_) * (c.type == 2 ? 85.0f : 42.0f));
            if (wind_dir != 0 && chance(rng_) < wind_probability && inside(x + wind_dir, y)
                && cells_[index(x + wind_dir, y)].type == 0) {
                move(x + wind_dir, y);
                continue;
            }

            if (c.type == 1) {
                int d = ((x + y + static_cast<int>(frame_)) & 1) ? 1 : -1;
                if (inside(x + d, y + 1) && cells_[index(x + d, y + 1)].type == 0) { move(x + d, y + 1); continue; }
                d = -d;
                if (inside(x + d, y + 1) && cells_[index(x + d, y + 1)].type == 0) { move(x + d, y + 1); continue; }
                if (draining_) {
                    const int center = nearest_drain_center(x);
                    const int toward = center > x ? 1 : (center < x ? -1 : 0);
                    if (toward && inside(x + toward, y) && cells_[index(x + toward, y)].type == 0) {
                        move(x + toward, y);
                        continue;
                    }
                }
            } else {
                int d = wind_dir != 0 ? wind_dir : (((x + static_cast<int>(frame_)) & 1) ? 1 : -1);
                bool moved = false;
                for (int pass = 0; pass < 2 && !moved; ++pass, d = -d) {
                    for (int dist = 1; dist <= 5; ++dist) {
                        const int nx = x + d * dist;
                        if (!inside(nx, y)) break;
                        if (cells_[index(nx, y)].type == 0) { move(nx, y); moved = true; break; }
                    }
                }
            }
        }
    }

    update_debris();
    update_dust();


    int occupied = 0;
    for (const auto &cell : cells_) if (cell.type != 0) ++occupied;
    const int total = matrix_width * matrix_height;
    const int threshold = std::clamp(reset_fill_percent_->get(), 25, 95);
    if (!draining_ && occupied * 100 >= total * threshold) {
        draining_ = true;
        drain_frames_ = 0;
    } else if (draining_ && occupied * 100 <= total * 3) {
        draining_ = false;
        drain_frames_ = 0;
    }


}

void FallingSandScene::draw(rgb_matrix::FrameCanvas *canvas) {
    canvas->Clear();
    for (int y = 0; y < matrix_height; ++y) {
        for (int x = 0; x < matrix_width; ++x) {
            const Cell &c = cells_[index(x, y)];
            if (!c.type) continue;
            uint8_t r, g, b;
            if (c.type == 2) { r = 20; g = 95; b = 255; }
            else hsv_to_rgb(c.hue * 360.0f / 255.0f, 0.82f, 1.0f, r, g, b);
            canvas->SetPixel(x, y, r, g, b);
        }
    }
    for (const auto &p : dust_) {
        const int x = static_cast<int>(std::round(p.x));
        const int y = static_cast<int>(std::round(p.y));
        if (!inside(x, y)) continue;
        const float alpha = static_cast<float>(p.life) / static_cast<float>(p.max_life);
        const uint8_t intensity = static_cast<uint8_t>(std::clamp(18.0f + alpha * 115.0f, 0.0f, 255.0f));
        const uint8_t r = static_cast<uint8_t>(std::min(255, static_cast<int>(intensity) + 38));
        const uint8_t g = static_cast<uint8_t>(std::min(255, static_cast<int>(intensity) + (p.warmth < 45 ? 20 : 4)));
        const uint8_t b = static_cast<uint8_t>(std::max(4, static_cast<int>(intensity) / 3));
        canvas->SetPixel(x, y, r, g, b);
    }
    for (const auto &p : debris_) {
        const int x = static_cast<int>(std::round(p.x));
        const int y = static_cast<int>(std::round(p.y));
        if (!inside(x, y)) continue;
        uint8_t r, g, b;
        if (p.cell.type == 2) { r = 80; g = 170; b = 255; }
        else hsv_to_rgb(p.cell.hue * 360.0f / 255.0f, 0.72f, 1.0f, r, g, b);
        canvas->SetPixel(x, y, r, g, b);
    }
    if (frame_ < flash_until_) {
        const float progress = static_cast<float>(frame_ - flash_started_)
            / static_cast<float>(std::max<uint32_t>(1, flash_until_ - flash_started_));
        // White-hot core at impact, followed by three expanding shock rings.
        if (progress < 0.22f) {
            const int core = std::max(2, static_cast<int>(flash_radius_ * (0.34f - progress)));
            for (int y = static_cast<int>(flash_y_) - core; y <= static_cast<int>(flash_y_) + core; ++y)
                for (int x = static_cast<int>(flash_x_) - core; x <= static_cast<int>(flash_x_) + core; ++x)
                    if (inside(x, y) && (x-flash_x_)*(x-flash_x_) + (y-flash_y_)*(y-flash_y_) <= core*core)
                        canvas->SetPixel(x, y, 255, 245, 205);
        }
        for (int ring = 0; ring < 3; ++ring) {
            const float ring_progress = std::clamp(progress - ring * 0.09f, 0.0f, 1.0f);
            const float radius = flash_radius_ * (0.25f + ring_progress * (2.2f + ring * 0.28f));
            for (int a = 0; a < 360; a += 3) {
                const float rad = static_cast<float>(a) * kPi / 180.0f;
                const int x = static_cast<int>(std::round(flash_x_ + std::cos(rad) * radius));
                const int y = static_cast<int>(std::round(flash_y_ + std::sin(rad) * radius));
                if (inside(x, y)) canvas->SetPixel(x, y,
                    static_cast<uint8_t>(255 - ring * 28),
                    static_cast<uint8_t>(205 - ring * 45),
                    static_cast<uint8_t>(80 - ring * 20));
            }
        }
    }


}

bool FallingSandScene::render(rgb_matrix::FrameCanvas *canvas) {
    if (cells_.size() != static_cast<size_t>(matrix_width * matrix_height)) reset();

    const auto now = std::chrono::steady_clock::now();
    float elapsed = std::chrono::duration<float>(now - last_update_).count();
    last_update_ = now;
    elapsed = std::clamp(elapsed, 0.0f, 0.25f);
    simulation_accumulator_ += elapsed;

    // Physics runs at a fixed real-world rate on both the Pi and desktop.
    // 26 Hz keeps the simulation lively while remaining identical on the Pi and emulator.
    constexpr float simulation_step = 1.0f / 26.0f;
    int steps = 0;
    while (simulation_accumulator_ >= simulation_step && steps < 3) {
        simulate_step();
        simulation_accumulator_ -= simulation_step;
        ++steps;
    }
    // Never enter an expensive catch-up spiral after a temporary stall.
    if (steps == 3 && simulation_accumulator_ >= simulation_step)
        simulation_accumulator_ = std::fmod(simulation_accumulator_, simulation_step);

    draw(canvas);
    wait_until_next_frame();
    return true;
}

std::unique_ptr<Scenes::Scene> FallingSandSceneWrapper::create() {
    return std::make_unique<FallingSandScene>();
}
