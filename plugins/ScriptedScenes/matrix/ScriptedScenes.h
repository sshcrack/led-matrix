#pragma once

#include "shared/matrix/plugin/main.h"
#include "LuaScene.h"

class ScriptedScenes : public Plugins::BasicPlugin {
public:
    ScriptedScenes() = default;

    std::string get_plugin_name() const override { return PLUGIN_NAME; }

    vector<std::unique_ptr<Plugins::ImageProviderWrapper,
                            void (*)(Plugins::ImageProviderWrapper *)>>
    create_image_providers() override { return {}; }

    vector<std::unique_ptr<Plugins::SceneWrapper,
                            void (*)(Plugins::SceneWrapper *)>>
    create_scenes() override;
};
