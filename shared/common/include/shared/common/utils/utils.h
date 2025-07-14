#pragma once
#include <cstdint>
#include <filesystem>

typedef int64_t tmillis_t;
tmillis_t GetTimeInMillis();

bool try_remove(const std::filesystem::path&);

bool is_valid_filename(const std::string& filename);
bool replace(std::string &str, const std::string &from, const std::string &to);
std::string stringify_url(const std::string& url);
std::optional<std::string> get_exec_dir();

int get_random_number_inclusive(int start, int end);