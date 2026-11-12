#pragma once

#include <vector>
#include <string>
#include <functional>
#include <shared/matrix/wrappers.h>
#include <shared/matrix/server/common.h>
// ReSharper disable once CppUnusedIncludeDirective
// This is included so the plugins don't have to include it every time
#include "shared/common/plugin_macros.h"
#include "shared/matrix/post_processing_effect.h"
#include "shared/matrix/transition_effect.h"
#include "shared/matrix/preview.h"

using std::string;
using std::vector;
using router_t = restinio::router::express_router_t<>;

namespace Plugins
{
    class BasicPlugin
    {
        vector<std::shared_ptr<ImageProviderWrapper>> image_providers;
        vector<std::shared_ptr<ShaderProviderWrapper>> shader_providers;
        vector<std::shared_ptr<SceneWrapper>> scenes;
        vector<std::shared_ptr<Previews::DataProvider>> preview_data_providers;

        virtual vector<std::unique_ptr<ImageProviderWrapper>>
        create_image_providers() = 0;

        virtual vector<std::unique_ptr<ShaderProviderWrapper>>
        create_shader_providers() {
            return {};
        }

        virtual vector<std::unique_ptr<SceneWrapper>> create_scenes() = 0;

        virtual vector<std::unique_ptr<Previews::DataProvider>> create_preview_data_providers() {
            return {};
        }

    public:
        // Transport seam, matching DesktopPlugin: integration tests can join
        // the real scene/plugin logic without opening a production socket.
        using MessageSink = std::function<void(const std::string&, const std::string&)>;
        static SHARED_MATRIX_API void set_message_sink(MessageSink sink);
        static SHARED_MATRIX_API bool deliver_to_message_sink(const std::string& plugin, const std::string& message);
        virtual vector<std::unique_ptr<PostProcessingEffect>> create_effects() {
            return {};
        }

        virtual vector<std::unique_ptr<TransitionEffect>> create_transitions() {
            return {};
        }

        /// Runtime Input ids this plugin can publish in production. Registry
        /// validation uses these declarations to reject misspelled or orphaned
        /// scene requirements before playback starts.
        [[nodiscard]] virtual vector<std::string> get_runtime_input_ids() const {
            return {};
        }
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

        vector<std::shared_ptr<ShaderProviderWrapper>> get_shader_providers()
        {
            if (shader_providers.empty())
            {
                auto providers = create_shader_providers();
                shader_providers.reserve(providers.size());
                for (auto &item : providers)
                {
                    shader_providers.push_back(std::move(item));
                }
            }

            return shader_providers;
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


        vector<std::shared_ptr<Previews::DataProvider>> get_preview_data_providers()
        {
            if (preview_data_providers.empty())
            {
                auto providers = create_preview_data_providers();
                preview_data_providers.reserve(providers.size());
                for (auto &item : providers)
                    preview_data_providers.push_back(std::move(item));
            }
            return preview_data_providers;
        }

        // Lifecycle hooks. Hosts run them through plugin/lifecycle.h, so a
        // returned error string or a thrown exception is handled the same way
        // in the daemon, the scene worker and the preview generator: init
        // failures are fatal, pre_exit failures are logged.
        //
        // before_server_init(): runs before the HTTP server accepts requests.
        //   Create every object that register_routes() handlers touch here.
        // after_server_init(): runs once the HTTP server is listening, so it
        //   may block on a request to it (e.g. an OAuth callback). Routes can
        //   already be called while it runs.
        // pre_exit(): runs during shutdown; must tolerate the init hooks having
        //   failed or never run.
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
        virtual std::unique_ptr<router_t> register_routes(std::unique_ptr<router_t> router) { return router; }

        /// Return true if the request has been handled by this plugin
        virtual bool on_udp_packet(const uint8_t /*pluginId*/, const uint8_t* /*data*/, const size_t /*size*/) { return false; }

        virtual std::string get_plugin_name() const = 0;

        // Some desktop plugins are single physical media producers. Multiple
        // controller connections may coexist, but only the elected producer
        // owner may receive commands or publish state for such a plugin.
        [[nodiscard]] virtual bool requires_single_desktop_producer() const
        {
            return false;
        }

        virtual void on_websocket_message(const std::string& /*message*/)
        {
            // Default implementation does nothing
        }

        virtual void send_msg_to_desktop(const std::string &msg)
        {
            if (deliver_to_message_sink(get_plugin_name(), msg))
                return;
            namespace rb = restinio::websocket::basic;

            rb::message_t message;
            message.set_final_flag(rb::final_frame_flag_t::final_frame);
            message.set_opcode(rb::opcode_t::text_frame);
            message.set_payload("msg:" + get_plugin_name() + ":" + msg);

            if (requires_single_desktop_producer())
            {
                const auto targets = Server::desktop_producer_targets();
                std::shared_lock lock(Server::registryMutex);
                for (const auto connection_id : targets) {
                    const auto it = Server::registry.find(connection_id);
                    if (it != Server::registry.end())
                        it->second->send_message(message);
                }
                return;
            }

            std::shared_lock lock(Server::registryMutex);
            for (const auto &val : Server::registry | std::views::values)
            {
                val->send_message(message);
            }
        }

    };
}
