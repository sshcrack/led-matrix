#include "utils.h"
#include "shared.h"
#include <sys/time.h>
#include <thread>
#include <regex>
#include <spdlog/spdlog.h>

using namespace spdlog;

tmillis_t GetTimeInMillis() {
    struct timeval tp{};

    gettimeofday(&tp, nullptr);
    return tp.tv_sec * 1000 + tp.tv_usec / 1000;
}

void SleepMillis(tmillis_t milli_seconds) {
    if (milli_seconds <= 0) return;
    tmillis_t end_time = GetTimeInMillis() + milli_seconds;

    while (GetTimeInMillis() < end_time) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (skip_image || exit_canvas_update) {
            skip_image.store(false);
            break;
        }
    }
}

bool try_remove(const filesystem::path& file_path) {
    if (!filesystem::exists(file_path))
        return true;

    try {
        debug("Removing {}", file_path.string());
        auto res = filesystem::remove(file_path);
        debug("Done removing.");

        return res;
    } catch (exception &ex) {
        warn("Could not delete file {}", file_path.string());
        return false;
    }
}

bool is_valid_filename(const string& filename) {
    regex file_regex("^[\\w\\-. ]+$");
    std::smatch sm;
    regex_match(filename, sm, file_regex);

    if(sm.empty())
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