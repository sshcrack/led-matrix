#include "GithubScenes.h"
#include "scenes/WatermelonPlasmaScene.h"
#include "scenes/WaveScene.h"

using namespace Scenes;

REGISTER_PLUGIN(GithubScenes, GithubScenes)

vector<std::unique_ptr<ImageProviderWrapper>> GithubScenes::create_image_providers() {
    return {};
}

vector<std::unique_ptr<SceneWrapper>> GithubScenes::create_scenes() {
    auto created = vector<std::unique_ptr<SceneWrapper>>();

    created.push_back(std::make_unique<WatermelonPlasmaSceneWrapper>());
    created.push_back(std::make_unique<WaveSceneWrapper>());

    return created;
}

GithubScenes::GithubScenes() = default;
