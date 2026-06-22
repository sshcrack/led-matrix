#include "FractalScenes.h"
#include "shared/matrix/utils/shared.h"
#include <spdlog/spdlog.h>
#include "scenes/JuliaSetScene.h"
#include "scenes/WavePatternScene.h"
#include "scenes/GameOfLifeScene.h"

using namespace Scenes;

extern "C" PLUGIN_EXPORT FractalScenes *createFractalScenes() {
    return new FractalScenes();
}

extern "C" PLUGIN_EXPORT void destroyFractalScenes(FractalScenes *c) {
    delete c;
}

vector<std::unique_ptr<ImageProviderWrapper>> FractalScenes::create_image_providers() {
    return {};
}

vector<std::unique_ptr<SceneWrapper>> FractalScenes::create_scenes() {
    vector<std::unique_ptr<SceneWrapper>> scenes;
    scenes.push_back(std::make_unique<JuliaSetSceneWrapper>());
    scenes.push_back(std::make_unique<WavePatternSceneWrapper>());
    scenes.push_back(std::make_unique<GameOfLifeSceneWrapper>());
    return scenes;
}

FractalScenes::FractalScenes() = default;
