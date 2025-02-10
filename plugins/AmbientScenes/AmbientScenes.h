#pragma once

#include "plugin/main.h"

using Plugins::SceneWrapper;
using Plugins::ImageProviderWrapper;
using Plugins::BasicPlugin;

namespace AmbientScenes {
    class AmbientPlugin : public BasicPlugin {
    public:
        AmbientPlugin();

        vector<std::unique_ptr<SceneWrapper, void (*)(Plugins::SceneWrapper *)>> get_scenes() override;

        vector<std::unique_ptr<ImageProviderWrapper>> get_image_providers() override;
    };
}