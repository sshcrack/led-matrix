#include "shared/common/utils/utils.h"
#include <spdlog/spdlog.h>
#include <random>
#include <regex>



#if defined(_WIN32)
#include <windows.h>
constexpr size_t PATH_BUF_SIZE = MAX_PATH;
#elif defined(__linux__)
#include <unistd.h>
    constexpr size_t PATH_BUF_SIZE = PATH_MAX;
#endif

namespace filesystem = std::filesystem;
using namespace spdlog;


tmillis_t GetTimeInMillis() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
}


bool try_remove(const filesystem::path &file_path) {
    if (!filesystem::exists(file_path))
        return true;

    try {
        debug("Removing {}", file_path.string());
        return filesystem::remove(file_path);
    } catch (std::exception &ex) {
        warn("Could not delete file {}: {}", file_path.string(), ex.what());
        return false;
    }
}

bool is_valid_filename(const std::string &filename) {
    std::regex file_regex("^[\\w\\-. ]+$");
    std::smatch sm;
    regex_match(filename, sm, file_regex);

    if (sm.empty())
        return false;

    return true;
}


bool replace(std::string &str, const std::string &from, const std::string &to) {
    size_t start_pos = str.find(from);
    if (start_pos == std::string::npos)
        return false;
    str.replace(start_pos, from.length(), to);
    return true;
}


std::string stringify_url(const std::string &url) {
    std::hash<std::string> hasher;
    size_t hashed_url = hasher(url);
    return std::to_string(hashed_url);
}


int get_random_number_inclusive(int start, int end) {
    // Use a random_device to seed the generator
    std::random_device rd;

    // Use a Mersenne Twister engine for random number generation
    std::mt19937 engine(rd());

    // Use a uniform distribution for the range
    std::uniform_int_distribution<> dist(start, end);

    // Generate and return the random number
    return dist(engine);
}

std::filesystem::path get_exec_file() {
        char buffer[PATH_BUF_SIZE];

#if defined(_WIN32)
    DWORD len = GetModuleFileNameA(NULL, buffer, sizeof(buffer));
    if (len == 0 || len == sizeof(buffer)) {
        throw std::runtime_error("GetModuleFileNameA failed");
    }
#elif defined(__linux__)
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len == -1) {
        throw std::runtime_error("readlink failed");
    }
    buffer[len] = '\0';
#else
    #error Unsupported platform
#endif

    return std::filesystem::path(buffer);
}

std::filesystem::path get_exec_dir() {
    auto v = get_exec_file();

    return v.parent_path();
}
