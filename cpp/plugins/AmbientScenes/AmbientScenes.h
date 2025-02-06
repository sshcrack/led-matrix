#pragma once

#include "plugin.h"

using Plugins::SceneWrapper;
using Plugins::ImageProviderWrapper;
using Plugins::BasicPlugin;

class AmbientScenes : public BasicPlugin {
public:
    AmbientScenes();

    vector<SceneWrapper *> get_scenes() override;

    vector<ImageProviderWrapper *> get_image_providers() override;
};