#include "shared/common/utils/utils.h"
#include <spdlog/spdlog.h>
#include <random>
#include <regex>
#include <optional>



#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>
constexpr size_t PATH_BUF_SIZE = MAX_PATH;
#elif defined(__linux__)
#include <unistd.h>
    constexpr size_t PATH_BUF_SIZE = PATH_MAX;
#endif

#ifndef _WIN32
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace filesystem = std::filesystem;
using namespace spdlog;

#ifndef _WIN32
void close_inherited_descriptors()
{
#if defined(__linux__) && defined(SYS_close_range)
    if (syscall(SYS_close_range, 3U, ~0U, 0U) == 0)
        return;
#endif
    long limit = sysconf(_SC_OPEN_MAX);
    if (limit < 0 || limit > 65536)
        limit = 65536;
    for (int fd = 3; fd < limit; ++fd) close(fd);
}
#endif

tmillis_t GetTimeInMillis() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
}


bool try_remove(const filesystem::path &file_path) {
    if (!filesystem::exists(file_path))
        return true;

    try {
        trace("Removing {}", file_path.string());
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

#if defined(_WIN32)
namespace {
    // The file the process image was mapped from. Junctions in the launch path were resolved
    // when it was loaded, so this stays the version that runs even after the per-user
    // install re-points its `current` junction (canonicalizing the launch path would not).
    std::optional<std::filesystem::path> mapped_image_path() {
        std::wstring nt(32768, L'\0');
        const DWORD n = K32GetMappedFileNameW(GetCurrentProcess(), GetModuleHandleW(nullptr), nt.data(),
                                              static_cast<DWORD>(nt.size()));
        if (n == 0)
            return std::nullopt;
        nt.resize(n);
        // \Device\HarddiskVolume3\... -> C:\...
        wchar_t drive[] = L"A:";
        wchar_t device[1024];
        for (wchar_t letter = L'A'; letter <= L'Z'; letter++) {
            drive[0] = letter;
            if (QueryDosDeviceW(drive, device, 1024) == 0)
                continue;
            const std::wstring prefix(device);
            if (nt.size() > prefix.size() && nt.compare(0, prefix.size(), prefix) == 0 && nt[prefix.size()] == L'\\')
                return std::filesystem::path(std::wstring(drive) + nt.substr(prefix.size()));
        }
        return std::nullopt;
    }
}
#endif

static std::filesystem::path find_exec_file() {
        char buffer[PATH_BUF_SIZE];

#if defined(_WIN32)
    if (auto mapped = mapped_image_path())
        return *mapped;
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

#if defined(_WIN32)
    std::error_code ec;
    auto resolved = std::filesystem::canonical(std::filesystem::path(buffer), ec);
    if (!ec)
        return resolved;
#endif
    return std::filesystem::path(buffer);
}

std::filesystem::path get_exec_file() {
    // The running image never changes, while the path to it can (an update re-points the
    // desktop install's `current` link, or prunes the version): resolve it once.
    static const std::filesystem::path exec_file = find_exec_file();
    return exec_file;
}

std::filesystem::path get_exec_dir() {
    auto v = get_exec_file();

    return v.parent_path();
}
