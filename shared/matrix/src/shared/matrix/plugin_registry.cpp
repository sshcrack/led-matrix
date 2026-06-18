#include "shared/matrix/plugin_registry.h"

namespace {
    std::unordered_map<std::string, std::any> s_registry;
    std::mutex s_mutex;
}

void PluginRegistry::set(const std::string& key, std::any ptr) {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_registry[key] = std::move(ptr);
}

std::any PluginRegistry::get(const std::string& key) {
    std::lock_guard<std::mutex> lock(s_mutex);
    auto it = s_registry.find(key);
    return it != s_registry.end() ? it->second : std::any{};
}
