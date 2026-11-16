#pragma once
#include "shared/common/macro.h"
#include <cstdint>
#include <filesystem>
#include <optional>

typedef int64_t tmillis_t;
SHARED_COMMON_API tmillis_t GetTimeInMillis();

SHARED_COMMON_API bool try_remove(const std::filesystem::path&);

SHARED_COMMON_API bool is_valid_filename(const std::string& filename);
SHARED_COMMON_API bool replace(std::string &str, const std::string &from, const std::string &to);
SHARED_COMMON_API std::string stringify_url(const std::string& url);
SHARED_COMMON_API std::filesystem::path get_exec_file();
SHARED_COMMON_API std::filesystem::path get_exec_dir();

SHARED_COMMON_API int get_random_number_inclusive(int start, int end);

#ifndef _WIN32
/// For the child between fork() and exec(): closes every descriptor above
/// stderr. Sockets (ixwebsocket does not set close-on-exec), pipes of other
/// threads and log files would otherwise stay open in yt-dlp / ffmpeg / the
/// scene worker for as long as they live, which also keeps a closed
/// connection or a pipe's EOF from reaching its peer. Async-signal-safe.
SHARED_COMMON_API void close_inherited_descriptors();
#endif
