#pragma once

#include "plugin/main.h"

using Plugins::SceneWrapper;
using Plugins::ImageProviderWrapper;
using Plugins::BasicPlugin;

class PixelJoint : public BasicPlugin {
public:
    PixelJoint();

    vector<SceneWrapper *> get_scenes() override;
    vector<ImageProviderWrapper *> get_image_providers() override;
};