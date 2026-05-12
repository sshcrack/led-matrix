#include "RGBMatrixAnimations.h"
#include "scenes/RainScene.h"
#include "scenes/SparksScene.h"

using namespace Scenes;

extern "C" PLUGIN_EXPORT RGBMatrixAnimations *createRGBMatrixAnimations() {
    return new RGBMatrixAnimations();
}

extern "C" PLUGIN_EXPORT void destroyRGBMatrixAnimations(RGBMatrixAnimations *c) {
    delete c;
}

vector<std::unique_ptr<ImageProviderWrapper, void (*)(ImageProviderWrapper *)>>
RGBMatrixAnimations::create_image_providers() {
    return {};
}

vector<std::unique_ptr<SceneWrapper, void (*)(SceneWrapper *)>> RGBMatrixAnimations::create_scenes() {
    auto scenes = vector<std::unique_ptr<SceneWrapper, void (*)(SceneWrapper *)>>();
    auto deleteScene = [](SceneWrapper *scene) {
        delete scene;
    };

    scenes.push_back({new RainSceneWrapper(), deleteScene});
    scenes.push_back({new SparksSceneWrapper(), deleteScene});

    return scenes;
}

RGBMatrixAnimations::RGBMatrixAnimations() = default;

DECLARE_PLUGIN_API_VERSION(plugin_get_api_version, MATRIX_PLUGIN_API_VERSION)
DECLARE_PLUGIN_API_VERSION(get_api_version, MATRIX_PLUGIN_API_VERSION)
