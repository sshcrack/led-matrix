#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wimplicit-const-int-float-conversion"
#pragma ide diagnostic ignored "cert-msc50-cpp"

#include "WaveScene.h"
#include <cmath>
#include "shared/matrix/utils/utils.h"


unsigned int xyToIndex(int h, int x, int y) { return y * h + x; }


void WaveScene::drawMap(rgb_matrix::FrameCanvas *canvas, const std::vector<float> &map) const {
    for (int y = 0; y < matrix_height; y++) {
        for (int x = 0; x < matrix_width; x++) {
            const int i = xyToIndex(matrix_width, x, y);
            floatPixelSet(canvas, x, y,
                          std::pow(map[i], 4 + (map[i] * 0.5)) * std::cos(map[i]),
                          std::pow(map[i], 3 + (map[i] * 0.5)) * std::sin(map[i]),
                          std::pow(map[i], 2 + (map[i] * 0.5)));
        }
    }
}

bool Scenes::WaveScene::render(rgb_matrix::FrameCanvas *canvas) {
    canvas->Clear();

    std::uniform_real_distribution<float> rdist(0.0f, 1.0f);
    for (int y = 0; y < matrix_height; y++) {
        for (int x = 0; x < matrix_width; x++) {
            const int i = xyToIndex(matrix_width, x, y);
            const float lastValue = map_[i];

            next_map_[i] = lastValue * (0.96f + 0.02f * rdist(rng));

            if (lastValue <= (0.18f + 0.04f * rdist(rng))) {
                float n = 0;

                for (int u = -1; u <= 1; u++) {
                    for (int v = -1; v <= 1; v++) {
                        if (u == 0)
                            continue;

                        const int nX = std::abs((x + u) % matrix_width);
                        const int nY = std::abs((y + v) % matrix_height);
                        const int nI = xyToIndex(matrix_width, nX, nY);
                        const float nLastValue = map_[nI];

                        if (nLastValue >= (0.5f + 0.04f * rdist(rng))) {
                            n += 1;
                            next_map_[i] += nLastValue * (0.8f + 0.4f * rdist(rng));
                        }
                    }
                }

                if (n > 0)
                    next_map_[i] *= 1.0f / n;
                next_map_[i] = std::min(next_map_[i], 1.0f);
            }
        }
    }

    drawMap(canvas, next_map_);
    map_.swap(next_map_);
    wait_until_next_frame();
    return true;
}

void WaveScene::initialize(int width, int height) {
    Scene::initialize(width, height);

    const auto pixels = static_cast<size_t>(matrix_width * matrix_height);
    map_.resize(pixels);
    next_map_.resize(pixels);

    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    for (auto &value : map_)
        value = dist(rng);
}

string WaveScene::get_name() const {
    return "wave";
}

Scenes::SceneDescriptor WaveScene::get_descriptor() const {
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "wave-simulation";
    d.tags = {"ambient", "generative", "organic", "texture", "flow"};
    d.intensity = 0.38f; d.motion = 0.34f; d.music_affinity = 0.12f; d.performance_cost = 0.46f;
    return d;
}


std::unique_ptr<Scene> WaveSceneWrapper::create() {
    return std::make_unique<WaveScene>();
}

#pragma clang diagnostic pop
