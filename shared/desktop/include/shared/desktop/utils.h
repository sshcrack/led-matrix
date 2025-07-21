#pragma once

#include <filesystem>
#include <iostream>
#include <fstream>
#include <string>
#include <shared/common/utils/utils.h>

namespace fs = std::filesystem;

bool isWritableExistingFile(const std::filesystem::path &path);
fs::path get_data_dir();