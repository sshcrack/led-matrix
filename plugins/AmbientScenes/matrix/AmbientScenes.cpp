#include "AmbientScenes.h"
#include "scenes/StarFieldScene.h"
#include "scenes/MetaBlobScene.h"
#include "scenes/ClockScene.h"

using namespace Scenes;
using namespace AmbientScenes;

extern "C" [[maybe_unused]] AmbientPlugin *createAmbientScenes() {
    return new AmbientPlugin();
}

extern "C" [[maybe_unused]] void destroyAmbientScenes(AmbientPlugin *c) {
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

    return scenes;
}

vector<std::unique_ptr<ImageProviderWrapper, void (*)(ImageProviderWrapper *)> >
AmbientPlugin::create_image_providers() {
    return {};
}


AmbientPlugin::AmbientPlugin() = default;
