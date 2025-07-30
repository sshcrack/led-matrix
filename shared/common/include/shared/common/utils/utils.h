#pragma once
#include "shared/common/utils/macro.h"
#include <cstdint>
#include <filesystem>
#include <optional>

typedef int64_t tmillis_t;
tmillis_t GetTimeInMillis();

SHARED_COMMON_API bool try_remove(const std::filesystem::path&);

SHARED_COMMON_API bool is_valid_filename(const std::string& filename);
SHARED_COMMON_API bool replace(std::string &str, const std::string &from, const std::string &to);
SHARED_COMMON_API std::string stringify_url(const std::string& url);
SHARED_COMMON_API std::filesystem::path get_exec_file();
SHARED_COMMON_API std::filesystem::path get_exec_dir();

SHARED_COMMON_API int get_random_number_inclusive(int start, int end);