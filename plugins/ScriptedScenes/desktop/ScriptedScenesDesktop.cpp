#include "ScriptedScenesDesktop.h"

#include <spdlog/spdlog.h>
#include <chrono>
#include <algorithm>

extern "C" PLUGIN_EXPORT ScriptedScenesDesktop* createScriptedScenes()
{
    return new ScriptedScenesDesktop();
}

extern "C" PLUGIN_EXPORT void destroyScriptedScenes(ScriptedScenesDesktop* c)
{
    delete c;
}

static double get_time_sec()
{
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(now.time_since_epoch()).count();
}

static constexpr int MIN_RENDER_DOWNSCALE = 1;
static constexpr int MAX_RENDER_DOWNSCALE = 4;

// Adaptive tuning constants
static constexpr int   ADAPTIVE_SCALE_UP_BURST_THRESHOLD   = 2;
static constexpr int   ADAPTIVE_SCALE_DOWN_STABLE_SECONDS  = 10;
static constexpr int   ADAPTIVE_MIN_WORKERS                = 1;
static constexpr int   ADAPTIVE_LOOKAHEAD_STEP_UP          = 2;
static constexpr int   ADAPTIVE_LOOKAHEAD_MAX              = 30;
static constexpr int   ADAPTIVE_LOOKAHEAD_MIN              = 4;
static constexpr int   ADAPTIVE_QUEUE_STEP_UP              = 8;
static constexpr int   ADAPTIVE_QUEUE_MAX                  = 120;
static constexpr int   ADAPTIVE_QUEUE_MIN                  = 16;
static constexpr float ADAPTIVE_FPS_POOR_THRESHOLD         = 0.90f;
static constexpr float ADAPTIVE_FPS_GOOD_THRESHOLD         = 0.98f;
static constexpr float ADAPTIVE_REORDER_STEP_UP_MS         = 50.0f;
static constexpr float ADAPTIVE_REORDER_MAX_MS             = 2000.0f;
static constexpr float ADAPTIVE_REORDER_STEP_DOWN_MS       = 5.0f;
static constexpr float ADAPTIVE_REORDER_MIN_MS             = 34.0f;

ScriptedScenesDesktop::ScriptedScenesDesktop()
{
    canvas_data_.resize(matrix_width_ * matrix_height_ * 3, 0);
    update_script_dimensions_locked();
}

ScriptedScenesDesktop::~ScriptedScenesDesktop()
{
    std::lock_guard<std::mutex> lock(script_mutex_);
    stop_pipeline_workers_locked();
}

void ScriptedScenesDesktop::initialize_imgui(ImGuiContext* im_gui_context, ImGuiMemAllocFunc* alloc_fn,
                                             ImGuiMemFreeFunc* free_fn, void** user_data)
{
    ImGui::SetCurrentContext(im_gui_context);
    ImGui::GetAllocatorFunctions(alloc_fn, free_fn, user_data);
}

void ScriptedScenesDesktop::render()
{
    std::lock_guard<std::mutex> lock(script_mutex_);
    if (!is_lua_loaded_)
    {
        ImGui::Text("No Lua script loaded.");
        return;
    }

    ImGui::Text("Current Scene: %s", current_scene_name_.c_str());
    ImGui::Text("Offload Render: %s", offload_render_ ? "Yes" : "No");
    ImGui::Text("FPS: %.1f", current_fps_);

    bool should_restart_pipeline = false;

    if (ImGui::SliderInt("Render Downscale", &render_downscale_, MIN_RENDER_DOWNSCALE, MAX_RENDER_DOWNSCALE))
    {
        update_script_dimensions_locked();
        if (lua_)
        {
            (*lua_)["width"] = script_width_;
            (*lua_)["height"] = script_height_;
        }
        should_restart_pipeline = true;
    }

    ImGui::Text("Lua Resolution: %dx%d", script_width_, script_height_);

    if (ImGui::Checkbox("Unsafe Parallel Mode", &bypass_protected_calls_))
    {
        should_restart_pipeline = true;
    }

    const bool manual_control = !adaptive_pipeline_;

    if (!manual_control) ImGui::BeginDisabled();

    if (ImGui::SliderInt("Pipeline Workers", &pipeline_worker_count_, 1, MAX_WORKERS))
    {
        sync_pipeline_workers_locked(); // Seamless adjustment
    }

    ImGui::SliderInt("Pipeline Lookahead", &pipeline_lookahead_depth_, 1, 30);
    ImGui::SliderInt("Pipeline Max Queue", &pipeline_max_queued_frames_, 4, 120);
    ImGui::SliderFloat("Max Reorder Wait (ms)", &pipeline_max_reorder_wait_ms_, 0.0f, 2000.0f, "%.1f");

    if (!manual_control) ImGui::EndDisabled();

    ImGui::Text("Deterministic parallel: %s", deterministic_parallel_ ? "Yes" : "No");
    ImGui::Text("Parallel active: %s", use_parallel_pipeline_ ? "Yes" : "No");

    if (should_restart_pipeline)
    {
        stop_pipeline_workers_locked();
        maybe_update_pipeline_mode_locked();
    }

    ImGui::Separator();
    ImGui::Text("Lua Profiling (1s avg)");
    ImGui::Text("Compute total: %.3f ms", profile_avg_total_ms_);
    ImGui::Text("Lua render(): %.3f ms", profile_avg_lua_render_ms_);
    ImGui::Text("Global updates: %.3f ms", profile_avg_globals_ms_);
    ImGui::Text("Packet creation: %.3f ms", profile_avg_packet_ms_);
    ImGui::Text("set_pixel/frame: %.1f", profile_avg_set_pixel_calls_per_frame_);
    ImGui::Text("clear/frame: %.2f", profile_avg_clear_calls_per_frame_);

    if (use_parallel_pipeline_)
    {
        ImGui::Separator();
        ImGui::Text("Parallel Pipeline");
        ImGui::Text("Queue depth: %.0f", pipeline_queue_depth_);
        ImGui::Text("Completed depth: %.0f", pipeline_completed_depth_);
        ImGui::Text("Reorder wait: %.2f ms", pipeline_reorder_wait_ms_);
        ImGui::Text("Dropped frames: %llu", static_cast<unsigned long long>(pipeline_frames_dropped_));
        ImGui::Text("Sent frames: %llu", static_cast<unsigned long long>(pipeline_frames_sent_));
        ImGui::Text("Effective send FPS: %.1f", pipeline_effective_send_fps_);

        const double base_frame_budget_ms = 1000.0 / static_cast<double>(std::max(1.0f, pipeline_target_fps_));
        const double parallel_budget_ms = base_frame_budget_ms * pipeline_worker_count_ * 0.95;

        ImGui::Text("Worker render avg: %.3f ms", pipeline_avg_worker_render_ms_);
        ImGui::Text("Worker total avg: %.3f ms", pipeline_avg_worker_total_ms_);
        ImGui::Text("Parallel budget: %.2f ms", parallel_budget_ms);
    }

    ImGui::Separator();
    ImGui::Text("Adaptive Pipeline Tuning");

    if (ImGui::Checkbox("Enable Adaptive Mode", &adaptive_pipeline_))
    {
        adaptive_last_drop_count_  = pipeline_frames_dropped_;
        adaptive_stable_seconds_   = 0;
        adaptive_drop_bursts_      = 0;
        adaptive_drops_last_window_= 0;
        adaptive_fps_ratio_last_   = 1.0f;
        adaptive_last_action_      = "none";
    }

    if (adaptive_pipeline_)
    {
        ImGui::Text("FPS ratio (eff/target): %.2f", adaptive_fps_ratio_last_);
        ImGui::Text("Drops last window: %llu", static_cast<unsigned long long>(adaptive_drops_last_window_));
        ImGui::Text("Stable seconds: %d", adaptive_stable_seconds_);
        ImGui::Text("Drop burst count: %d", adaptive_drop_bursts_);
        ImGui::Text("Last action: %s", adaptive_last_action_.c_str());
    }
}

