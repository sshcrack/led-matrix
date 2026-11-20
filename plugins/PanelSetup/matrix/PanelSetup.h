#pragma once

#include "shared/matrix/plugin/main.h"

/// Scenes for setting up the panel hardware: the `panel_test` pattern the web
/// UI's "Panel hardware" card shows while the layout is being changed.
class PanelSetup : public Plugins::BasicPlugin {
public:
    vector<std::unique_ptr<Plugins::SceneWrapper>> create_scenes() override;
    vector<std::unique_ptr<Plugins::ImageProviderWrapper>> create_image_providers() override { return {}; }
    std::string get_plugin_name() const override { return PLUGIN_NAME; }
};
