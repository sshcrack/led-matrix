#pragma once

#include "shared/matrix/plugin/main.h"

using Plugins::BasicPlugin;
using Plugins::ImageProviderWrapper;
using Plugins::SceneWrapper;

namespace Hobbies {
// Hobbies as ambient scenes: horses, hiking in the hills, basketball, soccer.
class HobbyPlugin final : public BasicPlugin {
public:
    HobbyPlugin();

    vector<std::unique_ptr<SceneWrapper>> create_scenes() override;

    vector<std::unique_ptr<ImageProviderWrapper>> create_image_providers() override;

    std::string get_plugin_name() const override { return PLUGIN_NAME; }
};
}  // namespace Hobbies
