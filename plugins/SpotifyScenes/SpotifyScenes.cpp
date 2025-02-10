#include "SpotifyScenes.h"
#include "shared/utils/shared.h"
#include "manager/shared_spotify.h"
#include "scenes/CoverOnlyScene.h"
#include "spdlog/spdlog.h"

using namespace Scenes;

extern "C" [[maybe_unused]] SpotifyScenes *createSpotifyScenes() {
    return new SpotifyScenes();
}

extern "C" [[maybe_unused]] void destroySpotifyScenes(SpotifyScenes *c) {
    spotify->terminate();
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

std::optional<string> SpotifyScenes::post_init() {
    spdlog::info("Initializing SpotifyScenes");

    spotify = new Spotify();
    spotify->initialize();

    config->save();
    return BasicPlugin::post_init();
}

SpotifyScenes::SpotifyScenes() = default;
