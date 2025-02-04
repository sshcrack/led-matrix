#pragma once

#include "plugin.h"

using Plugins::SceneWrapper;
using Plugins::ImageProviderWrapper;
using Plugins::BasicPlugin;

class SpotifyScenes : public BasicPlugin {
public:
    SpotifyScenes();

    vector<SceneWrapper *> get_scenes() override;

    vector<ImageProviderWrapper *> get_image_providers() override;

    std::optional<string> post_init() override;
};