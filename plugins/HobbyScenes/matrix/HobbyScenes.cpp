#include "HobbyScenes.h"

#include "scenes/HikingScenes.h"
#include "scenes/HorseScenes.h"
#include "scenes/SportScenes.h"

using namespace Hobbies;

REGISTER_PLUGIN(HobbyScenes, HobbyPlugin)

vector<std::unique_ptr<SceneWrapper>> HobbyPlugin::create_scenes()
{
    vector<std::unique_ptr<SceneWrapper>> wrappers;
    wrappers.push_back(std::make_unique<HobbySceneWrapper<SunsetGallopScene>>());
    wrappers.push_back(std::make_unique<HobbySceneWrapper<MistyHerdScene>>());
    wrappers.push_back(std::make_unique<HobbySceneWrapper<PegasusSkyScene>>());
    wrappers.push_back(std::make_unique<HobbySceneWrapper<RidgeTrailScene>>());
    wrappers.push_back(std::make_unique<HobbySceneWrapper<SummitNightScene>>());
    wrappers.push_back(std::make_unique<HobbySceneWrapper<CloudShadowsScene>>());
    wrappers.push_back(std::make_unique<HobbySceneWrapper<BasketballScene>>());
    wrappers.push_back(std::make_unique<HobbySceneWrapper<SoccerScene>>());
    return wrappers;
}

vector<std::unique_ptr<ImageProviderWrapper>> HobbyPlugin::create_image_providers() { return {}; }

HobbyPlugin::HobbyPlugin() = default;
