#pragma once
#include <string>
#include <unordered_map>
#include <mutex>

namespace PluginRegistry {

void set(const std::string& key, void* ptr);
void* get(const std::string& key);

}
