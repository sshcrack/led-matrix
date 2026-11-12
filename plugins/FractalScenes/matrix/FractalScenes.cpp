#include "FractalScenes.h"
#include "shared/matrix/utils/shared.h"
#include <spdlog/spdlog.h>
#include "scenes/JuliaSetScene.h"
#include "scenes/WavePatternScene.h"
#include "scenes/GameOfLifeScene.h"

using namespace Scenes;

REGISTER_PLUGIN(FractalScenes, FractalScenes)

vector<std::unique_ptr<ImageProviderWrapper>> FractalScenes::create_image_providers() {
    return {};
}

vector<std::unique_ptr<SceneWrapper>> FractalScenes::create_scenes() {
    vector<std::unique_ptr<SceneWrapper>> created;
    created.push_back(std::make_unique<JuliaSetSceneWrapper>());
    created.push_back(std::make_unique<WavePatternSceneWrapper>());
    created.push_back(std::make_unique<GameOfLifeSceneWrapper>());
    return created;
}

FractalScenes::FractalScenes() = default;
