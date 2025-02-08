#pragma once

#include "plugin/main.h"

using Plugins::SceneWrapper;
using Plugins::ImageProviderWrapper;
using Plugins::BasicPlugin;

class GithubScenes : public BasicPlugin {
public:
    GithubScenes();

    vector<SceneWrapper *> get_scenes() override;

    vector<ImageProviderWrapper *> get_image_providers() override;
};