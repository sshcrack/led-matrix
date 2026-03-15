#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wimplicit-const-int-float-conversion"
#pragma ide diagnostic ignored "cert-msc50-cpp"

#include "WaveScene.h"
#include <cmath>
#include "shared/matrix/utils/utils.h"


unsigned int xyToIndex(int h, int x, int y) { return y * h + x; }


void WaveScene::drawMap(rgb_matrix::FrameCanvas *canvas, float *iMap) {
    for (int y = 0; y < matrix_height; y++) {
        for (int x = 0; x < matrix_width; x++) {
            const int i = xyToIndex(matrix_width, x, y);
            floatPixelSet(canvas, x, y,
                          std::pow(iMap[i], 4 + (iMap[i] * 0.5)) * std::cos(iMap[i]),
                          std::pow(iMap[i], 3 + (iMap[i] * 0.5)) * std::sin(iMap[i]),
                          std::pow(iMap[i], 2 + (iMap[i] * 0.5)));
        }
    }
}

bool Scenes::WaveScene::render(rgb_matrix::FrameCanvas *canvas) {
    canvas->Clear();

    float *lastMap = map;
    map = new float[matrix_width * matrix_height];

    for (int y = 0; y < matrix_height; y++) {
        for (int x = 0; x < matrix_width; x++) {
            const int i = xyToIndex(matrix_width, x, y);
            const float lastValue = lastMap[i];

            map[i] = lastValue * (0.96 + 0.02 * (static_cast<float>(std::rand()) / RAND_MAX));

            if (lastValue <= (0.18 + 0.04 * (static_cast<float>(std::rand()) / RAND_MAX))) {
                float n = 0;

                for (int u = -1; u <= 1; u++) {
                    for (int v = -1; v <= 1; v++) {
                        if (u == 0 /*&& u == 0*/) {
                            continue;
                        }

                        int nX = std::abs((x + u) % matrix_width);
                        int nY = std::abs((y + v) % matrix_height);

                        const int nI = xyToIndex(matrix_width, nX, nY);
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

    drawMap(canvas, map);

    delete[] lastMap;
    wait_until_next_frame();
    return true;
}

void WaveScene::initialize(int width, int height) {
    Scene::initialize(width, height);

    std::srand(std::time(nullptr));

    map = new float[matrix_width * matrix_height];

    for (int y = 0; y < matrix_height; y++) {
        for (int x = 0; x < matrix_width; x++) {
            const int i = xyToIndex(matrix_width, x, y);
            map[i] = static_cast<float>(std::rand()) / RAND_MAX;
        }
    }
}

string WaveScene::get_name() const {
    return "wave";
}

WaveScene::~WaveScene() {
    delete[] map;
}

std::unique_ptr<Scene, void (*)(Scene *)> WaveSceneWrapper::create() {
    return {
        new WaveScene(), [](Scene *scene) {
            delete scene;
        }
    };
}

#pragma clang diagnostic pop
