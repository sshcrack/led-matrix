#include "ChristmasCountdownScene.h"
#include "../Constants.h"
#include "graphics.h"
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace Scenes {

ChristmasCountdownScene::ChristmasCountdownScene() 
    : rng(std::random_device{}()) {
}

int ChristmasCountdownScene::getDaysUntilChristmas() {
    std::time_t now = std::time(nullptr);
    std::tm* now_tm = std::localtime(&now);
    
    int current_year = now_tm->tm_year + 1900;
    int current_month = now_tm->tm_mon + 1;
    int current_day = now_tm->tm_mday;
    
    // Determine which Christmas to count down to
    int target_year = current_year;
    if (current_month == 12 && current_day > 25) {
        target_year++;
    }
    
    // Create Christmas date
    std::tm christmas = {};
    christmas.tm_year = target_year - 1900;
    christmas.tm_mon = 11; // December (0-indexed)
    christmas.tm_mday = 25;
    christmas.tm_hour = 0;
    christmas.tm_min = 0;
    christmas.tm_sec = 0;
    
    std::time_t christmas_time = std::mktime(&christmas);
    
    // Calculate difference in seconds
    double diff_seconds = std::difftime(christmas_time, now);
    int days = static_cast<int>(diff_seconds / (60 * 60 * 24));
    
    return std::max(0, days);
}

int ChristmasCountdownScene::getHoursUntilChristmas() {
    std::time_t now = std::time(nullptr);
    std::tm* now_tm = std::localtime(&now);
    
    int current_year = now_tm->tm_year + 1900;
    int current_month = now_tm->tm_mon + 1;
    int current_day = now_tm->tm_mday;
    
    // Determine which Christmas to count down to
    int target_year = current_year;
    if (current_month == 12 && current_day > 25) {
        target_year++;
    }
    
    // Create Christmas date
    std::tm christmas = {};
    christmas.tm_year = target_year - 1900;
    christmas.tm_mon = 11; // December (0-indexed)
    christmas.tm_mday = 25;
    christmas.tm_hour = 0;
    christmas.tm_min = 0;
    christmas.tm_sec = 0;
    
    std::time_t christmas_time = std::mktime(&christmas);
    
    // Calculate difference in seconds
    double diff_seconds = std::difftime(christmas_time, now);
    int hours = static_cast<int>(diff_seconds / (60 * 60));
    
    return std::max(0, hours);
}

void ChristmasCountdownScene::spawnSparkle() {
    std::uniform_real_distribution<float> x_dist(0.0f, static_cast<float>(matrix_width));
    std::uniform_real_distribution<float> y_dist(0.0f, static_cast<float>(matrix_height));
    std::uniform_real_distribution<float> vx_dist(-10.0f, 10.0f);
    std::uniform_real_distribution<float> vy_dist(-10.0f, 10.0f);
    std::uniform_real_distribution<float> life_dist(0.5f, 2.0f);
    std::uniform_int_distribution<int> color_dist(0, 2);
    
    Sparkle sparkle;
    sparkle.x = x_dist(rng);
    sparkle.y = y_dist(rng);
    sparkle.vx = vx_dist(rng);
    sparkle.vy = vy_dist(rng);
    sparkle.life = 1.0f;
    sparkle.max_life = life_dist(rng);
    
    // Random festive colors
    int color_choice = color_dist(rng);
    if (color_choice == 0) {
        sparkle.color = rgb_matrix::Color(255, 215, 0); // Gold
    } else if (color_choice == 1) {
        sparkle.color = rgb_matrix::Color(255, 255, 255); // White
    } else {
        sparkle.color = rgb_matrix::Color(200, 200, 255); // Light blue
    }
    
    sparkles.push_back(sparkle);
}

void ChristmasCountdownScene::setPixelSafe(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if (x >= 0 && x < matrix_width && y >= 0 && y < matrix_height) {
        offscreen_canvas->SetPixel(x, y, r, g, b);
    }
}

void ChristmasCountdownScene::drawCountdown(float t) {
    auto txt_color = text_color->get();
    
    // Pulsing effect for the text
    float pulse = 0.8f + 0.2f * std::sin(t * 2.0f);
    rgb_matrix::Color pulsed_color(
        static_cast<uint8_t>(txt_color.r * pulse),
        static_cast<uint8_t>(txt_color.g * pulse),
        static_cast<uint8_t>(txt_color.b * pulse)
    );
    
    if (show_hours->get()) {
        // Show hours countdown
        int hours = getHoursUntilChristmas();
        
        std::stringstream ss;
        ss << hours << "h";
        std::string hours_text = ss.str();
        
        // Draw hours on first line
        int x_pos = (matrix_width - hours_text.length() * 7) / 2;
        rgb_matrix::DrawText(offscreen_canvas, HEADER_FONT, x_pos, 40, 
                           pulsed_color, hours_text.c_str());
        
        // Draw label
        const char* label = "UNTIL";
        int label_x = (matrix_width - strlen(label) * 4) / 2;
        rgb_matrix::DrawText(offscreen_canvas, SMALL_FONT, label_x, 60, 
                           txt_color, label);
        
        const char* label2 = "CHRISTMAS";
        int label2_x = (matrix_width - strlen(label2) * 4) / 2;
        rgb_matrix::DrawText(offscreen_canvas, SMALL_FONT, label2_x, 75, 
                           txt_color, label2);
    } else {
        // Show days countdown
        int days = getDaysUntilChristmas();
        
        std::stringstream ss;
        ss << days;
        std::string days_text = ss.str();
        
        // Draw days number (large)
        int x_pos = (matrix_width - days_text.length() * 7) / 2;
        rgb_matrix::DrawText(offscreen_canvas, HEADER_FONT, x_pos, 40, 
                           pulsed_color, days_text.c_str());
        
        // Draw "DAYS" label
        const char* label = "DAYS";
        int label_x = (matrix_width - strlen(label) * 4) / 2;
        rgb_matrix::DrawText(offscreen_canvas, SMALL_FONT, label_x, 60, 
                           txt_color, label);
        
        // Draw "UNTIL CHRISTMAS" label
        const char* label2 = "UNTIL";
        int label2_x = (matrix_width - strlen(label2) * 4) / 2;
        rgb_matrix::DrawText(offscreen_canvas, SMALL_FONT, label2_x, 75, 
                           txt_color, label2);
        
        const char* label3 = "CHRISTMAS";
        int label3_x = (matrix_width - strlen(label3) * 4) / 2;
        rgb_matrix::DrawText(offscreen_canvas, SMALL_FONT, label3_x, 90, 
                           txt_color, label3);
    }
}

bool ChristmasCountdownScene::render(RGBMatrixBase *matrix) {
    auto frame = frameTimer.tick();
    float dt = frame.dt;
    float t = frame.t;
    
    // Clear with background color
    auto bg = bg_color->get();
    offscreen_canvas->Fill(bg.r, bg.g, bg.b);
    
    // Spawn sparkles
    if (show_sparkles->get()) {
        int target_sparkles = sparkle_density->get();
        float spawn_rate = target_sparkles * 0.3f;
        
        if (static_cast<int>(sparkles.size()) < target_sparkles && 
            std::uniform_real_distribution<float>(0.0f, 1.0f)(rng) < spawn_rate * dt) {
            spawnSparkle();
        }
        
        // Update and draw sparkles
        for (auto it = sparkles.begin(); it != sparkles.end();) {
            auto& sparkle = *it;
            
            // Update position
            sparkle.x += sparkle.vx * dt;
            sparkle.y += sparkle.vy * dt;
            
            // Update life
            sparkle.life -= dt / sparkle.max_life;
            
            // Remove dead sparkles
            if (sparkle.life <= 0.0f) {
                it = sparkles.erase(it);
                continue;
            }
            
            // Draw sparkle
            int ix = static_cast<int>(sparkle.x);
            int iy = static_cast<int>(sparkle.y);
            
            float brightness = sparkle.life;
            rgb_matrix::Color color(
                static_cast<uint8_t>(sparkle.color.r * brightness),
                static_cast<uint8_t>(sparkle.color.g * brightness),
                static_cast<uint8_t>(sparkle.color.b * brightness)
            );
            
            setPixelSafe(ix, iy, color.r, color.g, color.b);
            
            // Twinkle effect for bright sparkles
            if (brightness > 0.7f) {
                uint8_t glow = static_cast<uint8_t>(brightness * 100);
                setPixelSafe(ix - 1, iy, glow, glow, glow);
                setPixelSafe(ix + 1, iy, glow, glow, glow);
                setPixelSafe(ix, iy - 1, glow, glow, glow);
                setPixelSafe(ix, iy + 1, glow, glow, glow);
            }
            
            ++it;
        }
    }
    
    // Draw countdown text
    drawCountdown(t);
    
    return true;
}

void ChristmasCountdownScene::register_properties() {
    add_property(show_sparkles);
    add_property(sparkle_density);
    add_property(text_color);
    add_property(bg_color);
    add_property(show_hours);
}

std::unique_ptr<Scene, void (*)(Scene *)> ChristmasCountdownSceneWrapper::create() {
    return {new ChristmasCountdownScene(), [](Scene *scene) { delete scene; }};
}

}
