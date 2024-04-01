#pragma once

#include "plugin.h"

using Plugins::SceneWrapper;
using Plugins::ImageTypeWrapper;
using Plugins::BasicPlugin;

class PixelJoint : public BasicPlugin {
public:
    PixelJoint();

    vector<SceneWrapper *> get_scenes() override;
    vector<ImageTypeWrapper *> get_images_types() override;
};