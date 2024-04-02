#include "shared/utils/utils.h"
#include "shared/utils/shared.h"
#include <iostream>
#include <expected>
#include <thread>
#include <regex>
#include <spdlog/spdlog.h>
#include <random>

using namespace spdlog;

tmillis_t GetTimeInMillis() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
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

bool try_remove(const filesystem::path &file_path) {
    if (!filesystem::exists(file_path))
        return true;

    try {
        debug("Removing {}", file_path.string());
        return filesystem::remove(file_path);
    } catch (exception &ex) {
        warn("Could not delete file {}", file_path.string());
        return false;
    }
}

bool is_valid_filename(const string &filename) {
    regex file_regex("^[\\w\\-. ]+$");
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

std::expected<std::string, std::string> execute_process(const string &cmd, const vector<std::string> &args) {
    std::stringstream fullCmd;
    fullCmd << cmd;

    for (const auto &arg: args) {
        fullCmd << " " << arg;
    }

    char out_file[] = "/tmp/spotify.XXXXXX";
    int temp_fd = mkstemp(out_file);

    if (temp_fd == -1) {
        return unexpected("Could not create temporary file");
    }

    // Redirect output of the command appropriately
    std::ostringstream ss;
    ss << fullCmd.str() << " >" << out_file;
    const auto finalCmd = ss.str();

    // Call the command
    const auto status = std::system(finalCmd.c_str());

    if (status != 0) {
        cout << "Status " << status << " \n";
        return unexpected("Command failed");
    }

    // Read the output from the files and remove them
    std::stringstream out_stream;

    std::ifstream file(out_file);
    if (file) {
        out_stream << file.rdbuf();
        file.close();
    }

    close(temp_fd);
    std::remove(out_file);

    return out_stream.str();
}

void floatPixelSet(rgb_matrix::FrameCanvas *canvas, int x, int y, float r, float g, float b) {
    const uint8_t rByte = static_cast<uint8_t>(max(0.0f, min(1.0f, r)) * 255.0f);
    const uint8_t gByte = static_cast<uint8_t>(max(0.0f, min(1.0f, g)) * 255.0f);
    const uint8_t bByte = static_cast<uint8_t>(max(0.0f, min(1.0f, b)) * 255.0f);

    canvas->SetPixel(x, y, rByte, gByte, bByte);
}

std::string stringify_url(const string &url) {
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

std::optional<std::string> get_exec_dir() {
    char result[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    const char *path;

    if (count == -1)
        return nullopt;

    path = result;
    std::filesystem::path full_path(path);

    return full_path.parent_path().string();
}
