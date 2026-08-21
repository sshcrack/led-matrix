#include "ExampleScenes.h"
#include "scenes/ColorPulseScene.h"
#include "scenes/PropertyDemoScene.h"
#include "scenes/RenderingDemoScene.h"

REGISTER_PLUGIN(ExampleScenes, ExampleScenes)

vector<std::unique_ptr<ImageProviderWrapper>> ExampleScenes::create_image_providers() {
    return {};
}

vector<std::unique_ptr<SceneWrapper>> ExampleScenes::create_scenes() {
    auto scenes = vector<std::unique_ptr<SceneWrapper>>();

    scenes.push_back(std::make_unique<Scenes::ColorPulseSceneWrapper>());
    scenes.push_back(std::make_unique<Scenes::PropertyDemoSceneWrapper>());
    scenes.push_back(std::make_unique<Scenes::RenderingDemoSceneWrapper>());
    return scenes;
}

ExampleScenes::ExampleScenes() = default;
