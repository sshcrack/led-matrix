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

vector<ImageProviderWrapper *> GithubScenes::get_image_providers() {
    return {};
}

vector<SceneWrapper *> GithubScenes::get_scenes() {
    return {
            new WatermelonPlasmaSceneWrapper(),
            new WaveSceneWrapper()
    };
}

GithubScenes::GithubScenes() = default;