void ScriptedScenesDesktop::on_websocket_message(const std::string message)
{
    if (message.starts_with("size:"))
    {
        std::string sizeStr = message.substr(5);
        auto xPos = sizeStr.find('x');
        if (xPos != std::string::npos)
        {
            std::lock_guard<std::mutex> lock(script_mutex_);
            matrix_width_ = std::stoi(sizeStr.substr(0, xPos));
            matrix_height_ = std::stoi(sizeStr.substr(xPos + 1));
            canvas_data_.resize(matrix_width_ * matrix_height_ * 3, 0);
            update_script_dimensions_locked();

            if (lua_)
            {
                (*lua_)["width"] = script_width_;
                (*lua_)["height"] = script_height_;
            }

            stop_pipeline_workers_locked();
            maybe_update_pipeline_mode_locked();
        }
    }
    else if (message.starts_with("script:"))
    {
        std::string payload = message.substr(7);
        auto pos = payload.find(':');
        if (pos != std::string::npos)
        {
            std::string name = payload.substr(0, pos);
            std::string script_content = payload.substr(pos + 1);

            spdlog::info("[ScriptedScenesDesktop] Received script for '{}'", name);
            current_scene_name_ = name;

            std::lock_guard<std::mutex> lock(script_mutex_);
            default_properties_.clear();
            stop_pipeline_workers_locked();
            setup_lua_state();
            if (load_and_exec_script(script_content))
            {
                sol::protected_function setup_fn = (*lua_)["setup"];
                if (setup_fn.valid())
                {
                    auto result = setup_fn();
                    if (!result.valid())
                    {
                        sol::error err = result;
                        spdlog::error("[ScriptedScenesDesktop] setup() error: {}", err.what());
                    }
                }

                (*lua_)["width"] = script_width_;
                (*lua_)["height"] = script_height_;

                sol::protected_function init_fn = (*lua_)["initialize"];
                if (init_fn.valid())
                {
                    auto result = init_fn();
                    if (!result.valid())
                    {
                        sol::error err = result;
                        spdlog::error("[ScriptedScenesDesktop] initialize() error: {}", err.what());
                    }
                }

                reset_profiling_locked(get_time_sec());

                std::fill(canvas_data_.begin(), canvas_data_.end(), 0);
                std::fill(script_canvas_data_.begin(), script_canvas_data_.end(), 0);

                maybe_update_pipeline_mode_locked();
            }
        }
    }
}

void ScriptedScenesDesktop::update_script_dimensions_locked()
{
    render_downscale_ = std::clamp(render_downscale_, MIN_RENDER_DOWNSCALE, MAX_RENDER_DOWNSCALE);
    script_width_ = std::max(1, matrix_width_ / render_downscale_);
    script_height_ = std::max(1, matrix_height_ / render_downscale_);
    script_canvas_data_.assign(script_width_ * script_height_ * 3, 0);

    upscale_x_map_.resize(matrix_width_);
    for (int x = 0; x < matrix_width_; ++x)
    {
        upscale_x_map_[x] = std::min(script_width_ - 1, x / render_downscale_);
    }

    upscale_y_map_.resize(matrix_height_);
    for (int y = 0; y < matrix_height_; ++y)
    {
        upscale_y_map_[y] = std::min(script_height_ - 1, y / render_downscale_);
    }

    pipeline_render_config_.matrix_width = matrix_width_;
    pipeline_render_config_.matrix_height = matrix_height_;
    pipeline_render_config_.script_width = script_width_;
    pipeline_render_config_.script_height = script_height_;
    pipeline_render_config_.upscale_x_map = upscale_x_map_;
    pipeline_render_config_.upscale_y_map = upscale_y_map_;
}

void ScriptedScenesDesktop::blit_script_canvas_to_output(const std::vector<uint8_t>& script_canvas,
                                                         std::vector<uint8_t>& output_canvas,
                                                         const RenderConfig& config)
{
    output_canvas.resize(config.matrix_width * config.matrix_height * 3);

    if (config.script_width == config.matrix_width && config.script_height == config.matrix_height)
    {
        std::copy(script_canvas.begin(), script_canvas.end(), output_canvas.begin());
        return;
    }

    for (int y = 0; y < config.matrix_height; ++y)
    {
        const int src_y = config.upscale_y_map[y];
        for (int x = 0; x < config.matrix_width; ++x)
        {
            const int src_x = config.upscale_x_map[x];
            const int src_idx = (src_y * config.script_width + src_x) * 3;
            const int dst_idx = (y * config.matrix_width + x) * 3;
            output_canvas[dst_idx] = script_canvas[src_idx];
            output_canvas[dst_idx + 1] = script_canvas[src_idx + 1];
            output_canvas[dst_idx + 2] = script_canvas[src_idx + 2];
        }
    }
}

