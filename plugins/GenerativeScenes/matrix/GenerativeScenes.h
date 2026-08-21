#pragma once

#include "shared/matrix/plugin/main.h"

using Plugins::SceneWrapper;
using Plugins::ImageProviderWrapper;
using Plugins::BasicPlugin;

namespace GenerativeScenes {
    class GenerativePlugin final : public BasicPlugin {
    public:
        GenerativePlugin();

        vector<std::unique_ptr<SceneWrapper>> create_scenes() override;

        vector<std::unique_ptr<ImageProviderWrapper>> create_image_providers() override;

        std::string get_plugin_name() const override {
            return PLUGIN_NAME;
        }
    };
}
