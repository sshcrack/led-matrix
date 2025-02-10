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

vector<ImageProviderWrapper *> RGBMatrixAnimations::get_image_providers() {
    return {};
}

vector<SceneWrapper *> RGBMatrixAnimations::get_scenes() {
    return {
            new RainSceneWrapper(),
            new SparksSceneWrapper()
    };
}

RGBMatrixAnimations::RGBMatrixAnimations() = default;
