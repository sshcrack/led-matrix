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

vector<std::unique_ptr<ImageProviderWrapper>> SpotifyScenes::get_image_providers() {
    return {};
}

vector<std::unique_ptr<SceneWrapper, void (*)(Plugins::SceneWrapper *)>> SpotifyScenes::get_scenes() {
    auto scenes = vector<std::unique_ptr<SceneWrapper>>();
    scenes.push_back(std::make_unique<CoverOnlySceneWrapper>());

    return scenes;
}

std::optional<string> SpotifyScenes::post_init() {
    spdlog::info("Initializing SpotifyScenes");

    spotify = new Spotify();
    spotify->initialize();

    config->save();
    return BasicPlugin::post_init();
}

SpotifyScenes::SpotifyScenes() = default;
