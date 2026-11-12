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
    auto created = vector<std::unique_ptr<SceneWrapper>>();

    created.push_back(std::make_unique<RainSceneWrapper>());
    created.push_back(std::make_unique<SparksSceneWrapper>());

    return created;
}

RGBMatrixAnimations::RGBMatrixAnimations() = default;
