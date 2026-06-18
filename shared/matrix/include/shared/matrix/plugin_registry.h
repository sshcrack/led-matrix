#pragma once
#include <string>
#include <unordered_map>
#include <mutex>
#include <any>

namespace PluginRegistry {

void set(const std::string& key, std::any ptr);
std::any get(const std::string& key);

}
