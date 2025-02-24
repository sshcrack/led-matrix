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
    delete spotify;  // The destructor will handle termination
    delete c;
}

vector<std::unique_ptr<ImageProviderWrapper, void (*)(ImageProviderWrapper *)>>
SpotifyScenes::create_image_providers() {
    return {};
}

vector<std::unique_ptr<SceneWrapper, void (*)(Plugins::SceneWrapper *)>> SpotifyScenes::create_scenes() {
    auto scenes = vector<std::unique_ptr<SceneWrapper, void (*)(Plugins::SceneWrapper *)>>();
    scenes.push_back({new CoverOnlySceneWrapper(), [](SceneWrapper *scene) {
        delete scene;
    }});

    return scenes;
}

std::optional<string> SpotifyScenes::post_init() {
    spdlog::info("Initializing SpotifyScenes");

    spotify = new Spotify();
    spotify->initialize();

    config->save();
    return BasicPlugin::post_init();
}

std::unique_ptr<router_t> SpotifyScenes::
register_routes(std::unique_ptr<router_t> router) {
    return BasicPlugin::register_routes(router);
}

SpotifyScenes::SpotifyScenes() = default;
