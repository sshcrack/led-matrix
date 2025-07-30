#pragma once

#include "shared/matrix/plugin/main.h"

using Plugins::SceneWrapper;
using Plugins::ImageProviderWrapper;
using Plugins::BasicPlugin;

class PixelJoint : public BasicPlugin {
public:
    PixelJoint();

    vector<std::unique_ptr<SceneWrapper, void (*)(SceneWrapper *)>> create_scenes() override;
    vector<std::unique_ptr<ImageProviderWrapper, void(*)(ImageProviderWrapper*)>> create_image_providers() override;

    std::unique_ptr<router_t> register_routes(std::unique_ptr<router_t> router) override;
    std::string get_plugin_name() const override { return PLUGIN_NAME; }
};