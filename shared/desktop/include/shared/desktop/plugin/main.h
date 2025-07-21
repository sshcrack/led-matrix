#pragma once

#include <vector>
#include <string>
#include <imgui.h>
#include <nlohmann/json.hpp>
#include "shared/common/plugin_macros.h"
#include "shared/desktop/config.h"

using std::string;
using std::vector;
namespace fs = std::filesystem;

namespace Plugins
{
    class DesktopPlugin
    {
        // virtual vector<std::unique_ptr<ImageProviderWrapper, void(*)(ImageProviderWrapper *)> > create_some_provider() = 0;

    public:
        fs::path _plugin_location;

        [[nodiscard]] fs::path get_plugin_location() const
        {
            return _plugin_location;
        }

        virtual ~DesktopPlugin() = default;

        virtual void render(ImGuiContext *ctx) = 0;

        virtual void loadConfig(std::optional<const nlohmann::json> config) = 0;
        virtual void saveConfig(nlohmann::json &config) const = 0;

        virtual void beforeExit() {}
    };
}