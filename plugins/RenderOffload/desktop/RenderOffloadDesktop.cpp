#include "RenderOffloadDesktop.h"

#include <filesystem>
#include <vector>
#include <thread>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <shared/common/utils/utils.h>
#include <shared/desktop/config.h>
#include <shared/desktop/utils.h>

#ifndef _WIN32
#include <cerrno>
#include <csignal>
#include <sys/wait.h>
#include <unistd.h>
#endif

REGISTER_PLUGIN(RenderOffload, RenderOffloadDesktop)

namespace fs = std::filesystem;

namespace {
fs::path worker_executable()
{
#ifdef _WIN32
    constexpr const char *name = "led-matrix-scene-worker.exe";
#else
    constexpr const char *name = "led-matrix-scene-worker";
#endif
    const auto local = get_exec_dir() / name;
    if (fs::is_regular_file(local)) return local;
    const auto sibling = get_exec_dir().parent_path() / "bin" / name;
    if (fs::is_regular_file(sibling)) return sibling;
    return local;
}
} // namespace

RenderOffloadDesktop::~RenderOffloadDesktop()
{
    stop_worker();
}

void RenderOffloadDesktop::initialize_imgui(
    ImGuiContext *context, ImGuiMemAllocFunc *alloc_fn,
    ImGuiMemFreeFunc *free_fn, void **user_data)
{
    ImGui::SetCurrentContext(context);
    ImGui::GetAllocatorFunctions(alloc_fn, free_fn, user_data);
}

void RenderOffloadDesktop::post_init()
{
    ensure_worker();
}

void RenderOffloadDesktop::pre_new_frame()
{
    ensure_worker();
}

void RenderOffloadDesktop::before_exit()
{
    stop_worker();
}

void RenderOffloadDesktop::render()
{
    std::lock_guard lock(mutex_);
    ImGui::TextUnformatted("Automatic render offload");
    ImGui::Text("Scene worker: %s", worker_running_ ? "running" : "stopped");
    if (!worker_host_.empty())
        ImGui::Text("Matrix: %s:%u", worker_host_.c_str(), worker_port_);
    if (requested_session_ != 0) {
        ImGui::Text("Requested scene: %s", requested_scene_.c_str());
        ImGui::Text("Session: %u", requested_session_);
    } else {
        ImGui::TextUnformatted("Requested scene: none");
    }
    if (!worker_error_.empty())
        ImGui::TextWrapped("Worker: %s", worker_error_.c_str());
    ImGui::TextWrapped(
        "The worker runs the same plugin scene code as the Pi. New portable scenes are automatically eligible for offload; no desktop renderer implementation is required.");
}

void RenderOffloadDesktop::on_websocket_message(std::string message)
{
    try {
        const auto command = nlohmann::json::parse(message);
        const auto op = command.value("op", std::string{});
        const auto session = command.value("session", 0U);
        std::lock_guard lock(mutex_);
        if (op == "start" && session != 0) {
            requested_session_ = session;
            requested_scene_ = command.value("scene", std::string{});
        } else if (op == "stop" && session == requested_session_) {
            requested_session_ = 0;
            requested_scene_.clear();
        }
    } catch (...) {
        // Worker heartbeats and malformed commands are not UI state.
    }
}

bool RenderOffloadDesktop::worker_alive()
{
#ifdef _WIN32
    if (!worker_process_.hProcess) return false;
    DWORD code = 0;
    const bool got_exit_code = GetExitCodeProcess(worker_process_.hProcess, &code) != FALSE;
    if (!got_exit_code || code != STILL_ACTIVE) {
        if (!got_exit_code)
            spdlog::warn("Failed to query scene worker exit code (Windows error {})", GetLastError());
        else
            spdlog::warn("Scene worker exited unexpectedly with code {}", code);
        CloseHandle(worker_process_.hProcess);
        CloseHandle(worker_process_.hThread);
        worker_process_ = {};
        return false;
    }
    return true;
#else
    if (worker_pid_ <= 0) return false;
    int status = 0;
    const pid_t result = waitpid(worker_pid_, &status, WNOHANG);
    if (result == 0) return true;
    if (result == worker_pid_) {
        if (WIFEXITED(status))
            spdlog::warn("Scene worker exited unexpectedly with code {}", WEXITSTATUS(status));
        else if (WIFSIGNALED(status))
            spdlog::warn("Scene worker exited unexpectedly from signal {}", WTERMSIG(status));
        else
            spdlog::warn("Scene worker exited unexpectedly");
        worker_pid_ = -1;
        return false;
    }
    if (result < 0 && errno == ECHILD) {
        worker_pid_ = -1;
        return false;
    }
    return true;
#endif
}

