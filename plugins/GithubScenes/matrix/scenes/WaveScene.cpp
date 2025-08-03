#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wimplicit-const-int-float-conversion"
#pragma ide diagnostic ignored "cert-msc50-cpp"

#include "WaveScene.h"
#include <cmath>
#include "shared/matrix/utils/utils.h"


unsigned int xyToIndex(int h, int x, int y) { return y * h + x; }


void WaveScene::drawMap(RGBMatrixBase *matrix, float *iMap) {
    for (int y = 0; y < matrix->height(); y++) {
        for (int x = 0; x < matrix->width(); x++) {
            const int i = xyToIndex(matrix->width(), x, y);
            floatPixelSet(offscreen_canvas, x, y,
                          std::pow(iMap[i], 4 + (iMap[i] * 0.5)) * std::cos(iMap[i]),
                          std::pow(iMap[i], 3 + (iMap[i] * 0.5)) * std::sin(iMap[i]),
                          std::pow(iMap[i], 2 + (iMap[i] * 0.5)));
        }
    }
}

bool Scenes::WaveScene::render(RGBMatrixBase *matrix) {
    offscreen_canvas->Clear();

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
                        if (u == 0 /*&& u == 0*/) {
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

    delete[] lastMap;
    wait_until_next_frame();
    return true;
}

void WaveScene::initialize(RGBMatrixBase *matrix, FrameCanvas *l_offscreen_canvas) {
    Scene::initialize(matrix, l_offscreen_canvas);

    std::srand(std::time(nullptr));

    map = new float[matrix->width() * matrix->height()];

    for (int y = 0; y < matrix->height(); y++) {
        for (int x = 0; x < matrix->width(); x++) {
            const int i = xyToIndex(matrix->width(), x, y);
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
