#pragma once

#include "shared/matrix/plugin/main.h"

using Plugins::SceneWrapper;
using Plugins::ImageProviderWrapper;
using Plugins::BasicPlugin;

namespace AmbientScenes {
    class AmbientPlugin final : public BasicPlugin {
    public:
        AmbientPlugin();

        vector<std::unique_ptr<SceneWrapper, void (*)(SceneWrapper *)>> create_scenes() override;

        vector<std::unique_ptr<ImageProviderWrapper, void(*)(ImageProviderWrapper*)>> create_image_providers() override;

        std::string get_plugin_name() const override {
            return PLUGIN_NAME;
        }
    };
}