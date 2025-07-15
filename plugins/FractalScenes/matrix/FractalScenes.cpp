#include "FractalScenes.h"
#include "shared/matrix/utils/shared.h"
#include <spdlog/spdlog.h>
#include "scenes/JuliaSetScene.h"
#include "scenes/WavePatternScene.h"
#include "scenes/GameOfLifeScene.h"

using namespace Scenes;

extern "C" [[maybe_unused]] FractalScenes *createFractalScenes() {
    return new FractalScenes();
}

extern "C" [[maybe_unused]] void destroyFractalScenes(FractalScenes *c) {
    delete c;
}

vector<std::unique_ptr<ImageProviderWrapper, void (*)(ImageProviderWrapper *)>> FractalScenes::create_image_providers() {
    return {};
}

vector<std::unique_ptr<SceneWrapper, void (*)(SceneWrapper *)>> FractalScenes::create_scenes() {
    auto scenes = vector<std::unique_ptr<SceneWrapper, void (*)(SceneWrapper *)>>();
    auto destroyScene = [](SceneWrapper *scene) {
        delete scene;
    };

    scenes.push_back({new JuliaSetSceneWrapper(), destroyScene});
    scenes.push_back({new WavePatternSceneWrapper(), destroyScene});
    scenes.push_back({new GameOfLifeSceneWrapper(), destroyScene});

    return scenes;
}

FractalScenes::FractalScenes() = default;
