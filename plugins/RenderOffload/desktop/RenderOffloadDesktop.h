#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>

#include <shared/desktop/plugin/main.h>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <sys/types.h>
#endif

class RenderOffloadDesktop final : public Plugins::DesktopPlugin {
public:
    ~RenderOffloadDesktop() override;

    void render() override;
    void initialize_imgui(ImGuiContext *context, ImGuiMemAllocFunc *alloc_fn,
                          ImGuiMemFreeFunc *free_fn, void **user_data) override;
    std::string get_plugin_name() const override { return PLUGIN_NAME; }
    void on_websocket_message(std::string message) override;
    void post_init() override;
    void pre_new_frame() override;
    void before_exit() override;

private:
    using Clock = std::chrono::steady_clock;

    bool worker_alive();
    void ensure_worker();
    bool start_worker(const std::string &host, std::uint16_t port);
    void stop_worker();

    std::mutex mutex_;
    std::uint32_t requested_session_ = 0;
    std::string requested_scene_;
    bool worker_running_ = false;
    std::string worker_error_;
    std::string worker_host_;
    std::uint16_t worker_port_ = 0;
    Clock::time_point last_launch_attempt_{};

#ifdef _WIN32
    PROCESS_INFORMATION worker_process_{};
#else
    pid_t worker_pid_ = -1;
#endif
};
