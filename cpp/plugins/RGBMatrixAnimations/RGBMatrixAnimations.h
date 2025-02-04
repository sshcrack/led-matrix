#pragma once

/// All credits for the scenes / anim code go to
/// https://github.com/Footleg/RGBMatrixAnimations/tree/main
/// I just converted it so fit the plugin format

#include "plugin.h"

using Plugins::SceneWrapper;
using Plugins::ImageProviderWrapper;
using Plugins::BasicPlugin;

class RGBMatrixAnimations : public BasicPlugin {
public:
    RGBMatrixAnimations();

    vector<SceneWrapper *> get_scenes() override;
    vector<ImageProviderWrapper *> get_image_providers() override;
};
