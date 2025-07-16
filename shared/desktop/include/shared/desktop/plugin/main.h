#pragma once

#include <vector>
#include <string>
#include <imgui.h>
#include <nlohmann/json.hpp>
#include "shared/common/plugin_macros.h"
#include "shared/desktop/config.h"

using std::vector;
using std::string;

namespace Plugins {
    class DesktopPlugin {
        //virtual vector<std::unique_ptr<ImageProviderWrapper, void(*)(ImageProviderWrapper *)> > create_some_provider() = 0;

    public:
        std::string _plugin_location;

        [[nodiscard]] std::string get_plugin_location() const {
            return _plugin_location;
        }

        virtual ~DesktopPlugin() = default;

        virtual void render(ImGuiContext* ctx) = 0;

        virtual void loadConfig(const nlohmann::json& config) = 0;
        virtual void saveConfig(nlohmann::json& config) const = 0;
    };
}