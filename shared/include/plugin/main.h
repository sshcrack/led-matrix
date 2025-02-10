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
    public:
        virtual vector<std::unique_ptr<Plugins::ImageProviderWrapper>> get_image_providers() = 0;

        virtual vector<std::unique_ptr<SceneWrapper, void (*)(Plugins::SceneWrapper *)>> get_scenes() = 0;

        virtual std::optional<string> post_init() {
            return std::nullopt;
        };

        /// Returns true if the request has been handled by this plugin
        virtual std::optional<restinio::request_handling_status_t> handle_request(const restinio::request_handle_t &req) {
            return std::nullopt;
        };
    };
}