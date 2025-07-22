#pragma once

#include <filesystem>
#include <shared/common/utils/utils.h>

bool isWritableExistingFile(const std::filesystem::path &path);
std::filesystem::path get_data_dir();