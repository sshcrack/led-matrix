#pragma once

#include "plugin.h"

#ifndef PLUGIN_TEST
using Plugins::SceneWrapper;
using Plugins::ImageTypeWrapper;
#endif
using Plugins::BasicPlugin;

class PixelJoint : public BasicPlugin {
public:
    PixelJoint();

#ifndef PLUGIN_TEST
    vector<SceneWrapper *> get_scenes() override;
    vector<ImageTypeWrapper *> get_images_types() override;
#endif

    string test() override;
};