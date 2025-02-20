#pragma once

#include "plugin/main.h"

using Plugins::SceneWrapper;
using Plugins::ImageProviderWrapper;
using Plugins::BasicPlugin;

class PixelJoint : public BasicPlugin {
public:
    PixelJoint();

    vector<std::unique_ptr<SceneWrapper, void (*)(SceneWrapper *)>> create_scenes() override;
    vector<std::unique_ptr<ImageProviderWrapper, void(*)(ImageProviderWrapper*)>> create_image_providers() override;

    std::optional<restinio::request_handling_status_t> handle_request(const restinio::request_handle_t &req) override;;
};