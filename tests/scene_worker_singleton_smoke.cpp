#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <csignal>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {

#ifdef _WIN32
struct Child {
    PROCESS_INFORMATION process{};
};

Child spawn_worker(const std::string& worker, const std::string& plugin_dir, const std::string& port)
{
    std::string command = "\"" + worker + "\" --host 127.0.0.1 --port " + port + " --plugin-dir \"" + plugin_dir + "\"";
    std::vector<char> command_line(command.begin(), command.end());
    command_line.push_back('\0');
    STARTUPINFOA startup{};
    startup.cb = sizeof(startup);
    Child child;
    if (!CreateProcessA(nullptr, command_line.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                        nullptr, nullptr, &startup, &child.process)) {
        std::cerr << "CreateProcess failed: " << GetLastError() << '\n';
    }
    return child;
}

bool running(const Child& child)
{
    return child.process.hProcess
        && WaitForSingleObject(child.process.hProcess, 0) == WAIT_TIMEOUT;
}

void stop(Child& child)
{
    if (!child.process.hProcess)
        return;
    if (running(child))
        TerminateProcess(child.process.hProcess, 0);
    WaitForSingleObject(child.process.hProcess, 3000);
    CloseHandle(child.process.hProcess);
    CloseHandle(child.process.hThread);
    child.process = {};
}
#else
struct Child {
    pid_t pid = -1;
};

Child spawn_worker(const std::string& worker, const std::string& plugin_dir, const std::string& port)
{
    const pid_t pid = fork();
    if (pid == 0) {
        execl(worker.c_str(), worker.c_str(), "--host", "127.0.0.1", "--port", port.c_str(),
              "--plugin-dir", plugin_dir.c_str(), static_cast<char*>(nullptr));
        _exit(127);
    }
    return {pid};
}

bool running(const Child& child)
{
    if (child.pid <= 0)
        return false;
    int status = 0;
    const auto result = waitpid(child.pid, &status, WNOHANG);
    return result == 0;
}

void stop(Child& child)
{
    if (child.pid <= 0)
        return;
    if (running(child))
        kill(child.pid, SIGTERM);
    int status = 0;
    waitpid(child.pid, &status, 0);
    child.pid = -1;
}
#endif

} // namespace

int main(int argc, char** argv)
{
    if (argc != 3) {
        std::cerr << "usage: scene_worker_singleton_smoke <worker> <plugin-dir>\n";
        return 2;
    }

    Child first = spawn_worker(argv[1], argv[2], "6553");
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    if (!running(first)) {
        std::cerr << "first scene worker did not stay alive\n";
        stop(first);
        return 3;
    }

    Child second = spawn_worker(argv[1], argv[2], "6553");
    std::this_thread::sleep_for(std::chrono::milliseconds(700));
    const bool second_running = running(second);

    Child different_target = spawn_worker(argv[1], argv[2], "6554");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    const bool different_target_running = running(different_target);

    stop(different_target);
    stop(second);
    stop(first);

    if (second_running) {
        std::cerr << "two scene workers for the same matrix endpoint were alive at the same time\n";
        return 4;
    }
    if (!different_target_running) {
        std::cerr << "singleton guard incorrectly blocked a different matrix endpoint\n";
        return 5;
    }

    std::cout << "duplicate scene worker launch is rejected per matrix endpoint\n";
    return 0;
}
