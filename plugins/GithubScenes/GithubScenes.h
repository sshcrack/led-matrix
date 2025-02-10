#pragma once

#include "plugin/main.h"

using Plugins::SceneWrapper;
using Plugins::ImageProviderWrapper;
using Plugins::BasicPlugin;

class GithubScenes : public BasicPlugin {
public:
    GithubScenes();

    vector<std::unique_ptr<SceneWrapper, void (*)(Plugins::SceneWrapper *)>> get_scenes() override;

    vector<std::unique_ptr<ImageProviderWrapper>> get_image_providers() override;
};