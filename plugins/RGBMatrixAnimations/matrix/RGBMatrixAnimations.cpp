#include "RGBMatrixAnimations.h"
#include "scenes/RainScene.h"
#include "scenes/SparksScene.h"

using namespace Scenes;

REGISTER_PLUGIN(RGBMatrixAnimations, RGBMatrixAnimations)

vector<std::unique_ptr<ImageProviderWrapper>>
RGBMatrixAnimations::create_image_providers() {
    return {};
}

vector<std::unique_ptr<SceneWrapper>> RGBMatrixAnimations::create_scenes() {
    auto scenes = vector<std::unique_ptr<SceneWrapper>>();

    scenes.push_back(std::make_unique<RainSceneWrapper>());
    scenes.push_back(std::make_unique<SparksSceneWrapper>());

    return scenes;
}

RGBMatrixAnimations::RGBMatrixAnimations() = default;
