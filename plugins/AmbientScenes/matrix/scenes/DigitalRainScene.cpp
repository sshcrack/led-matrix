#include "DigitalRainScene.h"
#include <cmath>

namespace AmbientScenes {
    DigitalRainScene::DigitalRainScene() :
            Scene(),
            gen(rd()) {
    }

    void DigitalRainScene::reset_drop(Drop& drop) {
        if (!matrix_width || !matrix_height) return;
        drop.x = dis_x(gen);
        drop.y = -(float)(dis_length(gen)); // Start slightly above the screen
        drop.speed = base_speed->get() * (0.5f + (float)dis_speed(gen));
        drop.length = dis_length(gen);
    }

    void DigitalRainScene::initialize(int width, int height) {
        Scene::initialize(width, height);
        
        matrix_brightness.resize(width, std::vector<float>(height, 0.0f));
        drops.resize(num_drops->get());
        
        dis_speed = std::uniform_real_distribution<>(0.0, 1.0);
        dis_length = std::uniform_int_distribution<>(5, height / 2);
        dis_x = std::uniform_int_distribution<>(0, width - 1);

        for (auto &drop : drops) {
            reset_drop(drop);
            // Randomize initial positions so they don't all start at the top at once
            drop.y = (float)(std::uniform_real_distribution<>(-height, height)(gen));
        }
    }

    bool DigitalRainScene::render(rgb_matrix::FrameCanvas *canvas) {
        canvas->Clear();

        // Fade existing matrix brightness
        for (int x = 0; x < matrix_width; ++x) {
            for (int y = 0; y < matrix_height; ++y) {
                matrix_brightness[x][y] *= fade_factor->get();
            }
        }

        // Update and draw drops
        for (auto &drop : drops) {
            int old_y = (int)drop.y;
            drop.y += drop.speed;
            int new_y = (int)drop.y;

            // Mark the new head brightness to maximum (1.0)
            if (new_y >= 0 && new_y < matrix_height && drop.x >= 0 && drop.x < matrix_width) {
                // To make the trails fuller, we can set everything from old_y to new_y to 1.0
                for (int y = std::max(0, old_y); y <= new_y && y < matrix_height; ++y) {
                    matrix_brightness[drop.x][y] = 1.0f;
                }
            }

            if (drop.y - drop.length > matrix_height) {
                reset_drop(drop);
            }
        }

        // Render to canvas
        rgb_matrix::Color c = color->get();
        for (int x = 0; x < matrix_width; ++x) {
            for (int y = 0; y < matrix_height; ++y) {
                float brightness = matrix_brightness[x][y];
                if (brightness > 0.01f) {
                    canvas->SetPixel(x, y, 
                        (uint8_t)(c.r * brightness), 
                        (uint8_t)(c.g * brightness), 
                        (uint8_t)(c.b * brightness));
                }
            }
        }

        // Draw white heads for drops (optional classic matrix look)
        for (auto &drop : drops) {
            int head_y = (int)drop.y;
            if (head_y >= 0 && head_y < matrix_height && drop.x >= 0 && drop.x < matrix_width) {
                canvas->SetPixel(drop.x, head_y, 255, 255, 255);
            }
        }

        wait_until_next_frame();
        return true;
    }

    std::string DigitalRainScene::get_name() const {
        return "digitalrain";
    }

    void DigitalRainScene::register_properties() {
        add_property(num_drops);
        add_property(base_speed);
        add_property(fade_factor);
        add_property(color);
    }

    std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> DigitalRainSceneWrapper::create() {
        return {new DigitalRainScene(), [](Scenes::Scene *scene) {
            delete dynamic_cast<DigitalRainScene *>(scene);
        }};
    }
}
