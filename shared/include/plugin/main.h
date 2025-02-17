#pragma once

#include <vector>
#include <string>
#include "wrappers.h"
#include <restinio/request_handler.hpp>

using std::vector;
using std::string;

namespace Plugins {
    class BasicPlugin {
        vector<std::shared_ptr<ImageProviderWrapper> > image_providers;
        vector<std::shared_ptr<SceneWrapper> > scenes;

        virtual vector<std::unique_ptr<ImageProviderWrapper, void(*)(ImageProviderWrapper *)> >
        create_image_providers() = 0;

        virtual vector<std::unique_ptr<SceneWrapper, void (*)(SceneWrapper *)> > create_scenes() = 0;

    public:
        std::string _plugin_location;

        [[nodiscard]] std::string get_plugin_location() const {
            return _plugin_location;
        }

        virtual ~BasicPlugin() = default;

        vector<std::shared_ptr<ImageProviderWrapper> > get_image_providers() {
            if (image_providers.empty()) {
                auto providers = create_image_providers();
                image_providers.reserve(providers.size());
                for (auto &item: providers) {
                    image_providers.push_back(std::move(item));
                }
            }

            return image_providers;
        }

        vector<std::shared_ptr<SceneWrapper> > get_scenes() {
            if (scenes.empty()) {
                auto sc = create_scenes();
                scenes.reserve(sc.size());
                for (auto &item: sc) {
                    scenes.push_back(std::move(item));
                }
            }

            return scenes;
        }

        virtual std::optional<string> post_init() {
            return std::nullopt;
        };

        virtual std::optional<string> pre_exit() {
            return std::nullopt;
        }

        /// Returns true if the request has been handled by this plugin
        virtual std::optional<restinio::request_handling_status_t>
        handle_request(const restinio::request_handle_t &req) {
            return std::nullopt;
        };
    };
}
