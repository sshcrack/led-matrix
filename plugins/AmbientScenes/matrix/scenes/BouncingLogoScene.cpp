#include "BouncingLogoScene.h"
#include <cmath>

namespace AmbientScenes {
    BouncingLogoScene::BouncingLogoScene() : Scene() {
        pos_x = 0;
        pos_y = 0;
        vel_x = 0.5f;
        vel_y = 0.5f;
        logo_width = 30;
        logo_height = 16;
        color_r = 255;
        color_g = 255;
        color_b = 255;
    }

    void BouncingLogoScene::change_color() {
        std::uniform_int_distribution<int> dist3(0, 2);
        int r = dist3(rng);
        if (r == 0) {
            color_r = std::uniform_int_distribution<int>(100, 254)(rng);
            color_g = std::uniform_int_distribution<int>(50, 149)(rng);
            color_b = std::uniform_int_distribution<int>(50, 149)(rng);
        } else if (r == 1) {
            color_r = std::uniform_int_distribution<int>(50, 149)(rng);
            color_g = std::uniform_int_distribution<int>(100, 254)(rng);
            color_b = std::uniform_int_distribution<int>(50, 149)(rng);
        } else {
            color_r = std::uniform_int_distribution<int>(50, 149)(rng);
            color_g = std::uniform_int_distribution<int>(50, 149)(rng);
            color_b = std::uniform_int_distribution<int>(100, 254)(rng);
        }
    }

    void BouncingLogoScene::draw_logo(rgb_matrix::FrameCanvas *canvas, int px, int py) {
        // Draw a simple rectangle representing the logo
        for (int x = 0; x < logo_width; x++) {
            for (int y = 0; y < logo_height; y++) {
                // If it's a border pixel
                if (x == 0 || x == logo_width - 1 || y == 0 || y == logo_height - 1) {
                    canvas->SetPixel(px + x, py + y, color_r, color_g, color_b);
                } else {
                    // Internal fill or "DVD" text placeholder
                    // Draw a simple diamond inside
                    int cx = logo_width / 2;
                    int cy = logo_height / 2;
                    if (std::abs(x - cx) + std::abs(y - cy) < (logo_height / 2)) {
                        canvas->SetPixel(px + x, py + y, color_r, color_g, color_b);
                    }
                }
            }
        }
    }

    void BouncingLogoScene::initialize(int width, int height) {
        Scene::initialize(width, height);

        logo_height = size->get();
        logo_width = logo_height * 2; // Aspect ratio ~ 2:1

        // Start somewhere safe
        pos_x = std::uniform_int_distribution<int>(0, std::max(0, matrix_width - logo_width))(rng);
        pos_y = std::uniform_int_distribution<int>(0, std::max(0, matrix_height - logo_height))(rng);

        vel_x = speed->get();
        vel_y = speed->get();

        change_color();
    }

    bool BouncingLogoScene::render(rgb_matrix::FrameCanvas *canvas) {
        canvas->Clear();

        pos_x += vel_x;
        pos_y += vel_y;

        bool bounced = false;

        if (pos_x <= 0) {
            pos_x = 0;
            vel_x = std::abs(vel_x);
            bounced = true;
        } else if (pos_x + logo_width >= matrix_width) {
            pos_x = (float)(matrix_width - logo_width);
            vel_x = -std::abs(vel_x);
            bounced = true;
        }

        if (pos_y <= 0) {
            pos_y = 0;
            vel_y = std::abs(vel_y);
            bounced = true;
        } else if (pos_y + logo_height >= matrix_height) {
            pos_y = (float)(matrix_height - logo_height);
            vel_y = -std::abs(vel_y);
            bounced = true;
        }

        if (bounced) {
            change_color();
        }

        int px = (int)std::round(pos_x);
        int py = (int)std::round(pos_y);

        draw_logo(canvas, px, py);

        wait_until_next_frame();
        return true;
    }

    std::string BouncingLogoScene::get_name() const {
        return "bouncing-logo";
    }

    void BouncingLogoScene::register_properties() {
        add_property(speed);
        add_property(size);
    }

    std::unique_ptr<Scenes::Scene> BouncingLogoSceneWrapper::create() {
        return std::make_unique<BouncingLogoScene>();
    }
}
