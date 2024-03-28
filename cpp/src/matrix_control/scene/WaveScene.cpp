//
// Created by hendrik on 3/28/24.
//

#include "WaveScene.h"
#include <cmath>
#include "../../utils/utils.h"


static float *map = nullptr;

unsigned int xyToIndex(int h, int x, int y) { return y * h + x; }


void Scenes::WaveScene::drawMap(rgb_matrix::RGBMatrix* matrix, float *map) {
    for (int y = 0; y < matrix->height(); y++) {
        for (int x = 0; x < matrix->width(); x++) {
            const int i = xyToIndex(matrix->width(), x, y);
            floatPixelSet(offscreen_canvas, x, y,
                                  std::pow(map[i], 4 + (map[i] * 0.5)) * std::cos(map[i]),
                                  std::pow(map[i], 3 + (map[i] * 0.5)) * std::sin(map[i]),
                                  std::pow(map[i], 2 + (map[i] * 0.5)));
        }
    }
}

bool Scenes::WaveScene::tick(rgb_matrix::RGBMatrix *matrix) {
    float *lastMap = map;
    map = new float[matrix->width() * matrix->height()];

    for (int y = 0; y < matrix->height(); y++) {
        for (int x = 0; x < matrix->width(); x++) {
            const int i = xyToIndex(matrix->width(), x, y);
            const float lastValue = lastMap[i];

            map[i] = lastValue * (0.96 + 0.02 * (static_cast<float>(std::rand()) / RAND_MAX));

            if (lastValue <= (0.18 + 0.04 * (static_cast<float>(std::rand()) / RAND_MAX))) {
                float n = 0;

                for (int u = -1; u <= 1; u++) {
                    for (int v = -1; v <= 1; v++) {
                        if (u == 0 && u == 0) {
                            continue;
                        }

                        int nX = std::abs((x + u) % matrix->width());
                        int nY = std::abs((y + v) % matrix->height());

                        const int nI = xyToIndex(matrix->width(), nX, nY);
                        const float nLastValue = lastMap[nI];

                        if (nLastValue >= (0.5 + 0.04 * (static_cast<float>(std::rand()) / RAND_MAX))) {
                            n += 1;
                            map[i] += nLastValue * (0.8 + 0.4 * (static_cast<float>(std::rand()) / RAND_MAX));
                        }
                    }
                }

                if (n > 0) {
                    map[i] *= 1 / n;
                }

                if (map[i] > 1)
                    map[i] = 1;
            }
        }
    }

    drawMap(matrix, map);

    matrix->SwapOnVSync(offscreen_canvas);
    return false;
}

Scenes::WaveScene::WaveScene(rgb_matrix::RGBMatrix *matrix) : Scene(matrix) {
    std::srand(std::time(nullptr));

    map = new float[matrix->width() * matrix->height()];

    for (int y = 0; y < matrix->height(); y++) {
        for (int x = 0; x < matrix->width(); x++) {
            const int i = xyToIndex(matrix->width(), x, y);
            map[i] = static_cast<float>(std::rand()) / RAND_MAX;
        }
    }
}
