#include "AmbientScenes.h"
#include "shared/utils/shared.h"
#include "spdlog/spdlog.h"
#include "scenes/StarFieldScene.h"
#include "scenes/MetaBlobScene.h"
#include "scenes/FireScene.h"

using namespace Scenes;
using namespace AmbientScenes;

extern "C" [[maybe_unused]] AmbientPlugin *createAmbientScenes() {
    return new AmbientPlugin();
}

extern "C" [[maybe_unused]] void destroyAmbientScenes(AmbientPlugin *c) {
    delete c;
}

vector<std::unique_ptr<SceneWrapper, void (*)(SceneWrapper *)>> AmbientPlugin::create_scenes() {
    auto destructor = [](SceneWrapper *scene) {
        delete scene;
    };

    vector<std::unique_ptr<SceneWrapper, void (*)(SceneWrapper *)>> scenes;
    scenes.push_back(std::unique_ptr<SceneWrapper, void (*)(SceneWrapper *)>(new StarFieldSceneWrapper(), destructor));
    scenes.push_back(std::unique_ptr<SceneWrapper, void (*)(SceneWrapper *)>(new MetaBlobSceneWrapper(), destructor));
    scenes.push_back(std::unique_ptr<SceneWrapper, void (*)(SceneWrapper *)>(new FireSceneWrapper(), destructor));
    
    return scenes;
}

vector<std::unique_ptr<ImageProviderWrapper, void (*)(ImageProviderWrapper *)>>
AmbientPlugin::create_image_providers() {
    return {};
}

AmbientPlugin::AmbientPlugin() = default;
