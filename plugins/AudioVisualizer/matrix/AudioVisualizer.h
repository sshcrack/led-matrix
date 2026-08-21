#pragma once

#include "shared/matrix/plugin/main.h"
#include <shared/matrix/audio_state.h>
#include <shared/matrix/input_ids.h>

using Plugins::BasicPlugin;
using Plugins::ImageProviderWrapper;
using Plugins::SceneWrapper;

class AudioVisualizer : public BasicPlugin {
public:
    AudioVisualizer() = default;

    std::vector<std::unique_ptr<SceneWrapper>> create_scenes() override;
    std::vector<std::unique_ptr<ImageProviderWrapper>> create_image_providers() override;
    std::vector<std::unique_ptr<Previews::DataProvider>> create_preview_data_providers() override;
    [[nodiscard]] std::vector<std::string> get_runtime_input_ids() const override {
        return {std::string(RuntimeInputIds::Audio)};
    }
    std::optional<std::string> before_server_init() override;
    std::optional<std::string> pre_exit() override;
    bool on_udp_packet(uint8_t pluginId, const uint8_t *data, size_t size) override;
    AudioState::Snapshot get_audio_state() const;
    std::string get_plugin_name() const override { return PLUGIN_NAME; }

};
