#pragma once

#include "shared/desktop/plugin/main.h"
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <map>
#include <cstdint>
#include <thread>
#include <deque>
#include <condition_variable>
#include <atomic>
#include "shared/desktop/utils.h"

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

const int MAX_WORKERS = std::thread::hardware_concurrency();

class ScriptedScenesPacket : public UdpPacket
{
public:
    std::vector<uint8_t> frameData;

    explicit ScriptedScenesPacket(const std::vector<uint8_t> &data) : UdpPacket(0x04), frameData(data)
    {
    }

    [[nodiscard]] std::vector<uint8_t> toData() const override
    {
        return frameData;
    }
};

class ScriptedScenesDesktop : public Plugins::DesktopPlugin
{
public:
    ScriptedScenesDesktop();
    ~ScriptedScenesDesktop() override;

    void render() override;
    std::string get_plugin_name() const override { return "ScriptedScenes"; }

    void on_websocket_message(const std::string message) override;

    std::optional<std::unique_ptr<UdpPacket, void (*)(UdpPacket *)>>
    compute_next_packet(const std::string sceneName) override;

    void initialize_imgui(ImGuiContext *im_gui_context, ImGuiMemAllocFunc *alloc_fn, ImGuiMemFreeFunc *free_fn,
                          void **user_data) override;

private:
    struct RenderConfig
    {
        int matrix_width = 128;
        int matrix_height = 128;
        int script_width = 128;
        int script_height = 128;
        std::vector<int> upscale_x_map;
        std::vector<int> upscale_y_map;
    };

    struct FrameJob
    {
        uint64_t frame_index = 0;
        double t = 0.0;
        double dt = 0.0;
    };

    struct FrameResult
    {
        uint64_t frame_index = 0;
        std::vector<uint8_t> frame_data;
        uint64_t set_pixel_calls = 0;
        uint64_t clear_calls = 0;
        double render_ms = 0.0;
        double total_ms = 0.0;
    };

    struct WorkerState
    {
        int id = 0;
        std::thread thread;
        std::unique_ptr<sol::state> lua;
        RenderConfig render_config;
        std::vector<uint8_t> script_canvas_data;
        std::map<std::string, sol::object> default_properties;
        std::string scene_name;
        bool unsafe_mode = false;
        bool lua_loaded = false;
        uint64_t set_pixel_calls = 0;
        uint64_t clear_calls = 0;

        std::atomic<bool> stop_requested{false};
        std::atomic<bool> is_finished{false};
    };

    std::unique_ptr<sol::state> lua_;
    std::string script_content_;
    std::string current_scene_name_;
    bool is_lua_loaded_ = false;
    bool offload_render_ = true;
    bool deterministic_parallel_ = false;
    bool bypass_protected_calls_ = false;
    bool use_parallel_pipeline_ = false;

    // Desktop canvas buffer
    std::vector<uint8_t> canvas_data_;
    std::vector<uint8_t> script_canvas_data_;
    std::vector<int> upscale_x_map_;
    std::vector<int> upscale_y_map_;
    int matrix_width_ = 128;
    int matrix_height_ = 128;
    int script_width_ = 128;
    int script_height_ = 128;
    int render_downscale_ = 1;

    std::map<std::string, sol::object> default_properties_;

    // Time tracking
    double start_time_ = 0.0;
    double last_time_ = 0.0;

    // FPS tracking
    int frame_count_ = 0;
    double last_fps_update_ = 0.0;
    float current_fps_ = 0.0f;

    // Lightweight performance profiling (updated once per second)
    double profile_window_start_ = 0.0;
    uint64_t profile_frames_ = 0;
    uint64_t profile_set_pixel_calls_ = 0;
    uint64_t profile_clear_calls_ = 0;
    double profile_globals_ms_sum_ = 0.0;
    double profile_lua_render_ms_sum_ = 0.0;
    double profile_packet_ms_sum_ = 0.0;
    double profile_total_ms_sum_ = 0.0;

    float profile_avg_globals_ms_ = 0.0f;
    float profile_avg_lua_render_ms_ = 0.0f;
    float profile_avg_packet_ms_ = 0.0f;
    float profile_avg_total_ms_ = 0.0f;
    float profile_avg_set_pixel_calls_per_frame_ = 0.0f;
    float profile_avg_clear_calls_per_frame_ = 0.0f;

    std::mutex script_mutex_;
    std::mutex pipeline_mutex_;
    std::condition_variable pipeline_cv_;

    std::vector<std::unique_ptr<WorkerState>> workers_;
    std::deque<FrameJob> frame_jobs_;
    std::map<uint64_t, FrameResult> completed_frames_;
    RenderConfig pipeline_render_config_;
    bool stop_workers_ = false;
    bool workers_started_ = false;
    uint64_t next_schedule_frame_index_ = 0;
    uint64_t next_send_frame_index_ = 0;
    double pipeline_start_time_ = 0.0;
    double missing_frame_since_ = 0.0;

    int pipeline_worker_count_ = MAX_WORKERS;
    int pipeline_lookahead_depth_ = 12;
    int pipeline_max_queued_frames_ = 32;
    float pipeline_max_reorder_wait_ms_ = 24.0f;
    float pipeline_target_fps_ = 60.0f;

    uint64_t pipeline_frames_sent_ = 0;
    uint64_t pipeline_frames_dropped_ = 0;
    float pipeline_queue_depth_ = 0.0f;
    float pipeline_completed_depth_ = 0.0f;
    float pipeline_reorder_wait_ms_ = 0.0f;
    float pipeline_effective_send_fps_ = 0.0f;
    float pipeline_avg_worker_render_ms_ = 0.0f;
    float pipeline_avg_worker_total_ms_ = 0.0f;
    double pipeline_send_window_start_ = 0.0;
    uint64_t pipeline_send_window_frames_ = 0;
    double pipeline_worker_render_ms_sum_ = 0.0;
    double pipeline_worker_total_ms_sum_ = 0.0;
    uint64_t pipeline_worker_samples_ = 0;

    // Adaptive pipeline tuning
    bool adaptive_pipeline_ = true;
    uint64_t adaptive_last_drop_count_ = 0;
    int adaptive_stable_seconds_ = 0;
    int adaptive_drop_bursts_ = 0;
    uint64_t adaptive_drops_last_window_ = 0;
    float adaptive_fps_ratio_last_ = 1.0f;
    std::string adaptive_last_action_ = "none";

    void maybe_adapt_pipeline_locked();
    void update_script_dimensions_locked();
    void blit_script_canvas_to_output_locked();
    static void blit_script_canvas_to_output(const std::vector<uint8_t> &script_canvas,
                                             std::vector<uint8_t> &output_canvas,
                                             const RenderConfig &config);
    void setup_lua_state();
    bool load_and_exec_script(const std::string &script_content);
    void reset_profiling_locked(double now);

    // Dynamic thread lifecycle
    void stop_pipeline_workers_locked();
    bool start_pipeline_workers_locked();
    void sync_pipeline_workers_locked();
    bool init_worker(WorkerState *worker_ctx);

    void maybe_update_pipeline_mode_locked();
    void schedule_pipeline_jobs_locked();
    std::optional<FrameResult> try_take_next_pipeline_frame_locked(double now);
    void worker_loop(WorkerState *worker);
};