void ScriptedScenesDesktop::blit_script_canvas_to_output_locked()
{
    blit_script_canvas_to_output(script_canvas_data_, canvas_data_, pipeline_render_config_);
}

void ScriptedScenesDesktop::setup_lua_state()
{
    lua_ = std::make_unique<sol::state>();
    lua_->open_libraries(
        sol::lib::base,
        sol::lib::math,
        sol::lib::string,
        sol::lib::table
    );

    lua_->set_function("set_pixel", [this](int x, int y, int r, int g, int b)
    {
        profile_set_pixel_calls_++;
        if (x < 0 || x >= script_width_ || y < 0 || y >= script_height_) return;
        int idx = (y * script_width_ + x) * 3;
        script_canvas_data_[idx] = static_cast<uint8_t>(std::clamp(r, 0, 255));
        script_canvas_data_[idx + 1] = static_cast<uint8_t>(std::clamp(g, 0, 255));
        script_canvas_data_[idx + 2] = static_cast<uint8_t>(std::clamp(b, 0, 255));
    });

    lua_->set_function("clear", [this]()
    {
        profile_clear_calls_++;
        std::fill(script_canvas_data_.begin(), script_canvas_data_.end(), 0);
    });

    lua_->set_function("log", [this](const std::string& msg)
    {
        spdlog::info("[ScriptedScenesDesktop:{}] {}", current_scene_name_, msg);
    });

    lua_->set_function("define_property",[this](const std::string& name, const std::string& type, sol::object default_val,
                              sol::variadic_args va)
                       {
                           default_properties_[name] = default_val;
                       });

    lua_->set_function("get_property", [this](const std::string& name) -> sol::object
    {
        auto it = default_properties_.find(name);
        if (it != default_properties_.end())
        {
            return it->second;
        }
        return sol::nil;
    });

    (*lua_)["width"] = script_width_;
    (*lua_)["height"] = script_height_;
    (*lua_)["time"] = 0.0;
    (*lua_)["dt"] = 0.0;
}

bool ScriptedScenesDesktop::load_and_exec_script(const std::string& script_content)
{
    script_content_ = script_content;

    auto result = lua_->script(
        script_content,[](lua_State*, sol::protected_function_result pfr) { return pfr; }
    );

    if (!result.valid())
    {
        sol::error err = result;
        spdlog::error("[ScriptedScenesDesktop:{}] Script load error: {}", current_scene_name_, err.what());
        is_lua_loaded_ = false;
        return false;
    }

    sol::object offload_obj = (*lua_)["offload"];
    if (offload_obj.is<bool>())
    {
        offload_render_ = offload_obj.as<bool>();
    }
    else
    {
        offload_render_ = true;
    }

    render_downscale_ = 1;
    sol::object render_downscale_obj = (*lua_)["render_downscale"];
    if (render_downscale_obj.is<int>())
    {
        render_downscale_ = render_downscale_obj.as<int>();
    }
    else if (render_downscale_obj.is<double>())
    {
        render_downscale_ = static_cast<int>(render_downscale_obj.as<double>());
    }
    render_downscale_ = std::clamp(render_downscale_, MIN_RENDER_DOWNSCALE, MAX_RENDER_DOWNSCALE);
    update_script_dimensions_locked();
    (*lua_)["width"] = script_width_;
    (*lua_)["height"] = script_height_;

    sol::object deterministic_obj = (*lua_)["parallel_deterministic"];
    deterministic_parallel_ = !deterministic_obj.is<bool>() || deterministic_obj.as<bool>();

    is_lua_loaded_ = true;
    return true;
}

void ScriptedScenesDesktop::reset_profiling_locked(double now)
{
    start_time_ = now;
    last_time_ = now;
    last_fps_update_ = now;
    frame_count_ = 0;
    current_fps_ = 0.0f;
    profile_window_start_ = now;
    profile_frames_ = 0;
    profile_set_pixel_calls_ = 0;
    profile_clear_calls_ = 0;
    profile_globals_ms_sum_ = 0.0;
    profile_lua_render_ms_sum_ = 0.0;
    profile_packet_ms_sum_ = 0.0;
    profile_total_ms_sum_ = 0.0;
    profile_avg_globals_ms_ = 0.0f;
    profile_avg_lua_render_ms_ = 0.0f;
    profile_avg_packet_ms_ = 0.0f;
    profile_avg_total_ms_ = 0.0f;
    profile_avg_set_pixel_calls_per_frame_ = 0.0f;
    profile_avg_clear_calls_per_frame_ = 0.0f;

    pipeline_frames_sent_ = 0;
    pipeline_frames_dropped_ = 0;
    pipeline_queue_depth_ = 0.0f;
    pipeline_completed_depth_ = 0.0f;
    pipeline_reorder_wait_ms_ = 0.0f;
    pipeline_effective_send_fps_ = 0.0f;
    pipeline_avg_worker_render_ms_ = 0.0f;
    pipeline_avg_worker_total_ms_ = 0.0f;
    pipeline_worker_render_ms_sum_ = 0.0;
    pipeline_worker_total_ms_sum_ = 0.0;
    pipeline_worker_samples_ = 0;

    adaptive_last_drop_count_   = 0;
    adaptive_stable_seconds_    = 0;
    adaptive_drop_bursts_       = 0;
    adaptive_drops_last_window_ = 0;
    adaptive_fps_ratio_last_    = 1.0f;
    adaptive_last_action_       = "none";
}

void ScriptedScenesDesktop::stop_pipeline_workers_locked()
{
    {
        std::lock_guard<std::mutex> pipeline_lock(pipeline_mutex_);
        stop_workers_ = true;
        frame_jobs_.clear();
    }
    pipeline_cv_.notify_all();

    for (auto& worker : workers_)
    {
        if (worker->thread.joinable())
        {
            worker->thread.join();
        }
    }

    workers_.clear();
    workers_started_ = false;
    use_parallel_pipeline_ = false;

    {
        std::lock_guard<std::mutex> pipeline_lock(pipeline_mutex_);
        frame_jobs_.clear();
        completed_frames_.clear();
        stop_workers_ = false;
        pipeline_queue_depth_ = 0.0f;
        pipeline_completed_depth_ = 0.0f;
    }
}

