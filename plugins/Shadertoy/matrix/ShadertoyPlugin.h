#pragma once

#include "shared/matrix/plugin/main.h"
#include <mutex>
#include <vector>

using Plugins::SceneWrapper;
using Plugins::ImageProviderWrapper;
using Plugins::ShaderProviderWrapper;
using Plugins::BasicPlugin;

class ShadertoyPlugin final : public BasicPlugin {
public:
    vector<std::unique_ptr<SceneWrapper, void (*)(Plugins::SceneWrapper *)> > create_scenes() override;

    vector<std::unique_ptr<ImageProviderWrapper, void(*)(ImageProviderWrapper *)> > create_image_providers() override;

    vector<std::unique_ptr<ShaderProviderWrapper, void(*)(ShaderProviderWrapper *)> > create_shader_providers() override;

    bool on_udp_packet(const uint8_t pluginId, const uint8_t *packetData,
                       const size_t size) override;

    std::string get_plugin_name() const override { return PLUGIN_NAME; }

    std::optional<std::vector<std::string>> on_websocket_open() override;
    void on_websocket_message(const std::string &message) override;

    std::vector<uint8_t> get_data() {
        std::lock_guard<std::mutex> lock(dataMutex);
        return data;
    }

private:
    std::mutex dataMutex;
    std::vector<uint8_t> data;
};
