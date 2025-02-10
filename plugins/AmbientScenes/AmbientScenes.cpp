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

vector<std::unique_ptr<ImageProviderWrapper>> AmbientPlugin::get_image_providers() {
    return {};
}

extern "C" void deleteSceneWrapper(Plugins::SceneWrapper* wrapper) {
    std::cout << "Deleting wrapper" << std::endl << std::flush;
    delete wrapper;
}

vector<std::unique_ptr<SceneWrapper, void (*)(Plugins::SceneWrapper *)>> AmbientPlugin::get_scenes() {
    auto scenes = vector<std::unique_ptr<SceneWrapper, void(*)(Plugins::SceneWrapper*)>>();
    scenes.push_back(std::unique_ptr<SceneWrapper, void(*)(Plugins::SceneWrapper*)>(new StarFieldSceneWrapper(), deleteSceneWrapper));
    scenes.push_back(std::unique_ptr<SceneWrapper, void(*)(Plugins::SceneWrapper*)>(new MetaBlobSceneWrapper(), deleteSceneWrapper));
    scenes.push_back(std::unique_ptr<SceneWrapper, void(*)(Plugins::SceneWrapper*)>(new FireSceneWrapper(), deleteSceneWrapper));

    return scenes;
}

AmbientPlugin::AmbientPlugin() = default;
