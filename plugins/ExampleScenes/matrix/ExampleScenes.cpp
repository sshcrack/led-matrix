#include "ExampleScenes.h"
#include "scenes/ColorPulseScene.h"
#include "scenes/PropertyDemoScene.h"
#include "scenes/RenderingDemoScene.h"

extern "C" PLUGIN_EXPORT ExampleScenes *createExampleScenes() {
    return new ExampleScenes();
}

extern "C" PLUGIN_EXPORT void destroyExampleScenes(ExampleScenes *c) {
    delete c;
}

vector<std::unique_ptr<ImageProviderWrapper, void(*)(ImageProviderWrapper*)>> ExampleScenes::create_image_providers() {
    return {};
}

vector<std::unique_ptr<SceneWrapper, void (*)(Plugins::SceneWrapper *)>> ExampleScenes::create_scenes() {
    auto scenes = vector<std::unique_ptr<SceneWrapper, void(*)(Plugins::SceneWrapper*)>>();
    auto deleteScene = [](SceneWrapper* scene) {
        delete scene;
    };

    scenes.push_back({new Scenes::ColorPulseSceneWrapper(), deleteScene});
    scenes.push_back({new Scenes::PropertyDemoSceneWrapper(), deleteScene});
    scenes.push_back({new Scenes::RenderingDemoSceneWrapper(), deleteScene});
    return scenes;
}

ExampleScenes::ExampleScenes() = default;
