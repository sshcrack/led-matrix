#include "shared/desktop/plugin_loader/loader.h"

using namespace spdlog;
using Plugins::DesktopPlugin;
using Plugins::PluginManager;

PluginManager::PluginManager()
    : PluginLoader("led-matrix-desktop")
{}

PluginManager *PluginManager::instance_ = nullptr;

PluginManager *PluginManager::instance()
{
    if (instance_ == nullptr)
    {
        instance_ = new PluginManager();
    }
    return instance_;
}

std::vector<std::pair<std::string, DesktopPlugin *>> PluginManager::get_plugins()
{
    std::vector<std::pair<std::string, DesktopPlugin *>> plugins;
    for (const auto &item : loaded_plugins)
    {
        plugins.emplace_back(item.name, item.plugin);
    }
    return plugins;
}
