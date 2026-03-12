#include "AmbientScenes.h"
#include "scenes/StarFieldScene.h"
#include "scenes/MetaBlobScene.h"
#include "scenes/ClockScene.h"
#include "scenes/SortingVisualizerScene.h"
#include "scenes/BoidsScene.h"
#include "scenes/BouncingLogoScene.h"
#include "scenes/FallingSandScene.h"
#include "scenes/NeonTunnelScene.h"
#include "scenes/OrbitingPlanetsScene.h"
#include "scenes/KaleidoscopeGenesisScene.h"
#include "scenes/CrystalGrowthScene.h"

using namespace Scenes;
using namespace AmbientScenes;

extern "C" PLUGIN_EXPORT AmbientPlugin *createAmbientScenes() {
    return new AmbientPlugin();
}

extern "C" PLUGIN_EXPORT void destroyAmbientScenes(AmbientPlugin *c) {
    delete c;
}

vector<std::unique_ptr<SceneWrapper, void (*)(SceneWrapper *)> > AmbientPlugin::create_scenes() {
    vector<std::unique_ptr<SceneWrapper, void (*)(SceneWrapper *)> > scenes;
    scenes.push_back(std::unique_ptr<SceneWrapper, void (*)(SceneWrapper *)>(new StarFieldSceneWrapper(),
                                                                             [](SceneWrapper *scene) {
                                                                                 delete (StarFieldSceneWrapper *) scene;
                                                                             }));

    scenes.push_back(std::unique_ptr<SceneWrapper, void (*)(SceneWrapper *)>(new MetaBlobSceneWrapper(),
                                                                             [](SceneWrapper *scene) {
                                                                                 delete (MetaBlobSceneWrapper *) scene;
                                                                             }));

    scenes.push_back(std::unique_ptr<SceneWrapper, void (*)(SceneWrapper *)>(new ClockSceneWrapper(),
                                                                             [](SceneWrapper *scene) {
                                                                                 delete (ClockSceneWrapper *) scene;
                                                                             }));

    scenes.push_back(std::unique_ptr<SceneWrapper, void (*)(SceneWrapper *)>(new SortingVisualizerSceneWrapper(),
                                                                             [](SceneWrapper *scene) {
                                                                                 delete (SortingVisualizerSceneWrapper *) scene;
                                                                             }));

    scenes.push_back(std::unique_ptr<SceneWrapper, void (*)(SceneWrapper *)>(new BoidsSceneWrapper(),
                                                                             [](SceneWrapper *scene) {
                                                                                 delete (BoidsSceneWrapper *) scene;
                                                                             }));

    scenes.push_back(std::unique_ptr<SceneWrapper, void (*)(SceneWrapper *)>(new BouncingLogoSceneWrapper(),
                                                                             [](SceneWrapper *scene) {
                                                                                 delete (BouncingLogoSceneWrapper *) scene;
                                                                             }));

    scenes.push_back(std::unique_ptr<SceneWrapper, void (*)(SceneWrapper *)>(new FallingSandSceneWrapper(),
                                                                             [](SceneWrapper *scene) {
                                                                                 delete (FallingSandSceneWrapper *) scene;
                                                                             }));

    scenes.push_back(std::unique_ptr<SceneWrapper, void (*)(SceneWrapper *)>(new NeonTunnelSceneWrapper(),
                                                                             [](SceneWrapper *scene) {
                                                                                 delete (NeonTunnelSceneWrapper *) scene;
                                                                             }));

    scenes.push_back(std::unique_ptr<SceneWrapper, void (*)(SceneWrapper *)>(new OrbitingPlanetsSceneWrapper(),
                                                                             [](SceneWrapper *scene) {
                                                                                 delete (OrbitingPlanetsSceneWrapper *) scene;
                                                                             }));

    scenes.push_back(std::unique_ptr<SceneWrapper, void (*)(SceneWrapper *)>(new KaleidoscopeGenesisSceneWrapper(),
                                                                             [](SceneWrapper *scene) {
                                                                                 delete (KaleidoscopeGenesisSceneWrapper *) scene;
                                                                             }));

    scenes.push_back(std::unique_ptr<SceneWrapper, void (*)(SceneWrapper *)>(new CrystalGrowthSceneWrapper(),
                                                                             [](SceneWrapper *scene) {
                                                                                 delete (CrystalGrowthSceneWrapper *) scene;
                                                                             }));

    return scenes;
}

vector<std::unique_ptr<ImageProviderWrapper, void (*)(ImageProviderWrapper *)> >
AmbientPlugin::create_image_providers() {
    return {};
}


AmbientPlugin::AmbientPlugin() = default;
