#pragma once

#include "shared/matrix/plugin/main.h"

using Plugins::SceneWrapper;
using Plugins::ImageProviderWrapper;
using Plugins::BasicPlugin;

class GameScenes : public BasicPlugin {
public:
    GameScenes();

    vector<std::unique_ptr<SceneWrapper, void (*)(Plugins::SceneWrapper *)>> create_scenes() override;

    vector<std::unique_ptr<ImageProviderWrapper, void(*)(Plugins::ImageProviderWrapper*)>> create_image_providers() override;
};