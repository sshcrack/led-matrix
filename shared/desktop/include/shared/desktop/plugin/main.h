#pragma once

#include <vector>
#include "shared/desktop/WebsocketClient.h"
#include <string>
#include <imgui.h>
#include <nlohmann/json.hpp>
#include "shared/common/plugin_macros.h"
#include "shared/desktop/config.h"
#include "shared/common/udp/packet.h"
#include "shared/desktop/macro.h"

using std::string;
using std::vector;
namespace fs = std::filesystem;

namespace Plugins
{
    class SHARED_DESKTOP_API DesktopPlugin
    {
    public:
        fs::path _plugin_location;

        [[nodiscard]] fs::path get_plugin_location() const
        {
            return _plugin_location;
        }

        virtual ~DesktopPlugin() = default;

        virtual void render() = 0;

        virtual void load_config(std::optional<const nlohmann::json> config) {}
        virtual void save_config(nlohmann::json &config) const {}
        virtual std::string get_plugin_name() const = 0;

        virtual void before_exit()
        {
        }

        /// You can here add a function that will be called once after everything is inited(ImGui, Platform and Renderer Backend)
        virtual void pre_new_frame() {}
        virtual void after_swap(ImGuiContext *imGuiContext) {}
        virtual void post_init() {}
        /// Called before the UDP mainloop of sending packets starts.
        virtual void udp_init() {}

        virtual void on_websocket_message(const std::string message) {}
        void send_websocket_message(const std::string &message)
        {
            if (WebsocketClient::instance() == nullptr)
            {
                spdlog::warn("WebsocketClient instance is null, cannot send message: {}", message);
                return;
            }

            WebsocketClient::instance()->webSocket.send("msg:" + get_plugin_name() + ":" + message);
        }

        // Return a vector of uint8_t if the plugin handles the scene and should send a packet
        [[nodiscard]] virtual std::optional<std::unique_ptr<UdpPacket, void (*)(UdpPacket *)>> compute_next_packet(const std::string sceneName)
        {
            return std::nullopt;
        }

        virtual void initialize_imgui(ImGuiContext * im_gui_context, ImGuiMemAllocFunc* alloc_fn, ImGuiMemFreeFunc* free_fn, void** user_data) = 0;
    };
}
