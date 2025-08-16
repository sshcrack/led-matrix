#include "CountdownScene.h"
#include "../Constants.h"

#include <chrono>
#include <cmath>
#include <random>
#include <ctime>
#include "spdlog/spdlog.h"

using namespace Scenes;
using namespace std::chrono;

CountdownScene::CountdownScene()
{
    std::random_device rd;
    rng.seed(rd());
}

void CountdownScene::spawn_confetti(int count)
{
    std::uniform_real_distribution<float> ux(0, width);
    std::uniform_real_distribution<float> uy(0, height / 2.0f);
    std::uniform_real_distribution<float> uvx(-10.0f, 10.0f);
    std::uniform_real_distribution<float> uvy(10.0f, 30.0f);
    std::uniform_int_distribution<int> uc(0, 255);

    const size_t MAX_CONFETTI = 600; // increased cap for richer confetti
    for (int i = 0; i < count; i++)
    {
        Particle p;
        p.x = ux(rng);
        p.y = uy(rng);
        p.vx = uvx(rng);
        p.vy = uvy(rng);
        p.color = rgb_matrix::Color(uc(rng), uc(rng), uc(rng));
        p.life = 2.0f + (uc(rng) % 100) / 100.0f;
        if (confetti_particles.size() < MAX_CONFETTI)
            confetti_particles.push_back(p);
    }
}

void CountdownScene::spawn_firework(float x, float y)
{
    std::uniform_real_distribution<float> uangle(0, 2.0f * M_PI);
    std::uniform_real_distribution<float> uspeed(20.0f, 60.0f);
    std::uniform_int_distribution<int> uc(0, 255);

    int pieces = 32;                   // more pieces per firework for fuller bursts
    const size_t MAX_FIREWORKS = 1200; // increased cap for more simultaneous fireworks
    for (int i = 0; i < pieces; i++)
    {
        float a = uangle(rng);
        float s = uspeed(rng);
        Particle p;
        p.x = x;
        p.y = y;
        p.vx = cos(a) * s;
        p.vy = sin(a) * s;
        p.color = rgb_matrix::Color(uc(rng), uc(rng), uc(rng));
        p.life = 1.5f + (uc(rng) % 100) / 100.0f;
        if (fireworks_particles.size() < MAX_FIREWORKS)
            fireworks_particles.push_back(p);
    }
}

void CountdownScene::update_particles(float dt)
{
    auto update = [&](std::vector<Particle> &vec)
    {
        for (auto &p : vec)
        {
            p.x += p.vx * dt;
            p.y -= p.vy * dt;         // y up
            p.vy -= 9.8f * dt * 3.0f; // gravity
            p.life -= dt;
        }
        // remove dead
        vec.erase(std::remove_if(vec.begin(), vec.end(), [](const Particle &p)
                                 { return p.life <= 0.0f; }),
                  vec.end());
    };
    update(confetti_particles);
    update(fireworks_particles);
}

// We now use the shared BDF fonts (HEADER_FONT/BODY_FONT/SMALL_FONT) for rendering digits

bool CountdownScene::render(RGBMatrixBase *matrix)
{
    auto ft = frameTimer.tick();
    float dt = ft.dt;

    width = matrix->width();
    height = matrix->height();

    // Clear with a subtle pulse background
    float pulse = (std::sin(ft.t * pulse_speed->get()) + 1.0f) / 2.0f;
    uint8_t bg = static_cast<uint8_t>(20 + pulse * 40);
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            offscreen_canvas->SetPixel(x, y, bg / 2, bg / 3, bg);
        }
    }

    // Determine time left until target year (assume Jan 1 target_year at midnight UTC)
    auto now = system_clock::now();
    std::tm tm_target = {};
    tm_target.tm_year = target_year->get() - 1900;
    tm_target.tm_mon = 0; // Jan
    tm_target.tm_mday = 1;
    tm_target.tm_hour = 0;
    tm_target.tm_min = 0;
    tm_target.tm_sec = 0;
    time_t target_time_t = timegm(&tm_target);
    system_clock::time_point target_tp = system_clock::from_time_t(target_time_t);

    auto diff = duration_cast<seconds>(target_tp - now);
    long long remaining = diff.count();
    bool is_past = remaining <= 0;

    if (is_past)
        remaining = 0;
    // Determine whether we're in the New Year's window: Dec 31 (any time) or Jan 1 within first 5 hours
    bool new_year_window = false;

