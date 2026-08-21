#include "GenerativeScenes.h"
#include "scenes/ReactionDiffusionScene.h"
#include "scenes/BoidsScene.h"
#include "scenes/FallingSandScene.h"

using namespace GenerativeScenes;

REGISTER_PLUGIN(GenerativeScenes, GenerativePlugin)

vector<std::unique_ptr<SceneWrapper>> GenerativePlugin::create_scenes() {
    vector<std::unique_ptr<SceneWrapper>> scenes;

    scenes.push_back(std::make_unique<ReactionDiffusionSceneWrapper>());
    scenes.push_back(std::make_unique<BoidsSceneWrapper>());
    scenes.push_back(std::make_unique<FallingSandSceneWrapper>());

    return scenes;
}

vector<std::unique_ptr<ImageProviderWrapper>> GenerativePlugin::create_image_providers() {
    return {};
}

GenerativePlugin::GenerativePlugin() = default;
