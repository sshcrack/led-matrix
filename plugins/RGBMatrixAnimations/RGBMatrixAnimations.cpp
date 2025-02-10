#include "RGBMatrixAnimations.h"
#include "scenes/RainScene.h"
#include "scenes/SparksScene.h"

using namespace Scenes;

extern "C" [[maybe_unused]] RGBMatrixAnimations *createRGBMatrixAnimations() {
    return new RGBMatrixAnimations();
}

extern "C" [[maybe_unused]] void destroyRGBMatrixAnimations(RGBMatrixAnimations *c) {
    delete c;
}

vector<std::unique_ptr<ImageProviderWrapper>> RGBMatrixAnimations::get_image_providers() {
    return {};
}

vector<std::unique_ptr<SceneWrapper, void (*)(Plugins::SceneWrapper *)>> RGBMatrixAnimations::get_scenes() {
    auto scenes = vector<std::unique_ptr<SceneWrapper>>();
    scenes.push_back(std::make_unique<RainSceneWrapper>());
    scenes.push_back(std::make_unique<SparksSceneWrapper>());

    return scenes;
}

RGBMatrixAnimations::RGBMatrixAnimations() = default;
