#pragma once

#include <shared/matrix/frame_stream.h>
#include <shared/matrix/plugin/main.h>

class RenderOffloadPlugin final : public Plugins::BasicPlugin {
public:
    std::vector<std::unique_ptr<Plugins::SceneWrapper>> create_scenes() override { return {}; }
    std::vector<std::unique_ptr<Plugins::ImageProviderWrapper>> create_image_providers() override { return {}; }
    std::string get_plugin_name() const override { return PLUGIN_NAME; }
    [[nodiscard]] bool requires_single_desktop_producer() const override { return true; }

    std::optional<std::string> before_server_init() override;
    std::optional<std::string> pre_exit() override;
    std::optional<std::vector<std::string>> on_websocket_open() override;
    void on_websocket_message(const std::string &message) override;
    bool on_udp_packet(uint8_t plugin_id, const uint8_t *data, size_t size) override;

private:
    FrameStream::Assembler assembler_{"RenderOffload"};
};
