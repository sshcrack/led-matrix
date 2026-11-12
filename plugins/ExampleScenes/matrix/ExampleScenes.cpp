#include "ExampleScenes.h"
#include "scenes/ColorPulseScene.h"
#include "scenes/PropertyDemoScene.h"
#include "scenes/RenderingDemoScene.h"

REGISTER_PLUGIN(ExampleScenes, ExampleScenes)

vector<std::unique_ptr<ImageProviderWrapper>> ExampleScenes::create_image_providers() {
    return {};
}

vector<std::unique_ptr<SceneWrapper>> ExampleScenes::create_scenes() {
    auto created = vector<std::unique_ptr<SceneWrapper>>();

    created.push_back(std::make_unique<Scenes::ColorPulseSceneWrapper>());
    created.push_back(std::make_unique<Scenes::PropertyDemoSceneWrapper>());
    created.push_back(std::make_unique<Scenes::RenderingDemoSceneWrapper>());
    return created;
}

ExampleScenes::ExampleScenes() = default;
