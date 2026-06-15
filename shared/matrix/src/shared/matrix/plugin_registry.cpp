#include "shared/matrix/plugin_registry.h"

namespace {
    std::unordered_map<std::string, void*> s_registry;
    std::mutex s_mutex;
}

void PluginRegistry::set(const std::string& key, void* ptr) {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_registry[key] = ptr;
}

void* PluginRegistry::get(const std::string& key) {
    std::lock_guard<std::mutex> lock(s_mutex);
    auto it = s_registry.find(key);
    return it != s_registry.end() ? it->second : nullptr;
}
