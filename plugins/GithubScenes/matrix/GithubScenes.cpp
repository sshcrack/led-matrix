#include "GithubScenes.h"
#include "scenes/WatermelonPlasmaScene.h"
#include "scenes/WaveScene.h"

using namespace Scenes;

REGISTER_PLUGIN(GithubScenes, GithubScenes)

vector<std::unique_ptr<ImageProviderWrapper>> GithubScenes::create_image_providers() {
    return {};
}

vector<std::unique_ptr<SceneWrapper>> GithubScenes::create_scenes() {
    auto scenes = vector<std::unique_ptr<SceneWrapper>>();

    scenes.push_back(std::make_unique<WatermelonPlasmaSceneWrapper>());
    scenes.push_back(std::make_unique<WaveSceneWrapper>());

    return scenes;
}

GithubScenes::GithubScenes() = default;
