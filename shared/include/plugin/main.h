#pragma once

#include <vector>
#include <string>
#include "wrappers.h"
#include <restinio/request_handler.hpp>
#include "plugin/property.h"

using std::vector;
using std::string;

namespace Plugins {
    class BasicPlugin {
    private:
        vector<std::shared_ptr<Plugins::ImageProviderWrapper>> image_providers;
        vector<std::shared_ptr<Plugins::SceneWrapper>> scenes;

        virtual vector<std::unique_ptr<Plugins::ImageProviderWrapper, void(*)(Plugins::ImageProviderWrapper*)>> create_image_providers() = 0;
        virtual vector<std::unique_ptr<SceneWrapper, void (*)(SceneWrapper *)>> create_scenes() = 0;
    public:
        ~BasicPlugin() = default;

        vector<std::shared_ptr<Plugins::ImageProviderWrapper>> get_image_providers() {
            if (image_providers.empty()) {
                auto providers = create_image_providers();
                image_providers.reserve(providers.size());
                for (auto &item: providers) {
                    image_providers.push_back(std::move(item));
                }
            }

            return image_providers;
        }

        vector<std::shared_ptr<Plugins::SceneWrapper>> get_scenes() {
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

        /// Returns true if the request has been handled by this plugin
        virtual std::optional<restinio::request_handling_status_t> handle_request(const restinio::request_handle_t &req) {
            return std::nullopt;
        };
    };
}