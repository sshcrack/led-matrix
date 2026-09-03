#include "shared/desktop/utils.h"
#include "shared/desktop/config.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <fstream>
#include <thread>
#include <chrono>
#include <vector>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;
namespace {
void append_output_tail(std::string& output, const char* data, std::size_t size,
                        std::size_t max_output_bytes)
{
    if (max_output_bytes == 0 || size == 0)
        return;
    if (size >= max_output_bytes) {
        output.assign(data + size - max_output_bytes, max_output_bytes);
        return;
    }
    if (output.size() + size > max_output_bytes)
        output.erase(0, output.size() + size - max_output_bytes);
    output.append(data, size);
}
}

bool isWritableExistingFile(const std::filesystem::path &path)
{
    if (!std::filesystem::exists(path))
        return false;

    std::ofstream ofs(path, std::ios::in | std::ios::out); // read/write mode
    return ofs.is_open();
}

fs::path get_data_dir()
{
    fs::path configPath;
    const fs::path exeDir = get_exec_dir();

    const fs::path portableFile = exeDir / "portable.txt";
    bool isPortable = fs::exists(portableFile);

    auto localDataDir = exeDir.parent_path() / "data";
    if (isPortable)
        return localDataDir;

    const fs::path dataConfigJson = localDataDir / "config.json";
    if (isWritableExistingFile(dataConfigJson))
        return localDataDir;

#ifdef _WIN32
    char *appData = nullptr;
    size_t sz = 0;
    _dupenv_s(&appData, &sz, "APPDATA");
    if (!appData)
        throw std::runtime_error("Failed to get APPDATA environment variable");

    fs::path configRootPath = fs::path(appData) / "led-matrix-desktop";
    if (!fs::exists(configRootPath))
        fs::create_directories(configRootPath);

    free(appData);
    return configRootPath;
#else
    const char *home = getenv("HOME");
    if (!home)
        throw std::runtime_error("Failed to get HOME environment variable");

    fs::path configRootPath = fs::path(home) / ".config" / "led-matrix-desktop";
    if (!fs::exists(configRootPath))
        fs::create_directories(configRootPath);

    return configRootPath;
#endif
}

