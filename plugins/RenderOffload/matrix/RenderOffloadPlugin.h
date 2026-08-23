#pragma once

#include <cstdint>
#include <vector>

#include <shared/matrix/plugin/main.h>

class RenderOffloadPlugin final : public Plugins::BasicPlugin {
public:
    std::vector<std::unique_ptr<Plugins::SceneWrapper>> create_scenes() override { return {}; }
    std::vector<std::unique_ptr<Plugins::ImageProviderWrapper>> create_image_providers() override { return {}; }
    std::string get_plugin_name() const override { return PLUGIN_NAME; }

    std::optional<std::string> before_server_init() override;
    std::optional<std::string> pre_exit() override;
    std::optional<std::vector<std::string>> on_websocket_open() override;
    void on_websocket_message(const std::string &message) override;
    bool on_udp_packet(uint8_t plugin_id, const uint8_t *data, size_t size) override;

private:
    std::uint32_t assembly_session_ = 0;
    std::uint32_t assembly_sequence_ = 0;
    int assembly_width_ = 0;
    int assembly_height_ = 0;
    std::uint16_t assembly_chunk_count_ = 0;
    std::size_t assembly_received_count_ = 0;
    bool assembly_submitted_ = false;
    std::vector<std::uint8_t> assembly_frame_;
    std::vector<std::uint8_t> assembly_received_;
};
