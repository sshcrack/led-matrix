#include "HorseScenes.h"
#include "scenes/HorseScenes.h"

using namespace HorseScenes;

REGISTER_PLUGIN(HorseScenes, HorsePlugin)

vector<std::unique_ptr<SceneWrapper>> HorsePlugin::create_scenes() {
    vector<std::unique_ptr<SceneWrapper>> wrappers;
    wrappers.push_back(std::make_unique<SunsetGallopSceneWrapper>());
    wrappers.push_back(std::make_unique<MistyHerdSceneWrapper>());
    wrappers.push_back(std::make_unique<PegasusSkySceneWrapper>());
    return wrappers;
}

vector<std::unique_ptr<ImageProviderWrapper>> HorsePlugin::create_image_providers() {
    return {};
}

HorsePlugin::HorsePlugin() = default;
