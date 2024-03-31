#pragma once

#include <glob.h>
#include <cstring>

#include <iostream>
#include <string>
#include <vector>

namespace PluginLoader {
    std::vector<std::string> lib_glob(const std::string &pattern);
}