int run_command(const std::string& cmd,
                const std::atomic<bool>* running) {
#ifdef _WIN32
    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    std::string fullCmd = "cmd.exe /C " + cmd;
    std::vector<char> cmdline(fullCmd.begin(), fullCmd.end());
    cmdline.push_back('\0');
    if (!CreateProcessA(nullptr, cmdline.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
        return -1;
    while (true) {
        DWORD waitResult = WaitForSingleObject(pi.hProcess, 200);
        if (waitResult == WAIT_OBJECT_0) break;
        if (running && !running->load()) {
            TerminateProcess(pi.hProcess, 1);
            WaitForSingleObject(pi.hProcess, INFINITE);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            return -2;
        }
    }
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return static_cast<int>(exitCode);
#else
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) { dup2(devnull, STDOUT_FILENO); dup2(devnull, STDERR_FILENO); close(devnull); }
        execl("/bin/sh", "sh", "-c", cmd.c_str(), (char*)nullptr);
        _exit(127);
    }
    while (true) {
        int status = 0;
        pid_t ret;
        do {
            ret = waitpid(pid, &status, WNOHANG);
        } while (ret < 0 && errno == EINTR);
        if (ret < 0) {
            return -1;
        }
        if (ret == pid) {
            return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
        }
        if (running && !running->load()) {
            kill(pid, SIGTERM);
            for (int i = 0; i < 5; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                if (waitpid(pid, &status, WNOHANG) == pid) goto done;
            }
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
            done:
            return -2;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
#endif
}

CommandResult run_command_capture(const std::string& cmd,
                                  const std::atomic<bool>* running,
                                  std::size_t max_output_bytes) {
    CommandResult result;
#ifdef _WIN32
    HANDLE hRead = nullptr, hWrite = nullptr;
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0))
        return result;
    if (!SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(hRead);
        CloseHandle(hWrite);
        return result;
    }

    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    std::string fullCmd = "cmd.exe /C " + cmd;
    std::vector<char> cmdline(fullCmd.begin(), fullCmd.end());
    cmdline.push_back('\0');
    if (!CreateProcessA(nullptr, cmdline.data(), nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(hRead);
        CloseHandle(hWrite);
        return result;
    }
    CloseHandle(hWrite);

    auto drain = [&]() {
        std::array<char, 4096> buffer{};
        while (true) {
            DWORD available = 0;
            if (!PeekNamedPipe(hRead, nullptr, 0, nullptr, &available, nullptr) || available == 0)
                break;
            DWORD bytes_read = 0;
            const DWORD wanted = std::min<DWORD>(available, static_cast<DWORD>(buffer.size()));
            if (!ReadFile(hRead, buffer.data(), wanted, &bytes_read, nullptr) || bytes_read == 0)
                break;
            append_output_tail(result.output, buffer.data(), bytes_read, max_output_bytes);
        }
    };

    bool cancelled = false;
    while (WaitForSingleObject(pi.hProcess, 50) == WAIT_TIMEOUT) {
        drain();
        if (running && !running->load()) {
            cancelled = true;
            TerminateProcess(pi.hProcess, 1);
            WaitForSingleObject(pi.hProcess, INFINITE);
            break;
        }
    }
    drain();
    DWORD exit_code = 1;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(hRead);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    result.exit_code = cancelled ? -2 : static_cast<int>(exit_code);
#else
    int fds[2];
    if (pipe(fds) != 0)
        return result;
    pid_t pid = fork();
    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        return result;
    }
    if (pid == 0) {
        close(fds[0]);
        dup2(fds[1], STDOUT_FILENO);
        dup2(fds[1], STDERR_FILENO);
        close(fds[1]);
        execl("/bin/sh", "sh", "-c", cmd.c_str(), (char*)nullptr);
        _exit(127);
    }
    close(fds[1]);
    const int flags = fcntl(fds[0], F_GETFL, 0);
    if (flags >= 0)
        fcntl(fds[0], F_SETFL, flags | O_NONBLOCK);

    auto drain = [&]() {
        std::array<char, 4096> buffer{};
        while (true) {
            const ssize_t bytes_read = read(fds[0], buffer.data(), buffer.size());
            if (bytes_read > 0) {
                append_output_tail(result.output, buffer.data(),
                                   static_cast<std::size_t>(bytes_read), max_output_bytes);
                continue;
            }
            if (bytes_read < 0 && errno == EINTR)
                continue;
            break;
        }
    };

    int status = 0;
    bool cancelled = false;
    while (true) {
        drain();
        pid_t ret;
        do {
            ret = waitpid(pid, &status, WNOHANG);
        } while (ret < 0 && errno == EINTR);
        if (ret == pid)
            break;
        if (ret < 0) {
            close(fds[0]);
            return result;
        }
        if (running && !running->load()) {
            cancelled = true;
            kill(pid, SIGTERM);
            for (int i = 0; i < 5; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                drain();
                if (waitpid(pid, &status, WNOHANG) == pid)
                    goto child_done;
            }
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
child_done:
    drain();
    close(fds[0]);
    result.exit_code = cancelled ? -2 : (WIFEXITED(status) ? WEXITSTATUS(status) : 1);
#endif
    return result;
}

std::string run_command_and_get_output(const std::string& cmd) {
    std::string result;
#ifdef _WIN32
    HANDLE hRead, hWrite;
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0))
        return {};
    if (!SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(hRead); CloseHandle(hWrite);
        return {};
    }
    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    std::string fullCmd = "cmd.exe /C " + cmd;
    std::vector<char> cmdline(fullCmd.begin(), fullCmd.end());
    cmdline.push_back('\0');
    if (!CreateProcessA(nullptr, cmdline.data(), nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(hRead); CloseHandle(hWrite);
        return {};
    }
    CloseHandle(hWrite);
    std::array<char, 4096> buf{};
    DWORD bytesRead;
    while (ReadFile(hRead, buf.data(), static_cast<DWORD>(buf.size() - 1), &bytesRead, nullptr)
           && bytesRead > 0) {
        buf[bytesRead] = '\0';
        result += buf.data();
    }
    CloseHandle(hRead);
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
#else
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return {};
    std::array<char, 4096> buf{};
    while (fgets(buf.data(), static_cast<int>(buf.size()), pipe))
        result += buf.data();
    pclose(pipe);
#endif
    return result;
}

namespace {
std::string quote_command_path(const std::string& path) {
#ifdef _WIN32
    std::string quoted = "\"";
    for (char c : path) {
        if (c == '\"') quoted += "\\\"";
        else quoted += c;
    }
    quoted += "\"";
    return quoted;
#else
    // POSIX shell single-quote escaping: ' becomes '\'' .
    std::string quoted = "'";
    for (char c : path) {
        if (c == '\'') quoted += "'\\''";
        else quoted += c;
    }
    quoted += "'";
    return quoted;
#endif
}

inline std::string trim_whitespace(std::string s) {
    s.erase(0, s.find_first_not_of(" \t\r\n"));
    s.erase(s.find_last_not_of(" \t\r\n") + 1);
    return s;
}
} // anonymous namespace

std::string get_ytdlp_command() {
    const auto path = Config::ConfigManager::instance()->getGeneralConfig().getYtDlpPath();
    return path.empty() ? "yt-dlp" : quote_command_path(path);
}

std::string get_ytdlp_network_command() {
    return get_ytdlp_command() + " -4";
}

std::string_view get_ytdlp_video_format_selector() {
    // SpotifyMV renders only pixels; it never uses the YouTube audio track.
    // Prefer a modest H.264 MP4 video-only stream (fast to download/decode for
    // a 128x128 target), then progressively relax codec/height constraints.
    // This remains usable when yt-dlp cannot expose legacy progressive A/V
    // format 18 because no optional JavaScript runtime is installed.
    return "bestvideo[vcodec^=avc1][height<=480][ext=mp4]/"
           "bestvideo[vcodec^=avc1][ext=mp4]/"
           "bestvideo[height<=480][ext=mp4]/"
           "bestvideo[ext=mp4]/bestvideo";
}

std::string check_ytdlp_available() {
    const auto configured = Config::ConfigManager::instance()->getGeneralConfig().getYtDlpPath();
    const std::string command = get_ytdlp_command();
#ifdef _WIN32
    const char* null_device = "nul";
#else
    const char* null_device = "/dev/null";
#endif
    if (run_command(command + " --version > " + null_device + " 2>&1") == 0)
        return {};

    if (configured.empty())
        return "yt-dlp was not found in PATH. Choose the yt-dlp binary in Desktop Settings.";
    return "yt-dlp could not be started from \"" + configured + "\".";
}

std::string check_video_tools_available() {
#ifdef _WIN32
    const char* null_device = "nul";
#else
    const char* null_device = "/dev/null";
#endif
    if (run_command(std::string("ffmpeg -version > ") + null_device + " 2>&1") != 0)
        return "ffmpeg not found in PATH.";
    return check_ytdlp_available();
}

std::string open_file_dialog(const std::string& title) {
#ifdef _WIN32
    char buffer[MAX_PATH] = {};
    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrTitle = title.c_str();
    ofn.lpstrFilter = "All Files (*.*)\0*.*\0";
    ofn.lpstrFile = buffer;
    ofn.nMaxFile = sizeof(buffer);
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameA(&ofn))
        return std::string(buffer);
    return "";
#else
    // Escape the title for the shell
    std::string quotedTitle = title;
    {
        std::size_t pos = 0;
        while ((pos = quotedTitle.find('"', pos)) != std::string::npos) {
            quotedTitle.replace(pos, 1, "\\\"");
            pos += 2;
        }
    }
    std::string zenity = "zenity --file-selection --title=\"" + quotedTitle + "\" 2>/dev/null";
    std::string result = trim_whitespace(run_command_and_get_output(zenity));
    if (!result.empty())
        return result;

    std::string kdialog = "kdialog --getopenfilename . --title=\"" + quotedTitle + "\" 2>/dev/null";
    result = trim_whitespace(run_command_and_get_output(kdialog));
    return result;
#endif
}