#ifndef ENABLE_EMULATOR
    {
        time_t now_time_t = time(nullptr);
        std::tm now_tm;
        gmtime_r(&now_time_t, &now_tm);
        // tm_mon: 0 = Jan, 11 = Dec
        if (now_tm.tm_mon == 11 && now_tm.tm_mday == 31)
        {
            new_year_window = true; // Dec 31
        }
        else if (now_tm.tm_mon == 0 && now_tm.tm_mday == 1 && now_tm.tm_hour < 5)
        {
            new_year_window = true; // Jan 1 first 5 hours past midnight
        }
    }

#else
    // If debug is enabled, force a short time left and enable visual effects for quick testing
    if (debug->get())
    {
        remaining = 8; // 8 seconds left for quick fireworks
        is_past = false;
        new_year_window = true;
    }
#endif

    int days = remaining / 86400;
    int hours = (remaining % 86400) / 3600;
    int minutes = (remaining % 3600) / 60;
    int seconds = remaining % 60;

    // Build a display string like DD:HH:MM or HH:MM:SS depending on space
    // We'll render big digits centered horizontally
    std::string left = std::to_string(days);
    // For simplicity, show HH:MM:SS if days==0 or if show_seconds enabled
    char buffer[64];
    if (days > 0)
    {
        snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", days, hours, minutes);
    }
    else if (show_seconds->get())
    {
        snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", hours, minutes, seconds);
    }
    else
    {
        snprintf(buffer, sizeof(buffer), "%02d:%02d", hours, minutes);
    }
    std::string disp = buffer;

    // Draw digits using the loaded fonts. Choose color
    rgb_matrix::Color text_color = digit_color->get();

    // Choose font and vertical position based on big_digits flag
    if (big_digits->get())
    {
        // Use HEADER_FONT as large font; draw centered horizontally
        int baseline = HEADER_FONT.baseline();
        // Compute text width using CharacterWidth per character (handles variable width fonts)
        int text_width = 0;
        for (char c : disp)
            text_width += HEADER_FONT.CharacterWidth(static_cast<uint32_t>(c));
        int text_x = (width - text_width) / 2;
        int text_y = (height + baseline) / 2;
        rgb_matrix::DrawText(offscreen_canvas, HEADER_FONT, text_x, text_y, text_color, disp.c_str());
    }
    else
    {
        int baseline = BODY_FONT.baseline();
        int text_width = 0;
        for (char c : disp)
            text_width += BODY_FONT.CharacterWidth(static_cast<uint32_t>(c));
        int text_x = (width - text_width) / 2;
        int text_y = (height + baseline) / 2;
        rgb_matrix::DrawText(offscreen_canvas, BODY_FONT, text_x, text_y, text_color, disp.c_str());
    }

    // Particle effects
    if ((confetti->get()
#ifdef ENABLE_EMULATOR
         || debug->get()
#endif
             ) &&
        new_year_window)
    {
        // throttle confetti spawns: at most every 0.5s
        if (last_confetti_spawn < 0 || ft.t - last_confetti_spawn >= 0.5f)
        {
            spawn_confetti(2);
            last_confetti_spawn = ft.t;
        }
    }

    // Trigger fireworks when less than 10 seconds to target and fireworks enabled
    if ((fireworks->get()
#ifdef ENABLE_EMULATOR
         || debug->get()
#endif
             ) &&
        !is_past && remaining <= 10 && new_year_window)
    {
        // spawn one firework randomly, throttle to reduce load
        if (last_firework_spawn < 0 || ft.t - last_firework_spawn >= 0.8f)
        {
            std::uniform_real_distribution<float> ux(0, width);
            float fx = ux(rng);
            float fy = height * 0.6f;
            spawn_firework(fx, fy);
            last_firework_spawn = ft.t;
        }
    }

    update_particles(dt);

    // render particles
    auto render_particles = [&](const std::vector<Particle> &vec)
    {
        // If there are many particles, sample to reduce draw calls (threshold raised)
        size_t sample_step = vec.size() > 800 ? 2 : 1;
        for (size_t idx = 0; idx < vec.size(); idx += sample_step)
        {
            const auto &p = vec[idx];
            int x = static_cast<int>(p.x);
            int y = static_cast<int>(p.y);
            if (x >= 0 && x < width && y >= 0 && y < height)
            {
                float life_ratio = std::max(0.0f, std::min(1.0f, p.life));
                uint8_t rr = static_cast<uint8_t>(p.color.r * life_ratio);
                uint8_t gg = static_cast<uint8_t>(p.color.g * life_ratio);
                uint8_t bb = static_cast<uint8_t>(p.color.b * life_ratio);
                offscreen_canvas->SetPixel(x, y, rr, gg, bb);
            }
        }
    };

    render_particles(confetti_particles);
    render_particles(fireworks_particles);

    // If reached target, show celebratory message (simple)
    if (is_past)
    {
        std::string msg = "HAPPY " + std::to_string(target_year->get());
        int px = (width - (int)msg.size() * 6) / 2;
        int py = 2;
        for (size_t i = 0; i < msg.size(); i++)
        {
            char c = msg[i];
            // draw simple block per char
            for (int oy = 0; oy < 4; oy++)
            {
                for (int ox = 0; ox < 4; ox++)
                {
                    int x = px + i * 6 + ox;
                    int y = py + oy;
                    if (x >= 0 && x < width && y >= 0 && y < height)
                        offscreen_canvas->SetPixel(x, y, 255, 220, 100);
                }
            }
        }
        // continuous fireworks
        if ((fireworks->get()
#ifdef ENABLE_EMULATOR
             || debug->get()
#endif
                 ) &&
            new_year_window)
        {
            if ((int)(ft.t * 2) % 2 == 0)
            {
                std::uniform_real_distribution<float> ux(0, width);
                spawn_firework(ux(rng), height * 0.5f);
            }
        }
    }

    return true;
}

