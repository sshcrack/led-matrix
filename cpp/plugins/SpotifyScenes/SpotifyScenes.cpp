#include "SpotifyScenes.h"
#include "scenes/CoverOnlyScene.h"

using namespace Scenes;

extern "C" SpotifyScenes *createSpotifyScenes() {
    return new SpotifyScenes();
}

extern "C" void destroySpotifyScenes(SpotifyScenes *c) {
    delete c;
}

vector<ImageProviderWrapper *> SpotifyScenes::get_image_providers() {
    return {};
}

vector<SceneWrapper *> SpotifyScenes::get_scenes() {
    return {
            new CoverOnlySceneWrapper(),
    };
}

SpotifyScenes::SpotifyScenes() = default;
