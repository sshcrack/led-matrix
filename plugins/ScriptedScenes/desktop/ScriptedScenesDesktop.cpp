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

    if (ImGui::Checkbox("Enable Parallel Pipeline", &enable_parallel_pipeline_))
    {
        should_restart_pipeline = true;
    }

    if (ImGui::Checkbox("Unsafe Parallel Mode", &bypass_protected_calls_))
    {
        should_restart_pipeline = true;
    }

    if (ImGui::SliderInt("Pipeline Workers", &pipeline_worker_count_, 1, 8))
    {
        should_restart_pipeline = true;
    }

    ImGui::SliderInt("Pipeline Lookahead", &pipeline_lookahead_depth_, 1, 30);
    ImGui::SliderInt("Pipeline Max Queue", &pipeline_max_queued_frames_, 4, 120);
    ImGui::SliderFloat("Max Reorder Wait (ms)", &pipeline_max_reorder_wait_ms_, 0.0f, 60.0f, "%.1f");

    ImGui::Text("Parallel script opt-in: %s", parallel_offload_opt_in_ ? "Yes" : "No");
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
        ImGui::Text("Worker render avg: %.3f ms", pipeline_avg_worker_render_ms_);
        ImGui::Text("Worker total avg: %.3f ms", pipeline_avg_worker_total_ms_);
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

                // Clear canvas on new script
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

    // set_pixel
    lua_->set_function("set_pixel", [this](int x, int y, int r, int g, int b)
    {
        profile_set_pixel_calls_++;
        if (x < 0 || x >= script_width_ || y < 0 || y >= script_height_) return;
        int idx = (y * script_width_ + x) * 3;
        script_canvas_data_[idx] = static_cast<uint8_t>(std::clamp(r, 0, 255));
        script_canvas_data_[idx + 1] = static_cast<uint8_t>(std::clamp(g, 0, 255));
        script_canvas_data_[idx + 2] = static_cast<uint8_t>(std::clamp(b, 0, 255));
    });

    // clear
    lua_->set_function("clear", [this]()
    {
        profile_clear_calls_++;
        std::fill(script_canvas_data_.begin(), script_canvas_data_.end(), 0);
    });

    // log
    lua_->set_function("log", [this](const std::string& msg)
    {
        spdlog::info("[ScriptedScenesDesktop:{}] {}", current_scene_name_, msg);
    });

    lua_->set_function("define_property",
                       [this](const std::string& name, const std::string& type, sol::object default_val,
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
        script_content,
        [](lua_State*, sol::protected_function_result pfr) { return pfr; }
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

    sol::object parallel_obj = (*lua_)["parallel_offload"];
    parallel_offload_opt_in_ = parallel_obj.is<bool>() && parallel_obj.as<bool>();

    sol::object deterministic_obj = (*lua_)["parallel_deterministic"];
    deterministic_parallel_ = deterministic_obj.is<bool>() && deterministic_obj.as<bool>();

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
    pipeline_send_window_start_ = now;
    pipeline_send_window_frames_ = 0;
    pipeline_worker_render_ms_sum_ = 0.0;
    pipeline_worker_total_ms_sum_ = 0.0;
    pipeline_worker_samples_ = 0;
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
        if (worker.thread.joinable())
        {
            worker.thread.join();
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

bool ScriptedScenesDesktop::start_pipeline_workers_locked()
{
    stop_pipeline_workers_locked();

    if (!is_lua_loaded_ || !offload_render_ || !enable_parallel_pipeline_ || !parallel_offload_opt_in_ ||
        !deterministic_parallel_)
    {
        return false;
    }

    const int worker_count = std::clamp(pipeline_worker_count_, 1, 8);
    workers_.reserve(static_cast<size_t>(worker_count));

    auto init_worker = [this](WorkerState& worker) -> bool
    {
        worker.render_config = pipeline_render_config_;
        worker.scene_name = current_scene_name_;
        worker.unsafe_mode = bypass_protected_calls_;
        worker.lua = std::make_unique<sol::state>();
        worker.lua->open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);
        worker.script_canvas_data.assign(worker.render_config.script_width * worker.render_config.script_height * 3, 0);
        worker.default_properties.clear();
        WorkerState* worker_ctx = &worker;
        const int canvas_width = worker.render_config.script_width;
        const int canvas_height = worker.render_config.script_height;

        worker.lua->set_function("set_pixel", [worker_ctx, canvas_width, canvas_height](int x, int y, int r, int g, int b)
        {
            worker_ctx->set_pixel_calls++;
            if (x < 0 || x >= canvas_width || y < 0 || y >= canvas_height)
                return;
            int idx = (y * canvas_width + x) * 3;
            worker_ctx->script_canvas_data[idx] = static_cast<uint8_t>(std::clamp(r, 0, 255));
            worker_ctx->script_canvas_data[idx + 1] = static_cast<uint8_t>(std::clamp(g, 0, 255));
            worker_ctx->script_canvas_data[idx + 2] = static_cast<uint8_t>(std::clamp(b, 0, 255));
        });

        worker.lua->set_function("clear", [worker_ctx]()
        {
            worker_ctx->clear_calls++;
            std::fill(worker_ctx->script_canvas_data.begin(), worker_ctx->script_canvas_data.end(), 0);
        });

        worker.lua->set_function("log", [&worker](const std::string& msg)
        {
            spdlog::info("[ScriptedScenesDesktop:{}] {}", worker.scene_name, msg);
        });

        worker.lua->set_function("define_property",
                                 [worker_ctx](const std::string& name, const std::string& type, sol::object default_val,
                                           sol::variadic_args va)
                                 {
                                     worker_ctx->default_properties[name] = default_val;
                                 });

        worker.lua->set_function("get_property", [worker_ctx](const std::string& name) -> sol::object
        {
            auto it = worker_ctx->default_properties.find(name);
            if (it != worker_ctx->default_properties.end())
            {
                return it->second;
            }
            return sol::nil;
        });

        (*worker.lua)["width"] = canvas_width;
        (*worker.lua)["height"] = canvas_height;
        (*worker.lua)["time"] = 0.0;
        (*worker.lua)["dt"] = 0.0;

        bool load_ok = true;
        try
        {
            if (worker.unsafe_mode)
            {
                worker.lua->script(script_content_);
            }
            else
            {
                auto result = worker.lua->script(
                    script_content_,
                    [](lua_State*, sol::protected_function_result pfr) { return pfr; });
                if (!result.valid())
                {
                    sol::error err = result;
                    spdlog::error("[ScriptedScenesDesktop:{}] Worker script load error: {}", worker.scene_name, err.what());
                    load_ok = false;
                }
            }
        }
        catch (const std::exception& ex)
        {
            spdlog::error("[ScriptedScenesDesktop:{}] Worker script load exception: {}", worker.scene_name, ex.what());
            load_ok = false;
        }

        if (!load_ok)
        {
            worker.lua_loaded = false;
            return false;
        }

        try
        {
            if (worker.unsafe_mode)
            {
                sol::function setup_fn = (*worker.lua)["setup"];
                if (setup_fn.valid())
                {
                    setup_fn();
                }

                sol::function init_fn = (*worker.lua)["initialize"];
                if (init_fn.valid())
                {
                    init_fn();
                }
            }
            else
            {
                sol::protected_function setup_fn = (*worker.lua)["setup"];
                if (setup_fn.valid())
                {
                    auto setup_result = setup_fn();
                    if (!setup_result.valid())
                    {
                        sol::error err = setup_result;
                        spdlog::error("[ScriptedScenesDesktop:{}] Worker setup() error: {}", worker.scene_name, err.what());
                    }
                }

                sol::protected_function init_fn = (*worker.lua)["initialize"];
                if (init_fn.valid())
                {
                    auto init_result = init_fn();
                    if (!init_result.valid())
                    {
                        sol::error err = init_result;
                        spdlog::error("[ScriptedScenesDesktop:{}] Worker initialize() error: {}", worker.scene_name, err.what());
                    }
                }
            }
        }
        catch (const std::exception& ex)
        {
            spdlog::error("[ScriptedScenesDesktop:{}] Worker init exception: {}", worker.scene_name, ex.what());
            worker.lua_loaded = false;
            return false;
        }

        worker.lua_loaded = true;
        return true;
    };

    for (int i = 0; i < worker_count; ++i)
    {
        workers_.push_back({});
        workers_.back().id = i;
        if (!init_worker(workers_.back()))
        {
            stop_pipeline_workers_locked();
            return false;
        }
    }

    {
        std::lock_guard<std::mutex> pipeline_lock(pipeline_mutex_);
        completed_frames_.clear();
        frame_jobs_.clear();
    }

    next_schedule_frame_index_ = 0;
    next_send_frame_index_ = 0;
    missing_frame_since_ = 0.0;
    pipeline_start_time_ = get_time_sec();

    for (int i = 0; i < worker_count; ++i)
    {
        workers_[static_cast<size_t>(i)].thread = std::thread(&ScriptedScenesDesktop::worker_loop, this, i);
    }

    workers_started_ = true;
    use_parallel_pipeline_ = true;
    schedule_pipeline_jobs_locked();

    spdlog::info(
        "[ScriptedScenesDesktop:{}] Parallel pipeline started: workers={} lookahead={} max_queue={} unsafe_mode={}.",
        current_scene_name_, worker_count, pipeline_lookahead_depth_, pipeline_max_queued_frames_, bypass_protected_calls_ ? "on" : "off");

    return true;
}

void ScriptedScenesDesktop::maybe_update_pipeline_mode_locked()
{
    const bool desired = is_lua_loaded_ && offload_render_ && enable_parallel_pipeline_ && parallel_offload_opt_in_ &&
                         deterministic_parallel_;

    if (!desired)
    {
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
        const bool within_lookahead = (next_schedule_frame_index_ - next_send_frame_index_) < max_lookahead;
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

        // Nothing ready within timeout, so advance one send index to keep the stream progressing.
        ++pipeline_frames_dropped_;
        ++next_send_frame_index_;
        missing_frame_since_ = now;
    }

    pipeline_queue_depth_ = static_cast<float>(frame_jobs_.size());
    pipeline_completed_depth_ = static_cast<float>(completed_frames_.size());
    return std::nullopt;
}

void ScriptedScenesDesktop::worker_loop(int worker_id)
{
    WorkerState* worker_ptr = nullptr;
    if (worker_id >= 0 && static_cast<size_t>(worker_id) < workers_.size())
    {
        worker_ptr = &workers_[static_cast<size_t>(worker_id)];
    }
    if (!worker_ptr)
    {
        return;
    }

    WorkerState& worker = *worker_ptr;

    while (true)
    {
        FrameJob job;
        {
            std::unique_lock<std::mutex> pipeline_lock(pipeline_mutex_);
            pipeline_cv_.wait(pipeline_lock, [this]()
            {
                return stop_workers_ || !frame_jobs_.empty();
            });

            if (stop_workers_)
            {
                return;
            }

            job = frame_jobs_.front();
            frame_jobs_.pop_front();
            pipeline_queue_depth_ = static_cast<float>(frame_jobs_.size());
        }

        if (!worker.lua || !worker.lua_loaded)
        {
            continue;
        }

        worker.set_pixel_calls = 0;
        worker.clear_calls = 0;

        const double total_start = get_time_sec();
        (*worker.lua)["time"] = job.t;
        (*worker.lua)["dt"] = job.dt;

        bool render_ok = true;
        double render_ms = 0.0;

        try
        {
            if (worker.unsafe_mode)
            {
                const double render_start = get_time_sec();
                sol::function render_fn = (*worker.lua)["render"];
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
                sol::protected_function render_fn = (*worker.lua)["render"];
                if (render_fn.valid())
                {
                    auto result = render_fn();
                    if (!result.valid())
                    {
                        sol::error err = result;
                        spdlog::error("[ScriptedScenesDesktop:{}] worker render() error: {}", worker.scene_name, err.what());
                        render_ok = false;
                    }
                }
                const double render_end = get_time_sec();
                render_ms = (render_end - render_start) * 1000.0;
            }
        }
        catch (const std::exception& ex)
        {
            spdlog::error("[ScriptedScenesDesktop:{}] worker render() exception: {}", worker.scene_name, ex.what());
            render_ok = false;
        }

        if (!render_ok)
        {
            continue;
        }

        FrameResult result;
        result.frame_index = job.frame_index;
        result.set_pixel_calls = worker.set_pixel_calls;
        result.clear_calls = worker.clear_calls;
        result.render_ms = render_ms;

        blit_script_canvas_to_output(worker.script_canvas_data, result.frame_data, worker.render_config);

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
        if (!next_frame.has_value())
        {
            return std::nullopt;
        }

        const double packet_start = get_time_sec();
        auto packet = std::unique_ptr<UdpPacket, void (*)(UdpPacket*)>(
            new ScriptedScenesPacket(next_frame->frame_data),
            [](UdpPacket* p) { delete dynamic_cast<ScriptedScenesPacket*>(p); }
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

        if (pipeline_send_window_start_ <= 0.0)
            pipeline_send_window_start_ = current_time;
        pipeline_send_window_frames_++;

        if (current_time - pipeline_send_window_start_ >= 1.0)
        {
            pipeline_effective_send_fps_ = static_cast<float>(pipeline_send_window_frames_) /
                static_cast<float>(current_time - pipeline_send_window_start_);
            pipeline_send_window_start_ = current_time;
            pipeline_send_window_frames_ = 0;
        }

        if (current_time - last_fps_update_ >= 1.0)
        {
            current_fps_ = pipeline_effective_send_fps_;
            last_fps_update_ = current_time;
        }

        if (profile_window_start_ <= 0.0)
            profile_window_start_ = current_time;

        if (current_time - profile_window_start_ >= 1.0 && profile_frames_ > 0)
        {
            const float frames = static_cast<float>(profile_frames_);
            profile_avg_globals_ms_ = static_cast<float>(profile_globals_ms_sum_ / frames);
            profile_avg_lua_render_ms_ = static_cast<float>(profile_lua_render_ms_sum_ / frames);
            profile_avg_packet_ms_ = static_cast<float>(profile_packet_ms_sum_ / frames);
            profile_avg_total_ms_ = static_cast<float>(profile_total_ms_sum_ / frames);
            profile_avg_set_pixel_calls_per_frame_ =
                static_cast<float>(profile_set_pixel_calls_) / frames;
            profile_avg_clear_calls_per_frame_ =
                static_cast<float>(profile_clear_calls_) / frames;

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

        return packet;
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
        new ScriptedScenesPacket(canvas_data_),
        [](UdpPacket* p) { delete dynamic_cast<ScriptedScenesPacket*>(p); }
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

    if (current_time - profile_window_start_ >= 1.0 && profile_frames_ > 0)
    {
        const float frames = static_cast<float>(profile_frames_);
        profile_avg_globals_ms_ = static_cast<float>(profile_globals_ms_sum_ / frames);
        profile_avg_lua_render_ms_ = static_cast<float>(profile_lua_render_ms_sum_ / frames);
        profile_avg_packet_ms_ = static_cast<float>(profile_packet_ms_sum_ / frames);
        profile_avg_total_ms_ = static_cast<float>(profile_total_ms_sum_ / frames);
        profile_avg_set_pixel_calls_per_frame_ =
            static_cast<float>(profile_set_pixel_calls_) / frames;
        profile_avg_clear_calls_per_frame_ =
            static_cast<float>(profile_clear_calls_) / frames;

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
