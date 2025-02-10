#pragma once

/// All credits for the scenes / anim code go to
/// https://github.com/Footleg/RGBMatrixAnimations/tree/main
/// I just converted it so fit the plugin format

#include "plugin/main.h"

using Plugins::SceneWrapper;
using Plugins::ImageProviderWrapper;
using Plugins::BasicPlugin;

class RGBMatrixAnimations : public BasicPlugin {
public:
    RGBMatrixAnimations();

    vector<std::unique_ptr<SceneWrapper, void (*)(Plugins::SceneWrapper *)>> get_scenes() override;
    vector<std::unique_ptr<ImageProviderWrapper>> get_image_providers() override;
};
