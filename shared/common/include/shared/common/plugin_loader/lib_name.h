#pragma once


#include <string>
#include <filesystem>
#include "shared/common/utils/macro.h"

namespace Plugins {
    SHARED_COMMON_API std::string get_lib_name(const std::filesystem::path &path);
}