void RenderOffloadDesktop::ensure_worker()
{
    const auto &general = Config::ConfigManager::instance()->getGeneralConfig();
    const auto host = general.getHostnameCopy();
    const auto port = general.getPort();

    bool alive = worker_alive();
    if (alive && (host != worker_host_ || port != worker_port_)) {
        stop_worker();
        alive = false;
    }

    {
        std::lock_guard lock(mutex_);
        worker_running_ = alive;
    }
    if (host.empty() || alive)
        return;

    const auto now = Clock::now();
    if (last_launch_attempt_ != Clock::time_point{}
        && now - last_launch_attempt_ < std::chrono::seconds(2))
        return;
    last_launch_attempt_ = now;
    start_worker(host, port);
}

bool RenderOffloadDesktop::start_worker(const std::string &host, std::uint16_t port)
{
    const auto executable = worker_executable();
    const auto crash_dir = get_data_dir() / "crashes";
    if (!fs::is_regular_file(executable)) {
        std::lock_guard lock(mutex_);
        worker_error_ = "led-matrix-scene-worker is missing from the desktop installation";
        worker_running_ = false;
        return false;
    }

#ifdef _WIN32
    std::string command = "\"" + executable.string() + "\" --host \"" + host
        + "\" --port " + std::to_string(port) + " --crash-dir \""
        + crash_dir.string() + "\"";
    std::vector<char> cmdline(command.begin(), command.end());
    cmdline.push_back('\0');
    STARTUPINFOA startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION process{};
    if (!CreateProcessA(nullptr, cmdline.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) {
        std::lock_guard lock(mutex_);
        worker_error_ = "failed to start scene worker (Windows error " + std::to_string(GetLastError()) + ")";
        worker_running_ = false;
        return false;
    }
    worker_process_ = process;
#else
    const pid_t pid = fork();
    if (pid < 0) {
        std::lock_guard lock(mutex_);
        worker_error_ = "failed to fork scene worker";
        worker_running_ = false;
        return false;
    }
    if (pid == 0) {
        const auto port_string = std::to_string(port);
        execl(executable.c_str(), executable.c_str(), "--host", host.c_str(),
              "--port", port_string.c_str(), "--crash-dir", crash_dir.c_str(),
              static_cast<char *>(nullptr));
        _exit(127);
    }
    worker_pid_ = pid;
#endif

    worker_host_ = host;
    worker_port_ = port;
    {
        std::lock_guard lock(mutex_);
        worker_error_.clear();
        worker_running_ = true;
    }
    spdlog::info("Started generic scene worker for {}:{}", host, port);
    return true;
}

void RenderOffloadDesktop::stop_worker()
{
#ifdef _WIN32
    if (worker_process_.hProcess) {
        if (WaitForSingleObject(worker_process_.hProcess, 0) == WAIT_TIMEOUT) {
            TerminateProcess(worker_process_.hProcess, 0);
            WaitForSingleObject(worker_process_.hProcess, 2000);
        }
        CloseHandle(worker_process_.hProcess);
        CloseHandle(worker_process_.hThread);
        worker_process_ = {};
    }
#else
    if (worker_pid_ > 0) {
        kill(worker_pid_, SIGTERM);
        int status = 0;
        for (int i = 0; i < 10; ++i) {
            if (waitpid(worker_pid_, &status, WNOHANG) == worker_pid_) {
                worker_pid_ = -1;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        if (worker_pid_ > 0) {
            kill(worker_pid_, SIGKILL);
            waitpid(worker_pid_, &status, 0);
            worker_pid_ = -1;
        }
    }
#endif
    std::lock_guard lock(mutex_);
    worker_running_ = false;
}
