#include "shared/desktop/utils.h"
#include <fstream>
#include <thread>
#include <chrono>
#include <vector>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;
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
        execl("/bin/sh", "sh", "-c", cmd.c_str(), nullptr);
        _exit(127);
    }
    while (true) {
        int status = 0;
        pid_t ret = waitpid(pid, &status, WNOHANG);
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