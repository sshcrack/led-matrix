#include "AmbientScenes.h"
#include "scenes/StarFieldScene.h"
#include "scenes/MetaBlobScene.h"
#include "scenes/ClockScene.h"
#include "scenes/SortingVisualizerScene.h"
#include "scenes/BoidsScene.h"
#include "scenes/BouncingLogoScene.h"
#include "scenes/FallingSandScene.h"
#include "scenes/NeonTunnelScene.h"
#include "scenes/DigitalRainScene.h"

using namespace Scenes;
using namespace AmbientScenes;

extern "C" PLUGIN_EXPORT AmbientPlugin *createAmbientScenes() {
    return new AmbientPlugin();
}

extern "C" PLUGIN_EXPORT void destroyAmbientScenes(AmbientPlugin *c) {
    delete c;
}

vector<std::unique_ptr<SceneWrapper>> AmbientPlugin::create_scenes() {
    vector<std::unique_ptr<SceneWrapper>> scenes;
    scenes.push_back(std::make_unique<StarFieldSceneWrapper>());
    scenes.push_back(std::make_unique<MetaBlobSceneWrapper>());
    scenes.push_back(std::make_unique<ClockSceneWrapper>());
    scenes.push_back(std::make_unique<SortingVisualizerSceneWrapper>());
    scenes.push_back(std::make_unique<BoidsSceneWrapper>());
    scenes.push_back(std::make_unique<BouncingLogoSceneWrapper>());
    scenes.push_back(std::make_unique<FallingSandSceneWrapper>());
    scenes.push_back(std::make_unique<NeonTunnelSceneWrapper>());
    scenes.push_back(std::make_unique<DigitalRainSceneWrapper>());
    return scenes;
}

vector<std::unique_ptr<ImageProviderWrapper> >
AmbientPlugin::create_image_providers() {
    return {};
}


AmbientPlugin::AmbientPlugin() = default;
