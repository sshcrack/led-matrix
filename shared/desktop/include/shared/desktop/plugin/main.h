#pragma once

#include <vector>
#include <string>
#include <optional>

using std::vector;
using std::string;

namespace Plugins {
    class DesktopPlugin {
        //virtual vector<std::unique_ptr<ImageProviderWrapper, void(*)(ImageProviderWrapper *)> > create_desktop_plugin() = 0;

    public:
        std::string _plugin_location;

        [[nodiscard]] std::string get_plugin_location() const {
            return _plugin_location;
        }

        virtual ~DesktopPlugin() = default;

        virtual std::optional<string> before_server_init() {
            return std::nullopt;
        };

        virtual std::optional<string> after_server_init() {
            return std::nullopt;
        };

        virtual std::optional<string> pre_exit() {
            return std::nullopt;
        }
    };
}
