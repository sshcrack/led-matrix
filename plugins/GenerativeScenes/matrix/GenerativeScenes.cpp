#include "GenerativeScenes.h"
#include "scenes/ReactionDiffusionScene.h"

using namespace GenerativeScenes;

extern "C" PLUGIN_EXPORT GenerativePlugin *createGenerativeScenes() {
    return new GenerativePlugin();
}

extern "C" PLUGIN_EXPORT void destroyGenerativeScenes(GenerativePlugin *c) {
    delete c;
}

vector<std::unique_ptr<SceneWrapper>> GenerativePlugin::create_scenes() {
    vector<std::unique_ptr<SceneWrapper>> scenes;

    scenes.push_back(std::make_unique<ReactionDiffusionSceneWrapper>());

    return scenes;
}

vector<std::unique_ptr<ImageProviderWrapper>> GenerativePlugin::create_image_providers() {
    return {};
}

GenerativePlugin::GenerativePlugin() = default;