string CountdownScene::get_name() const
{
    return "countdown";
}

void CountdownScene::register_properties()
{
    add_property(target_year);
    add_property(fireworks);
    add_property(confetti);
    add_property(big_digits);
    add_property(pulse_speed);
    add_property(digit_color);
    add_property(show_seconds);
#ifdef ENABLE_EMULATOR
    add_property(debug);
#endif
}

tmillis_t CountdownScene::get_default_duration()
{
    return 30000; // 30 seconds default
}

int CountdownScene::get_default_weight()
{
    return 50;
}

int CountdownScene::get_weight() const
{
    // Only enable this scene around New Year's Eve / first 5 hours of Jan 1.
#ifdef ENABLE_EMULATOR
    return weight->get();
#else
    // Determine current date in UTC
    time_t now_time_t = time(nullptr);
    std::tm now_tm;
    gmtime_r(&now_time_t, &now_tm);
    bool new_year_window = false;
    if (now_tm.tm_mon == 11 && now_tm.tm_mday == 31)
        new_year_window = true;
    if (now_tm.tm_mon == 0 && now_tm.tm_mday == 1 && now_tm.tm_hour < 5)
        new_year_window = true;
    return new_year_window ? weight->get() : 0;
#endif
}

std::unique_ptr<Scene, void (*)(Scene *)> CountdownSceneWrapper::create()
{
    return {new CountdownScene(), [](Scene *scene)
            { delete scene; }};
}
