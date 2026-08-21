#pragma once

#include <vector>
#include <string>
#include "shared/common/plugin_loader/PluginLoader.h"
#include "shared/desktop/plugin/main.h"
#include "shared/desktop/macro.h"

namespace Plugins
{
    class SHARED_DESKTOP_API PluginManager : public PluginLoader<DesktopPlugin>
    {
    protected:
        static PluginManager *instance_;

    private:
        explicit PluginManager();

    public:
        PluginManager(PluginManager &other) = delete;
        void operator=(const PluginManager &) = delete;

        static PluginManager *instance();

        std::vector<std::pair<std::string, DesktopPlugin *>> get_plugins();
    };
}
