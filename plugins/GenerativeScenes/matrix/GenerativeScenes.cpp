#include "GenerativeScenes.h"
#include "scenes/ReactionDiffusionScene.h"

using namespace GenerativeScenes;

extern "C" PLUGIN_EXPORT GenerativePlugin *createGenerativeScenes() {
    return new GenerativePlugin();
}

extern "C" PLUGIN_EXPORT void destroyGenerativeScenes(GenerativePlugin *c) {
    delete c;
}

vector<std::unique_ptr<SceneWrapper, void (*)(SceneWrapper *)>> GenerativePlugin::create_scenes() {
    vector<std::unique_ptr<SceneWrapper, void (*)(SceneWrapper *)>> scenes;

    scenes.push_back(std::unique_ptr<SceneWrapper, void (*)(SceneWrapper *)>(
            new ReactionDiffusionSceneWrapper(),
            [](SceneWrapper *s) { delete (ReactionDiffusionSceneWrapper *) s; }));

    return scenes;
}

vector<std::unique_ptr<ImageProviderWrapper, void (*)(ImageProviderWrapper *)>> GenerativePlugin::create_image_providers() {
    return {};
}

GenerativePlugin::GenerativePlugin() = default;
