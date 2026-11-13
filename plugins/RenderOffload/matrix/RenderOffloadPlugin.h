#pragma once

#include <shared/matrix/plugin/main.h>

#include <array>
#include <cstdint>
#include <vector>

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
    /// One frame being reassembled. Several are kept so chunks of frame N that
    /// arrive after the first chunks of N+1 (Wi-Fi reordering) still complete N.
    struct Assembly {
        bool in_use = false;
        std::uint32_t session = 0;
        std::uint32_t sequence = 0;
        int width = 0;
        int height = 0;
        std::uint16_t chunk_count = 0;
        std::size_t received_count = 0;
        std::vector<std::uint8_t> frame;
        std::vector<std::uint8_t> received;
    };
    static constexpr std::size_t AssemblySlots = 3;

    Assembly* assembly_for(std::uint32_t session, std::uint32_t sequence);
    void drop_older(std::uint32_t session, std::uint32_t sequence);

    std::array<Assembly, AssemblySlots> assemblies_;
    // Newest frame published to RemoteRender; older chunks are late.
    std::uint32_t published_session_ = 0;
    std::uint32_t published_sequence_ = 0;
};
