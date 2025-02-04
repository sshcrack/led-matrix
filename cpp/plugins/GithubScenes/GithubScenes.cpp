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
        // These scenes are taken from https://github.com/Knifa/matryx-gl/tree/3f815663b5c883ef52da5b895206d68d756d59c8
            new WatermelonPlasmaSceneWrapper(),
            new WaveSceneWrapper()
    };
}

GithubScenes::GithubScenes() = default;
