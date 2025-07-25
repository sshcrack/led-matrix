#pragma once

#include <vector>
#include <string>
#include <shared/matrix/wrappers.h>
#include <shared/matrix/server/common.h>
// ReSharper disable once CppUnusedIncludeDirective
// This is included so the plugins don't have to include it every time
#include "shared/common/plugin_macros.h"

using std::string;
using std::vector;
using router_t = restinio::router::express_router_t<>;

namespace Plugins
{
    class BasicPlugin
    {
        vector<std::shared_ptr<ImageProviderWrapper>> image_providers;
        vector<std::shared_ptr<SceneWrapper>> scenes;

        virtual vector<std::unique_ptr<ImageProviderWrapper, void (*)(ImageProviderWrapper *)>>
        create_image_providers() = 0;

        virtual vector<std::unique_ptr<SceneWrapper, void (*)(SceneWrapper *)>> create_scenes() = 0;

    public:
        std::string _plugin_location;

        [[nodiscard]] std::string get_plugin_location() const
        {
            return _plugin_location;
        }

        virtual ~BasicPlugin() = default;

        vector<std::shared_ptr<ImageProviderWrapper>> get_image_providers()
        {
            if (image_providers.empty())
            {
                auto providers = create_image_providers();
                image_providers.reserve(providers.size());
                for (auto &item : providers)
                {
                    image_providers.push_back(std::move(item));
                }
            }

            return image_providers;
        }

        vector<std::shared_ptr<SceneWrapper>> get_scenes()
        {
            if (scenes.empty())
            {
                auto sc = create_scenes();
                scenes.reserve(sc.size());
                for (auto &item : sc)
                {
                    scenes.push_back(std::move(item));
                }
            }

            return scenes;
        }

        virtual std::optional<string> before_server_init()
        {
            return std::nullopt;
        };

        virtual std::optional<string> after_server_init()
        {
            return std::nullopt;
        };

        virtual std::optional<string> pre_exit()
        {
            return std::nullopt;
        }

        /// Return with a string value to send an initial message to the desktop client
        virtual std::optional<std::vector<std::string>> on_websocket_open()
        {
            // Default implementation does nothing
            return std::nullopt;
        }

        /// Returns true if the request has been handled by this plugin
        virtual std::unique_ptr<router_t> register_routes(std::unique_ptr<router_t> router)
        {
            return std::move(router);
        }

        /// Return true if the request has been handled by this plugin
        virtual bool on_udp_packet(const uint8_t pluginId, const uint8_t *data, const size_t size)
        {
            return false;
        }

        virtual std::string get_plugin_name() const = 0;
        virtual void on_websocket_message(const std::string &message)
        {
            // Default implementation does nothing
        }

        virtual void send_msg_to_desktop(const std::string &msg)
        {
            namespace rb = restinio::websocket::basic;

            std::shared_lock lock(Server::registryMutex);
            rb::message_t message;
            message.set_final_flag(rb::final_frame_flag_t::final_frame);
            message.set_opcode(rb::opcode_t::text_frame);

            message.set_payload("msg:" + get_plugin_name() + ":" + msg);

            for (const auto &val : Server::registry | std::views::values)
            {
                val->send_message(message);
            }
        }
    };
}
