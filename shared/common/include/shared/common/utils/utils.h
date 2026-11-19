#pragma once
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>

#include "shared/common/macro.h"

typedef int64_t tmillis_t;
SHARED_COMMON_API tmillis_t GetTimeInMillis();

SHARED_COMMON_API bool try_remove(const std::filesystem::path&);

SHARED_COMMON_API bool is_valid_filename(const std::string& filename);
SHARED_COMMON_API bool replace(std::string &str, const std::string &from, const std::string &to);
SHARED_COMMON_API std::string stringify_url(const std::string& url);
SHARED_COMMON_API std::filesystem::path get_exec_file();
SHARED_COMMON_API std::filesystem::path get_exec_dir();
/// `<prefix>/share/<app>` of the install the running binary (`<prefix>/bin/...`)
/// belongs to, so an install unpacked anywhere (a version folder on the panel's
/// data partition, an emulator install tree) uses its own files; `fallback`
/// when that directory does not exist.
SHARED_COMMON_API std::filesystem::path get_share_dir(std::string_view app, const std::filesystem::path& fallback);

SHARED_COMMON_API int get_random_number_inclusive(int start, int end);

#ifndef _WIN32
/// For the child between fork() and exec(): closes every descriptor above
/// stderr. Sockets (ixwebsocket does not set close-on-exec), pipes of other
/// threads and log files would otherwise stay open in yt-dlp / ffmpeg / the
/// scene worker for as long as they live, which also keeps a closed
/// connection or a pipe's EOF from reaching its peer. Async-signal-safe.
SHARED_COMMON_API void close_inherited_descriptors();
#endif
