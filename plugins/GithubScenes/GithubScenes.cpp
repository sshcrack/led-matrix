#include "GithubScenes.h"
#include "scenes/WatermelonPlasmaScene.h"
#include "scenes/WaveScene.h"

using namespace Scenes;

extern "C" [[maybe_unused]] GithubScenes *createGithubScenes() {
    return new GithubScenes();
}

extern "C" [[maybe_unused]] void destroyGithubScenes(GithubScenes *c) {
    delete c;
}

vector<std::unique_ptr<ImageProviderWrapper>> GithubScenes::get_image_providers() {
    return {};
}

vector<std::unique_ptr<SceneWrapper, void (*)(Plugins::SceneWrapper *)>> GithubScenes::get_scenes() {
    auto scenes = vector<std::unique_ptr<SceneWrapper>>();
    scenes.push_back(std::make_unique<WatermelonPlasmaSceneWrapper>());
    scenes.push_back(std::make_unique<WaveSceneWrapper>());

    return scenes;
}

GithubScenes::GithubScenes() = default;
