#pragma once

#include <vector>
#include <string>
#include <imgui.h>
#include <nlohmann/json.hpp>

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


#ifdef _WIN32
#define PLUGIN_EXPORT __declspec(dllexport)
#else
#define PLUGIN_EXPORT
#endif