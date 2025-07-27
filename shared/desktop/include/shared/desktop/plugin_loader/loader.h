#pragma once

#include <vector>
#include "shared/desktop/plugin/main.h"
#include "shared/desktop/macro.h"

namespace Plugins
{
    struct SHARED_DESKTOP_API PluginInfo
    {
        void *handle;
        std::string destroyFnName;
        std::string name;

        DesktopPlugin *plugin;
    };

    class SHARED_DESKTOP_API PluginManager
    {
    protected:
        static PluginManager *instance_;

    private:
        /// Handle, DestroyFunction, Plugin
        std::vector<PluginInfo> loaded_plugins;

        bool initialized = false;

        explicit PluginManager();

    public:
        PluginManager(PluginManager &other) = delete;
        void operator=(const PluginManager &) = delete;

        static PluginManager *instance();
        void initialize();
        void delete_references();
        void destroy_plugins();

        std::vector<std::pair<std::string, DesktopPlugin *>> get_plugins();
    };
}