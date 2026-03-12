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
        int r = rand() % 3;
        if (r == 0) {
            color_r = rand() % 155 + 100;
            color_g = rand() % 100 + 50;
            color_b = rand() % 100 + 50;
        } else if (r == 1) {
            color_r = rand() % 100 + 50;
            color_g = rand() % 155 + 100;
            color_b = rand() % 100 + 50;
        } else {
            color_r = rand() % 100 + 50;
            color_g = rand() % 100 + 50;
            color_b = rand() % 155 + 100;
        }
    }

    void BouncingLogoScene::draw_logo(int px, int py) {
        // Draw a simple rectangle representing the logo
        for (int x = 0; x < logo_width; x++) {
            for (int y = 0; y < logo_height; y++) {
                // If it's a border pixel
                if (x == 0 || x == logo_width - 1 || y == 0 || y == logo_height - 1) {
                    offscreen_canvas->SetPixel(px + x, py + y, color_r, color_g, color_b);
                } else {
                    // Internal fill or "DVD" text placeholder
                    // Draw a simple diamond inside
                    int cx = logo_width / 2;
                    int cy = logo_height / 2;
                    if (std::abs(x - cx) + std::abs(y - cy) < (logo_height / 2)) {
                        offscreen_canvas->SetPixel(px + x, py + y, color_r, color_g, color_b);
                    }
                }
            }
        }
    }

    void BouncingLogoScene::initialize(RGBMatrixBase *matrix, rgb_matrix::FrameCanvas *l_offscreen_canvas) {
        Scene::initialize(matrix, l_offscreen_canvas);
        matrix_width = matrix->width();
        matrix_height = matrix->height();

        logo_height = size->get();
        logo_width = logo_height * 2; // Aspect ratio ~ 2:1

        // Start somewhere safe
        pos_x = (float)(rand() % (matrix_width - logo_width));
        pos_y = (float)(rand() % (matrix_height - logo_height));

        vel_x = speed->get();
        vel_y = speed->get();

        change_color();
    }

    bool BouncingLogoScene::render(RGBMatrixBase *matrix) {
        offscreen_canvas->Clear();

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

        draw_logo(px, py);

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

    std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> BouncingLogoSceneWrapper::create() {
        return {new BouncingLogoScene(), [](Scenes::Scene *scene) {
            delete dynamic_cast<BouncingLogoScene *>(scene);
        }};
    }
}
