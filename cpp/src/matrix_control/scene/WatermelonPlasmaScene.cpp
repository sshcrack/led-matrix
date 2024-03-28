//
// Created by hendrik on 3/28/24.
//

#include "WatermelonPlasmaScene.h"
#include "../../utils/utils.h"
#include <cmath>
#include <iostream>

using namespace std;

bool Scenes::WatermelonPlasmaScene::tick(rgb_matrix::RGBMatrix *matrix) {
    auto frameTime = frameTimer.tick();
    float t = frameTime.t;

    for (int y = 0; y < matrix->height(); y++) {
        for (int x = 0; x < matrix->width(); x++) {
            float xp =
                    ((x / 128.0f) - 0.5f) * (5.0f + sin(t * 0.25)) + sin(t * 0.25) * 5.0f;
            float yp =
                    ((y / 128.0f) - 0.5f) * (5.0f + sin(t * 0.25)) + cos(t * 0.25) * 5.0f;

            float pixel = sin(sin(sin(0.25 * t) * xp + cos(0.29 * t) * yp + t) +
                              sin(sqrt(pow(xp + sin(t * 0.25f) * 4.0f, 2) +
                                       pow(yp + cos(t * 0.43f) * 4.0f, 2)) +
                                  t) -
                              cos(sqrt(pow(xp + cos(t * 0.36f) * 6.0f, 2) +
                                       pow(yp + sin(t * 0.39f) * 5.3f, 2)) +
                                  t));

            float u = pow(cos(9 * pixel + 0.5f * xp + t) * 0.5f + 0.5f, 2);
            float v = pow(sin(9 * pixel + 0.5f * yp + t) * 0.5f + 0.5f, 2);

            floatPixelSet(offscreen_canvas, x, y, u, v, (u+v) / 2);
        }
    }

    offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas, 1);
    return false;
}
