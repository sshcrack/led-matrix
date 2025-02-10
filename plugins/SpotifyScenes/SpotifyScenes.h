#pragma once

#include "plugin/main.h"

using Plugins::SceneWrapper;
using Plugins::ImageProviderWrapper;
using Plugins::BasicPlugin;

class SpotifyScenes : public BasicPlugin {
public:
    SpotifyScenes();

    vector<std::unique_ptr<SceneWrapper, void (*)(Plugins::SceneWrapper *)>> get_scenes() override;

    vector<std::unique_ptr<ImageProviderWrapper>> get_image_providers() override;

    std::optional<string> post_init() override;
};