#pragma once


#include <cctype>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <filesystem>

namespace Plugins {
    std::string get_lib_name(const std::filesystem::path &path);
}

