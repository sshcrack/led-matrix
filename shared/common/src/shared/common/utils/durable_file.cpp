#include "shared/common/utils/durable_file.h"

#include <fmt/format.h>

#include <cerrno>
#include <cstring>
#include <fstream>
#include <random>
#include <system_error>

#ifndef _WIN32
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace DurableFile {
namespace {
void set_error(std::string* error, std::string message)
{
    if (error)
        *error = std::move(message);
}

fs::path make_temp_path(const fs::path& target)
{
    std::random_device random;
    return fs::path(temp_prefix(target) + fmt::format("{:08x}{:08x}", random(), random()));
}

#ifndef _WIN32
std::string errno_text(int err)
{
    return std::strerror(err);
}

bool write_all(int fd, std::string_view content)
{
    const char* data = content.data();
    std::size_t left = content.size();
    while (left > 0) {
        const ssize_t written = ::write(fd, data, left);
        if (written < 0) {
            if (errno == EINTR)
                continue;
            return false;
        }
        data += written;
        left -= static_cast<std::size_t>(written);
    }
    return true;
}

/// A rename is only durable once the directory entry is on disk too.
bool sync_directory(const fs::path& dir, std::string* error)
{
    const int fd = ::open(dir.empty() ? "." : dir.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd < 0) {
        set_error(error, fmt::format("could not open directory '{}': {}", dir.string(), errno_text(errno)));
        return false;
    }
    const bool ok = ::fsync(fd) == 0;
    const int err = errno;
    ::close(fd);
    if (!ok)
        set_error(error, fmt::format("could not sync directory '{}': {}", dir.string(), errno_text(err)));
    return ok;
}
#endif
}  // namespace

std::string temp_prefix(const fs::path& target)
{
    return target.string() + ".tmp-";
}

bool replace(const fs::path& target, std::string_view content, std::string* error)
{
    const auto temp = make_temp_path(target);
#ifndef _WIN32
    struct stat existing{};
    const bool has_existing = ::stat(target.c_str(), &existing) == 0;
    const mode_t mode = has_existing ? (existing.st_mode & 07777) : 0644;

    const int fd = ::open(temp.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (fd < 0) {
        set_error(error, fmt::format("could not create '{}': {}", temp.string(), errno_text(errno)));
        return false;
    }
    auto fail = [&](const std::string& what) {
        const int err = errno;
        ::close(fd);
        std::error_code ignored;
        fs::remove(temp, ignored);
        set_error(error, fmt::format("could not {} '{}': {}", what, temp.string(), errno_text(err)));
        return false;
    };
    if (!write_all(fd, content))
        return fail("write");
    // Keep the owner of a file the daemon wrote before it dropped privileges.
    if (has_existing && ::geteuid() == 0 && ::fchown(fd, existing.st_uid, existing.st_gid) != 0)
        return fail("set owner of");
    if (::fchmod(fd, mode) != 0)
        return fail("set permissions of");
    if (::fsync(fd) != 0)
        return fail("sync");
    if (::close(fd) != 0) {
        const int err = errno;
        std::error_code ignored;
        fs::remove(temp, ignored);
        set_error(error, fmt::format("could not close '{}': {}", temp.string(), errno_text(err)));
        return false;
    }
#else
    {
        std::ofstream file(temp, std::ios::binary | std::ios::trunc);
        if (!file || !file.write(content.data(), static_cast<std::streamsize>(content.size())) || !file.flush()) {
            std::error_code ignored;
            fs::remove(temp, ignored);
            set_error(error, fmt::format("could not write '{}'", temp.string()));
            return false;
        }
    }
#endif
    std::error_code ec;
    fs::rename(temp, target, ec);
    if (ec) {
        std::error_code ignored;
        fs::remove(temp, ignored);
        set_error(error, fmt::format("could not replace '{}': {}", target.string(), ec.message()));
        return false;
    }
#ifndef _WIN32
    // The new content is in place for readers either way; report a failed
    // directory sync because the rename may not survive a power cut.
    return sync_directory(target.parent_path(), error);
#else
    return true;
#endif
}

int remove_stale_temps(const fs::path& target, std::chrono::seconds min_age)
{
    const auto dir = target.parent_path().empty() ? fs::path(".") : target.parent_path();
    const auto prefix = fs::path(temp_prefix(target)).filename().string();
    const auto now = fs::file_time_type::clock::now();
    int removed = 0;
    std::error_code ec;
    for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
        const auto name = it->path().filename().string();
        if (!name.starts_with(prefix))
            continue;
        std::error_code entry_ec;
        if (!it->is_regular_file(entry_ec) || entry_ec)
            continue;
        const auto written = fs::last_write_time(it->path(), entry_ec);
        if (entry_ec || now - written < min_age)
            continue;
        if (fs::remove(it->path(), entry_ec) && !entry_ec)
            ++removed;
    }
    return removed;
}
}  // namespace DurableFile
