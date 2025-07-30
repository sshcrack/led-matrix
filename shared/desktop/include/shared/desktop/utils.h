#pragma once

#include "shared/desktop/macro.h"
#include <filesystem>
#include <shared/common/utils/utils.h>

SHARED_DESKTOP_API bool isWritableExistingFile(const std::filesystem::path &path);
SHARED_DESKTOP_API std::filesystem::path get_data_dir();