bool ScriptedScenesDesktop::init_worker(WorkerState* worker_ctx)
{
    worker_ctx->render_config = pipeline_render_config_;
    worker_ctx->scene_name = current_scene_name_;
    worker_ctx->unsafe_mode = bypass_protected_calls_;
    worker_ctx->lua = std::make_unique<sol::state>();
    worker_ctx->lua->open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);
    worker_ctx->script_canvas_data.assign(worker_ctx->render_config.script_width * worker_ctx->render_config.script_height * 3, 0);
    worker_ctx->default_properties.clear();
    
    const int canvas_width = worker_ctx->render_config.script_width;
    const int canvas_height = worker_ctx->render_config.script_height;

    worker_ctx->lua->set_function("set_pixel",[worker_ctx, canvas_width, canvas_height](int x, int y, int r, int g, int b)
    {
        worker_ctx->set_pixel_calls++;
        if (x < 0 || x >= canvas_width || y < 0 || y >= canvas_height)
            return;
        int idx = (y * canvas_width + x) * 3;
        worker_ctx->script_canvas_data[idx] = static_cast<uint8_t>(std::clamp(r, 0, 255));
        worker_ctx->script_canvas_data[idx + 1] = static_cast<uint8_t>(std::clamp(g, 0, 255));
        worker_ctx->script_canvas_data[idx + 2] = static_cast<uint8_t>(std::clamp(b, 0, 255));
    });

    worker_ctx->lua->set_function("clear", [worker_ctx]()
    {
        worker_ctx->clear_calls++;
        std::fill(worker_ctx->script_canvas_data.begin(), worker_ctx->script_canvas_data.end(), 0);
    });

    worker_ctx->lua->set_function("log",[worker_ctx](const std::string& msg)
    {
        spdlog::info("[ScriptedScenesDesktop:{}] Worker: {}", worker_ctx->scene_name, msg);
    });

    worker_ctx->lua->set_function("define_property",
                             [worker_ctx](const std::string& name, const std::string& type, sol::object default_val,
                                       sol::variadic_args va)
                             {
                                 worker_ctx->default_properties[name] = default_val;
                             });

    worker_ctx->lua->set_function("get_property", [worker_ctx](const std::string& name) -> sol::object
    {
        auto it = worker_ctx->default_properties.find(name);
        if (it != worker_ctx->default_properties.end())
        {
            return it->second;
        }
        return sol::nil;
    });

    (*worker_ctx->lua)["width"] = canvas_width;
    (*worker_ctx->lua)["height"] = canvas_height;
    (*worker_ctx->lua)["time"] = 0.0;
    (*worker_ctx->lua)["dt"] = 0.0;

    bool load_ok = true;
    try
    {
        if (worker_ctx->unsafe_mode)
        {
            worker_ctx->lua->script(script_content_);
        }
        else
        {
            auto result = worker_ctx->lua->script(
                script_content_,[](lua_State*, sol::protected_function_result pfr) { return pfr; });
            if (!result.valid())
            {
                sol::error err = result;
                spdlog::error("[ScriptedScenesDesktop:{}] Worker script load error: {}", worker_ctx->scene_name, err.what());
                load_ok = false;
            }
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::error("[ScriptedScenesDesktop:{}] Worker script load exception: {}", worker_ctx->scene_name, ex.what());
        load_ok = false;
    }

    if (!load_ok)
    {
        worker_ctx->lua_loaded = false;
        return false;
    }

    try
    {
        if (worker_ctx->unsafe_mode)
        {
            sol::function setup_fn = (*worker_ctx->lua)["setup"];
            if (setup_fn.valid())
            {
                setup_fn();
            }

            sol::function init_fn = (*worker_ctx->lua)["initialize"];
            if (init_fn.valid())
            {
                init_fn();
            }
        }
        else
        {
            sol::protected_function setup_fn = (*worker_ctx->lua)["setup"];
            if (setup_fn.valid())
            {
                auto setup_result = setup_fn();
                if (!setup_result.valid())
                {
                    sol::error err = setup_result;
                    spdlog::error("[ScriptedScenesDesktop:{}] Worker setup() error: {}", worker_ctx->scene_name, err.what());
                }
            }

            sol::protected_function init_fn = (*worker_ctx->lua)["initialize"];
            if (init_fn.valid())
            {
                auto init_result = init_fn();
                if (!init_result.valid())
                {
                    sol::error err = init_result;
                    spdlog::error("[ScriptedScenesDesktop:{}] Worker initialize() error: {}", worker_ctx->scene_name, err.what());
                }
            }
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::error("[ScriptedScenesDesktop:{}] Worker init exception: {}", worker_ctx->scene_name, ex.what());
        worker_ctx->lua_loaded = false;
        return false;
    }

    worker_ctx->lua_loaded = true;
    return true;
}

void ScriptedScenesDesktop::sync_pipeline_workers_locked()
{
    // 1. Clean up officially finished workers
    for (auto it = workers_.begin(); it != workers_.end(); )
    {
        if ((*it)->is_finished)
        {
            if ((*it)->thread.joinable())
            {
                (*it)->thread.join();
            }
            it = workers_.erase(it);
        }
        else
        {
            ++it;
        }
    }

    // 2. Count actively running workers
    int active_count = 0;
    for (const auto& w : workers_)
    {
        if (!w->stop_requested)
        {
            active_count++;
        }
    }

    // 3. Scale up dynamically
    while (active_count < pipeline_worker_count_)
    {
        auto new_worker = std::make_unique<WorkerState>();
        new_worker->id = static_cast<int>(workers_.size());
        
        if (!init_worker(new_worker.get()))
        {
            break; // initialization failed
        }
        
        WorkerState* raw_ptr = new_worker.get();
        workers_.push_back(std::move(new_worker));
        raw_ptr->thread = std::thread(&ScriptedScenesDesktop::worker_loop, this, raw_ptr);
        active_count++;
    }

    // 4. Scale down gracefully
    if (active_count > pipeline_worker_count_)
    {
        std::lock_guard<std::mutex> pipeline_lock(pipeline_mutex_);
        for (auto it = workers_.rbegin(); it != workers_.rend(); ++it)
        {
            if (!(*it)->stop_requested)
            {
                (*it)->stop_requested = true;
                active_count--;
                if (active_count == pipeline_worker_count_)
                {
                    break;
                }
            }
        }
        pipeline_cv_.notify_all();
    }
}

bool ScriptedScenesDesktop::start_pipeline_workers_locked()
{
    stop_pipeline_workers_locked();

    if (!is_lua_loaded_ || !offload_render_ || !deterministic_parallel_)
    {
        return false;
    }

    {
        std::lock_guard<std::mutex> pipeline_lock(pipeline_mutex_);
        completed_frames_.clear();
        frame_jobs_.clear();
        stop_workers_ = false;
    }

    next_schedule_frame_index_ = 0;
    next_send_frame_index_ = 0;
    missing_frame_since_ = 0.0;
    pipeline_start_time_ = get_time_sec();

    sync_pipeline_workers_locked();

    if (workers_.empty())
    {
        stop_pipeline_workers_locked();
        return false;
    }

    workers_started_ = true;
    use_parallel_pipeline_ = true;
    schedule_pipeline_jobs_locked();

    spdlog::info(
        "[ScriptedScenesDesktop:{}] Parallel pipeline started: workers={} lookahead={} max_queue={} unsafe_mode={}.",
        current_scene_name_, pipeline_worker_count_, pipeline_lookahead_depth_, pipeline_max_queued_frames_, bypass_protected_calls_ ? "on" : "off");

    return true;
}

void ScriptedScenesDesktop::maybe_update_pipeline_mode_locked()
{
    const bool desired = is_lua_loaded_ && offload_render_ && deterministic_parallel_;

    if (!desired)
    {
        if (workers_started_)
            stop_pipeline_workers_locked();
        return;
    }

    if (!workers_started_)
    {
        start_pipeline_workers_locked();
    }
}

void ScriptedScenesDesktop::schedule_pipeline_jobs_locked()
{
    if (!use_parallel_pipeline_)
        return;

    const double dt = 1.0 / static_cast<double>(std::max(1.0f, pipeline_target_fps_));

    std::lock_guard<std::mutex> pipeline_lock(pipeline_mutex_);
    const uint64_t max_lookahead = static_cast<uint64_t>(std::max(1, pipeline_lookahead_depth_));
    const size_t max_queue = static_cast<size_t>(std::max(1, pipeline_max_queued_frames_));
    while (true)
    {
        const uint64_t outstanding_frames =
            (next_schedule_frame_index_ > next_send_frame_index_)
                ? (next_schedule_frame_index_ - next_send_frame_index_)
                : 0;
        const bool within_lookahead = outstanding_frames < max_lookahead;
        const bool queue_has_capacity = (frame_jobs_.size() + completed_frames_.size()) < max_queue;
        if (!within_lookahead || !queue_has_capacity)
            break;

        const uint64_t frame_index = next_schedule_frame_index_;
        frame_jobs_.push_back(FrameJob{frame_index, static_cast<double>(frame_index) * dt, dt});
        next_schedule_frame_index_++;
    }

    pipeline_queue_depth_ = static_cast<float>(frame_jobs_.size());
    pipeline_completed_depth_ = static_cast<float>(completed_frames_.size());
    pipeline_cv_.notify_all();
}

std::optional<ScriptedScenesDesktop::FrameResult> ScriptedScenesDesktop::try_take_next_pipeline_frame_locked(double now)
{
    std::lock_guard<std::mutex> pipeline_lock(pipeline_mutex_);

    auto stale_end = completed_frames_.lower_bound(next_send_frame_index_);
    completed_frames_.erase(completed_frames_.begin(), stale_end);

    auto exact = completed_frames_.find(next_send_frame_index_);
    if (exact != completed_frames_.end())
    {
        FrameResult result = std::move(exact->second);
        completed_frames_.erase(exact);
        missing_frame_since_ = 0.0;
        pipeline_reorder_wait_ms_ = 0.0f;
        pipeline_queue_depth_ = static_cast<float>(frame_jobs_.size());
        pipeline_completed_depth_ = static_cast<float>(completed_frames_.size());
        return result;
    }

    if (missing_frame_since_ <= 0.0)
        missing_frame_since_ = now;

    const float waited_ms = static_cast<float>((now - missing_frame_since_) * 1000.0);
    pipeline_reorder_wait_ms_ = waited_ms;

    if (waited_ms >= pipeline_max_reorder_wait_ms_)
    {
        if (!completed_frames_.empty())
        {
            const uint64_t first_ready = completed_frames_.begin()->first;
            if (first_ready > next_send_frame_index_)
            {
                pipeline_frames_dropped_ += (first_ready - next_send_frame_index_);
                next_send_frame_index_ = first_ready;
            }

            auto fallback = completed_frames_.find(next_send_frame_index_);
            if (fallback != completed_frames_.end())
            {
                FrameResult result = std::move(fallback->second);
                completed_frames_.erase(fallback);
                missing_frame_since_ = 0.0;
                pipeline_reorder_wait_ms_ = 0.0f;
                pipeline_queue_depth_ = static_cast<float>(frame_jobs_.size());
                pipeline_completed_depth_ = static_cast<float>(completed_frames_.size());
                return result;
            }
        }

        ++pipeline_frames_dropped_;
        if (next_send_frame_index_ < next_schedule_frame_index_)
        {
            ++next_send_frame_index_;
        }
        missing_frame_since_ = now;
    }

    pipeline_queue_depth_ = static_cast<float>(frame_jobs_.size());
    pipeline_completed_depth_ = static_cast<float>(completed_frames_.size());
    return std::nullopt;
}

void ScriptedScenesDesktop::maybe_adapt_pipeline_locked()
{
    if (!adaptive_pipeline_ || !use_parallel_pipeline_)
        return;

    const uint64_t drops_this_window = pipeline_frames_dropped_ - adaptive_last_drop_count_;
    adaptive_last_drop_count_ = pipeline_frames_dropped_;
    adaptive_drops_last_window_ = drops_this_window;

    const float fps_ratio = (pipeline_target_fps_ > 0.0f)
        ? pipeline_effective_send_fps_ / pipeline_target_fps_
        : 1.0f;
    adaptive_fps_ratio_last_ = fps_ratio;

    const double base_frame_budget_ms = 1000.0 / static_cast<double>(std::max(1.0f, pipeline_target_fps_));
    const double parallel_budget_ms = base_frame_budget_ms * pipeline_worker_count_ * 0.95;
    
    const bool render_over_budget = pipeline_avg_worker_render_ms_ > static_cast<float>(parallel_budget_ms);
    const bool performing_poorly =
        (drops_this_window > 0) ||
        (fps_ratio < ADAPTIVE_FPS_POOR_THRESHOLD) ||
        render_over_budget;

    if (performing_poorly)
    {
        adaptive_stable_seconds_ = 0;
        ++adaptive_drop_bursts_;

        bool soft_changed = false;

        if (pipeline_lookahead_depth_ < ADAPTIVE_LOOKAHEAD_MAX)
        {
            pipeline_lookahead_depth_ = std::min(
                pipeline_lookahead_depth_ + ADAPTIVE_LOOKAHEAD_STEP_UP,
                ADAPTIVE_LOOKAHEAD_MAX);
            soft_changed = true;
        }

        if (pipeline_max_queued_frames_ < ADAPTIVE_QUEUE_MAX)
        {
            pipeline_max_queued_frames_ = std::min(
                pipeline_max_queued_frames_ + ADAPTIVE_QUEUE_STEP_UP,
                ADAPTIVE_QUEUE_MAX);
            soft_changed = true;
        }

        if (pipeline_max_reorder_wait_ms_ < ADAPTIVE_REORDER_MAX_MS)
        {
            pipeline_max_reorder_wait_ms_ = std::min(
                pipeline_max_reorder_wait_ms_ + ADAPTIVE_REORDER_STEP_UP_MS,
                ADAPTIVE_REORDER_MAX_MS);
            soft_changed = true;
        }

        if (soft_changed)
        {
            adaptive_last_action_ = "soft-up: lookahead=" + std::to_string(pipeline_lookahead_depth_)
                + " queue=" + std::to_string(pipeline_max_queued_frames_);

            spdlog::info(
                "[ScriptedScenesDesktop:{}] Adaptive soft-up: lookahead={} queue={} reorder_wait={:.1f}ms"
                " (drops={} fps_ratio={:.2f} burst={})",
                current_scene_name_, pipeline_lookahead_depth_, pipeline_max_queued_frames_,
                pipeline_max_reorder_wait_ms_, drops_this_window, fps_ratio, adaptive_drop_bursts_);
        }

        if (adaptive_drop_bursts_ >= ADAPTIVE_SCALE_UP_BURST_THRESHOLD &&
            pipeline_worker_count_ < MAX_WORKERS)
        {
            ++pipeline_worker_count_;
            adaptive_last_action_ = "scale-up workers=" + std::to_string(pipeline_worker_count_);

            spdlog::info(
                "[ScriptedScenesDesktop:{}] Adaptive scale-up: workers {} -> {} "
                "(drops={} fps_ratio={:.2f} render_ms={:.2f} budget_ms={:.2f})",
                current_scene_name_, pipeline_worker_count_ - 1, pipeline_worker_count_,
                drops_this_window, fps_ratio, pipeline_avg_worker_render_ms_, parallel_budget_ms);

            adaptive_drop_bursts_ = 0;
            // Removed complete restart. The worker sync handles adding threads transparently.
        }
    }
    else
    {
        adaptive_drop_bursts_ = 0;
        ++adaptive_stable_seconds_;

        if (adaptive_stable_seconds_ >= ADAPTIVE_SCALE_DOWN_STABLE_SECONDS)
        {
            const double simulated_lower_budget_ms = base_frame_budget_ms * (pipeline_worker_count_ - 1) * 0.90;

            if (pipeline_worker_count_ > ADAPTIVE_MIN_WORKERS && 
                pipeline_avg_worker_render_ms_ < static_cast<float>(simulated_lower_budget_ms))
            {
                --pipeline_worker_count_;
                adaptive_last_action_ = "scale-down workers=" + std::to_string(pipeline_worker_count_);

                spdlog::info(
                    "[ScriptedScenesDesktop:{}] Adaptive scale-down: workers {} -> {} "
                    "(stable for {}s, fps_ratio={:.2f})",
                    current_scene_name_, pipeline_worker_count_ + 1, pipeline_worker_count_,
                    adaptive_stable_seconds_, fps_ratio);

                adaptive_stable_seconds_ = 0;
                // Graceful retirement
            }
            else
            {
                bool soft_changed = false;

                if (pipeline_lookahead_depth_ > ADAPTIVE_LOOKAHEAD_MIN)
                {
                    --pipeline_lookahead_depth_;
                    soft_changed = true;
                }

                if (pipeline_max_queued_frames_ > ADAPTIVE_QUEUE_MIN)
                {
                    pipeline_max_queued_frames_ = std::max(
                        pipeline_max_queued_frames_ - ADAPTIVE_QUEUE_STEP_UP / 2,
                        ADAPTIVE_QUEUE_MIN);
                    soft_changed = true;
                }

                if (pipeline_max_reorder_wait_ms_ > ADAPTIVE_REORDER_MIN_MS)
                {
                    pipeline_max_reorder_wait_ms_ = std::max(
                        pipeline_max_reorder_wait_ms_ - ADAPTIVE_REORDER_STEP_DOWN_MS,
                        ADAPTIVE_REORDER_MIN_MS);
                    soft_changed = true;
                }

                if (soft_changed)
                {
                    adaptive_last_action_ = "soft-down: lookahead=" + std::to_string(pipeline_lookahead_depth_)
                        + " queue=" + std::to_string(pipeline_max_queued_frames_);

                    spdlog::info(
                        "[ScriptedScenesDesktop:{}] Adaptive soft-down: lookahead={} queue={} reorder_wait={:.1f}ms"
                        " (stable for {}s, fps_ratio={:.2f})",
                        current_scene_name_, pipeline_lookahead_depth_, pipeline_max_queued_frames_,
                        pipeline_max_reorder_wait_ms_, adaptive_stable_seconds_, fps_ratio);
                }

                adaptive_stable_seconds_ = ADAPTIVE_SCALE_DOWN_STABLE_SECONDS / 2;
            }
        }
    }
}

void ScriptedScenesDesktop::worker_loop(WorkerState* worker)
{
    if (!worker)
    {
        return;
    }

    while (true)
    {
        FrameJob job;
        {
            std::unique_lock<std::mutex> pipeline_lock(pipeline_mutex_);
            pipeline_cv_.wait(pipeline_lock, [this, worker]()
            {
                return stop_workers_ || worker->stop_requested || !frame_jobs_.empty();
            });

            if (stop_workers_ || worker->stop_requested)
            {
                break;
            }

            job = frame_jobs_.front();
            frame_jobs_.pop_front();
            pipeline_queue_depth_ = static_cast<float>(frame_jobs_.size());
        }

        if (!worker->lua || !worker->lua_loaded)
        {
            continue;
        }

        worker->set_pixel_calls = 0;
        worker->clear_calls = 0;

        const double total_start = get_time_sec();
        (*worker->lua)["time"] = job.t;
        (*worker->lua)["dt"] = job.dt;

        bool render_ok = true;
        double render_ms = 0.0;

        try
        {
            if (worker->unsafe_mode)
            {
                const double render_start = get_time_sec();
                sol::function render_fn = (*worker->lua)["render"];
                if (render_fn.valid())
                {
                    render_fn();
                }
                const double render_end = get_time_sec();
                render_ms = (render_end - render_start) * 1000.0;
            }
            else
            {
                const double render_start = get_time_sec();
                sol::protected_function render_fn = (*worker->lua)["render"];
                if (render_fn.valid())
                {
                    auto result = render_fn();
                    if (!result.valid())
                    {
                        sol::error err = result;
                        spdlog::error("[ScriptedScenesDesktop:{}] worker render() error: {}", worker->scene_name, err.what());
                        render_ok = false;
                    }
                }
                const double render_end = get_time_sec();
                render_ms = (render_end - render_start) * 1000.0;
            }
        }
        catch (const std::exception& ex)
        {
            spdlog::error("[ScriptedScenesDesktop:{}] worker render() exception: {}", worker->scene_name, ex.what());
            render_ok = false;
        }

        if (!render_ok)
        {
            continue;
        }

        FrameResult result;
        result.frame_index = job.frame_index;
        result.set_pixel_calls = worker->set_pixel_calls;
        result.clear_calls = worker->clear_calls;
        result.render_ms = render_ms;

        blit_script_canvas_to_output(worker->script_canvas_data, result.frame_data, worker->render_config);

        const double total_end = get_time_sec();
        result.total_ms = (total_end - total_start) * 1000.0;

        {
            std::lock_guard<std::mutex> pipeline_lock(pipeline_mutex_);
            if (!stop_workers_)
            {
                completed_frames_[result.frame_index] = std::move(result);
                pipeline_completed_depth_ = static_cast<float>(completed_frames_.size());
            }
        }
    }
    
    worker->is_finished = true;
}

std::optional<std::unique_ptr<UdpPacket, void (*)(UdpPacket*)>> ScriptedScenesDesktop::compute_next_packet(
    const std::string sceneName)
{
    const double total_start = get_time_sec();
    std::lock_guard<std::mutex> lock(script_mutex_);

    if ((sceneName != current_scene_name_ && sceneName != "Latest Lua Scene") || !is_lua_loaded_ || !offload_render_)
    {
        return std::nullopt;
    }

    maybe_update_pipeline_mode_locked();

    double current_time = get_time_sec();

    if (use_parallel_pipeline_)
    {
        schedule_pipeline_jobs_locked();

        auto next_frame = try_take_next_pipeline_frame_locked(current_time);
        
        std::optional<std::unique_ptr<UdpPacket, void (*)(UdpPacket*)>> packet_opt = std::nullopt;

        if (profile_window_start_ <= 0.0)
            profile_window_start_ = current_time;

        if (next_frame.has_value())
        {
            const double packet_start = get_time_sec();
            auto packet = std::unique_ptr<UdpPacket, void (*)(UdpPacket*)>(
                new ScriptedScenesPacket(next_frame->frame_data),[](UdpPacket* p) { delete dynamic_cast<ScriptedScenesPacket*>(p); }
            );
            const double packet_end = get_time_sec();

            ++next_send_frame_index_;
            ++pipeline_frames_sent_;

            profile_frames_++;
            profile_set_pixel_calls_ += next_frame->set_pixel_calls;
            profile_clear_calls_ += next_frame->clear_calls;
            profile_globals_ms_sum_ += 0.0;
            profile_lua_render_ms_sum_ += next_frame->render_ms;
            profile_packet_ms_sum_ += (packet_end - packet_start) * 1000.0;
            profile_total_ms_sum_ += next_frame->total_ms;

            pipeline_worker_render_ms_sum_ += next_frame->render_ms;
            pipeline_worker_total_ms_sum_ += next_frame->total_ms;
            pipeline_worker_samples_++;

            packet_opt = std::move(packet);
        }

        if (current_time - profile_window_start_ >= 1.0)
        {
            double elapsed = current_time - profile_window_start_;
            pipeline_effective_send_fps_ = static_cast<float>(profile_frames_) / static_cast<float>(elapsed);
            current_fps_ = pipeline_effective_send_fps_;

            if (profile_frames_ > 0)
            {
                const float frames = static_cast<float>(profile_frames_);
                profile_avg_globals_ms_ = static_cast<float>(profile_globals_ms_sum_ / frames);
                profile_avg_lua_render_ms_ = static_cast<float>(profile_lua_render_ms_sum_ / frames);
                profile_avg_packet_ms_ = static_cast<float>(profile_packet_ms_sum_ / frames);
                profile_avg_total_ms_ = static_cast<float>(profile_total_ms_sum_ / frames);
                profile_avg_set_pixel_calls_per_frame_ = static_cast<float>(profile_set_pixel_calls_) / frames;
                profile_avg_clear_calls_per_frame_ = static_cast<float>(profile_clear_calls_) / frames;
            }
            else
            {
                profile_avg_globals_ms_ = 0.0f;
                profile_avg_lua_render_ms_ = 0.0f;
                profile_avg_packet_ms_ = 0.0f;
                profile_avg_total_ms_ = 0.0f;
                profile_avg_set_pixel_calls_per_frame_ = 0.0f;
                profile_avg_clear_calls_per_frame_ = 0.0f;
            }

            if (pipeline_worker_samples_ > 0)
            {
                pipeline_avg_worker_render_ms_ = static_cast<float>(pipeline_worker_render_ms_sum_ / static_cast<double>(pipeline_worker_samples_));
                pipeline_avg_worker_total_ms_ = static_cast<float>(pipeline_worker_total_ms_sum_ / static_cast<double>(pipeline_worker_samples_));
            }

            spdlog::info(
                "[ScriptedScenesDesktop:{}] parallel avg: total={:.3f}ms render={:.3f}ms packet={:.3f}ms queue={:.0f} ready={:.0f} dropped={} fps={:.1f}",
                current_scene_name_, profile_avg_total_ms_, profile_avg_lua_render_ms_,
                profile_avg_packet_ms_, pipeline_queue_depth_, pipeline_completed_depth_,
                pipeline_frames_dropped_, pipeline_effective_send_fps_);

            maybe_adapt_pipeline_locked();
            sync_pipeline_workers_locked(); // Seamlessly enforce changes and reap retired workers

            profile_window_start_ = current_time;
            profile_frames_ = 0;
            profile_set_pixel_calls_ = 0;
            profile_clear_calls_ = 0;
            profile_globals_ms_sum_ = 0.0;
            profile_lua_render_ms_sum_ = 0.0;
            profile_packet_ms_sum_ = 0.0;
            profile_total_ms_sum_ = 0.0;
            pipeline_worker_render_ms_sum_ = 0.0;
            pipeline_worker_total_ms_sum_ = 0.0;
            pipeline_worker_samples_ = 0;
        }

        return packet_opt;
    }

    if (!lua_) return std::nullopt;

    double t = current_time - start_time_;
    double dt = current_time - last_time_;
    last_time_ = current_time;

    frame_count_++;
    if (current_time - last_fps_update_ >= 1.0)
    {
        current_fps_ = static_cast<float>(frame_count_) / static_cast<float>(current_time - last_fps_update_);
        frame_count_ = 0;
        last_fps_update_ = current_time;
    }

    (*lua_)["time"] = t;
    (*lua_)["dt"] = dt;
    const double globals_end = get_time_sec();
    const double globals_ms = (globals_end - total_start) * 1000.0;

    double render_ms = 0.0;
    sol::protected_function render_fn = (*lua_)["render"];
    if (render_fn.valid())
    {
        const double render_start = get_time_sec();
        auto result = render_fn();
        const double render_end = get_time_sec();
        render_ms = (render_end - render_start) * 1000.0;
        if (!result.valid())
        {
            sol::error err = result;
            spdlog::error("[ScriptedScenesDesktop:{}] render() error: {}", current_scene_name_, err.what());
            return std::nullopt;
        }
    }

    const double packet_start = get_time_sec();
    blit_script_canvas_to_output_locked();
    auto packet = std::unique_ptr<UdpPacket, void (*)(UdpPacket*)>(
        new ScriptedScenesPacket(canvas_data_),[](UdpPacket* p) { delete dynamic_cast<ScriptedScenesPacket*>(p); }
    );
    const double packet_end = get_time_sec();
    const double packet_ms = (packet_end - packet_start) * 1000.0;
    const double total_ms = (packet_end - total_start) * 1000.0;

    profile_frames_++;
    profile_globals_ms_sum_ += globals_ms;
    profile_lua_render_ms_sum_ += render_ms;
    profile_packet_ms_sum_ += packet_ms;
    profile_total_ms_sum_ += total_ms;

    if (profile_window_start_ <= 0.0)
        profile_window_start_ = current_time;

    if (current_time - profile_window_start_ >= 1.0)
    {
        if (profile_frames_ > 0)
        {
            const float frames = static_cast<float>(profile_frames_);
            profile_avg_globals_ms_ = static_cast<float>(profile_globals_ms_sum_ / frames);
            profile_avg_lua_render_ms_ = static_cast<float>(profile_lua_render_ms_sum_ / frames);
            profile_avg_packet_ms_ = static_cast<float>(profile_packet_ms_sum_ / frames);
            profile_avg_total_ms_ = static_cast<float>(profile_total_ms_sum_ / frames);
            profile_avg_set_pixel_calls_per_frame_ = static_cast<float>(profile_set_pixel_calls_) / frames;
            profile_avg_clear_calls_per_frame_ = static_cast<float>(profile_clear_calls_) / frames;
        }
        else
        {
            profile_avg_globals_ms_ = 0.0f;
            profile_avg_lua_render_ms_ = 0.0f;
            profile_avg_packet_ms_ = 0.0f;
            profile_avg_total_ms_ = 0.0f;
            profile_avg_set_pixel_calls_per_frame_ = 0.0f;
            profile_avg_clear_calls_per_frame_ = 0.0f;
        }

        spdlog::info(
            "[ScriptedScenesDesktop:{}] perf avg: total={:.3f}ms render={:.3f}ms globals={:.3f}ms packet={:.3f}ms set_pixel/frame={:.1f} clear/frame={:.2f} fps={:.1f}",
            current_scene_name_, profile_avg_total_ms_, profile_avg_lua_render_ms_,
            profile_avg_globals_ms_, profile_avg_packet_ms_,
            profile_avg_set_pixel_calls_per_frame_, profile_avg_clear_calls_per_frame_, current_fps_);

        profile_window_start_ = current_time;
        profile_frames_ = 0;
        profile_set_pixel_calls_ = 0;
        profile_clear_calls_ = 0;
        profile_globals_ms_sum_ = 0.0;
        profile_lua_render_ms_sum_ = 0.0;
        profile_packet_ms_sum_ = 0.0;
        profile_total_ms_sum_ = 0.0;
    }

    return packet;
}