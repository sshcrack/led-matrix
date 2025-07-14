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

vector<std::unique_ptr<ImageProviderWrapper, void(*)(ImageProviderWrapper*)>> GithubScenes::create_image_providers() {
    return {};
}

vector<std::unique_ptr<SceneWrapper, void (*)(Plugins::SceneWrapper *)>> GithubScenes::create_scenes() {
    auto scenes = vector<std::unique_ptr<SceneWrapper, void(*)(Plugins::SceneWrapper*)>>();
    auto deleteScene = [](SceneWrapper* scene) {
        delete scene;
    };

    scenes.push_back({new WatermelonPlasmaSceneWrapper(), deleteScene});
    scenes.push_back({new WaveSceneWrapper(), deleteScene});

    return scenes;
}

GithubScenes::GithubScenes() = default;
