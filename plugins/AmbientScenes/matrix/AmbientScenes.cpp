#include "AmbientScenes.h"
#include "scenes/StarFieldScene.h"
#include "scenes/MetaBlobScene.h"
#include "scenes/ClockScene.h"
#include "scenes/SortingVisualizerScene.h"
#include "scenes/BouncingLogoScene.h"
#include "scenes/NeonTunnelScene.h"
#include "scenes/DigitalRainScene.h"

using namespace Scenes;
using namespace AmbientScenes;

REGISTER_PLUGIN(AmbientScenes, AmbientPlugin)

vector<std::unique_ptr<SceneWrapper>> AmbientPlugin::create_scenes() {
    vector<std::unique_ptr<SceneWrapper>> created;
    created.push_back(std::make_unique<StarFieldSceneWrapper>());
    created.push_back(std::make_unique<MetaBlobSceneWrapper>());
    created.push_back(std::make_unique<ClockSceneWrapper>());
    created.push_back(std::make_unique<SortingVisualizerSceneWrapper>());
    created.push_back(std::make_unique<BouncingLogoSceneWrapper>());
    created.push_back(std::make_unique<NeonTunnelSceneWrapper>());
    created.push_back(std::make_unique<DigitalRainSceneWrapper>());
    return created;
}

vector<std::unique_ptr<ImageProviderWrapper> >
AmbientPlugin::create_image_providers() {
    return {};
}


AmbientPlugin::AmbientPlugin() = default;
