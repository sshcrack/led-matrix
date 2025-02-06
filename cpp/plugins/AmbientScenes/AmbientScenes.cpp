#include "AmbientScenes.h"
#include "shared/utils/shared.h"
#include "spdlog/spdlog.h"
#include "scenes/StarFieldScene.h"
#include "scenes/MetaBlobScene.h"

using namespace Scenes;

extern "C" [[maybe_unused]] AmbientScenes *createAmbientScenes() {
    return new AmbientScenes();
}

extern "C" [[maybe_unused]] void destroyAmbientScenes(AmbientScenes *c) {
    delete c;
}

vector<ImageProviderWrapper *> AmbientScenes::get_image_providers() {
    return {};
}

vector<SceneWrapper *> AmbientScenes::get_scenes() {
    return {
            new StarFieldSceneWrapper(),
            new MetaBlobSceneWrapper()
    };
}

AmbientScenes::AmbientScenes() = default;
