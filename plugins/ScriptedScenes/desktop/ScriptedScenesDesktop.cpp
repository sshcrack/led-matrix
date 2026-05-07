#include "ScriptedScenesDesktop.h"

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>
#include <spdlog/spdlog.h>
#include <chrono>

extern "C" PLUGIN_EXPORT ScriptedScenesDesktop *createScriptedScenes() {
    return new ScriptedScenesDesktop();
}

extern "C" PLUGIN_EXPORT void destroyScriptedScenes(ScriptedScenesDesktop *c) {
    delete c;
}

static double get_time_sec() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(now.time_since_epoch()).count();
}

ScriptedScenesDesktop::ScriptedScenesDesktop() {
    canvas_data_.resize(matrix_width_ * matrix_height_ * 3, 0);
}

ScriptedScenesDesktop::~ScriptedScenesDesktop() = default;

void ScriptedScenesDesktop::initialize_imgui(ImGuiContext *im_gui_context, ImGuiMemAllocFunc *alloc_fn, ImGuiMemFreeFunc *free_fn, void **user_data) {
    ImGui::SetCurrentContext(im_gui_context);
    ImGui::GetAllocatorFunctions(alloc_fn, free_fn, user_data);
}

void ScriptedScenesDesktop::render() {
    std::lock_guard<std::mutex> lock(script_mutex_);
    if (!is_lua_loaded_) {
        ImGui::Text("No Lua script loaded.");
        return;
    }
    ImGui::Text("Current Scene: %s", current_scene_name_.c_str());
    ImGui::Text("Offload Render: %s", offload_render_ ? "Yes" : "No");
}

void ScriptedScenesDesktop::on_websocket_message(const std::string message) {
    if (message.starts_with("size:")) {
        std::string sizeStr = message.substr(5);
        auto xPos = sizeStr.find('x');
        if (xPos != std::string::npos) {
            std::lock_guard<std::mutex> lock(script_mutex_);
            matrix_width_ = std::stoi(sizeStr.substr(0, xPos));
            matrix_height_ = std::stoi(sizeStr.substr(xPos + 1));
            canvas_data_.resize(matrix_width_ * matrix_height_ * 3, 0);
            
            if (lua_) {
                (*lua_)["width"]  = matrix_width_;
                (*lua_)["height"] = matrix_height_;
            }
        }
    } else if (message.starts_with("script:")) {
        std::string payload = message.substr(7);
        auto pos = payload.find(':');
        if (pos != std::string::npos) {
            std::string name = payload.substr(0, pos);
            std::string script_content = payload.substr(pos + 1);
            
            spdlog::info("[ScriptedScenesDesktop] Received script for '{}'", name);
            current_scene_name_ = name;
            
            std::lock_guard<std::mutex> lock(script_mutex_);
            setup_lua_state();
            if (load_and_exec_script(script_content)) {
                sol::protected_function setup_fn = (*lua_)["setup"];
                if (setup_fn.valid()) {
                    auto result = setup_fn();
                    if (!result.valid()) {
                        sol::error err = result;
                        spdlog::error("[ScriptedScenesDesktop] setup() error: {}", err.what());
                    }
                }

                (*lua_)["width"]  = matrix_width_;
                (*lua_)["height"] = matrix_height_;

                sol::protected_function init_fn = (*lua_)["initialize"];
                if (init_fn.valid()) {
                    auto result = init_fn();
                    if (!result.valid()) {
                        sol::error err = result;
                        spdlog::error("[ScriptedScenesDesktop] initialize() error: {}", err.what());
                    }
                }
                
                start_time_ = get_time_sec();
                last_time_ = start_time_;
                
                // Clear canvas on new script
                std::fill(canvas_data_.begin(), canvas_data_.end(), 0);
            }
        }
    }
}

void ScriptedScenesDesktop::setup_lua_state() {
    lua_ = std::make_unique<sol::state>();
    lua_->open_libraries(
        sol::lib::base,
        sol::lib::math,
        sol::lib::string,
        sol::lib::table
    );

    // set_pixel
    lua_->set_function("set_pixel", [this](int x, int y, int r, int g, int b) {
        if (x < 0 || x >= matrix_width_ || y < 0 || y >= matrix_height_) return;
        int idx = (y * matrix_width_ + x) * 3;
        canvas_data_[idx]     = static_cast<uint8_t>(std::clamp(r, 0, 255));
        canvas_data_[idx + 1] = static_cast<uint8_t>(std::clamp(g, 0, 255));
        canvas_data_[idx + 2] = static_cast<uint8_t>(std::clamp(b, 0, 255));
    });

    // clear
    lua_->set_function("clear", [this]() {
        std::fill(canvas_data_.begin(), canvas_data_.end(), 0);
    });

    // log
    lua_->set_function("log", [this](const std::string &msg) {
        spdlog::info("[ScriptedScenesDesktop:{}] {}", current_scene_name_, msg);
    });

    lua_->set_function("define_property", [this](const std::string &name, const std::string &type, sol::object default_val, sol::variadic_args va) {
        default_properties_[name] = default_val;
    });
    
    lua_->set_function("get_property", [this](const std::string &name) -> sol::object {
        auto it = default_properties_.find(name);
        if (it != default_properties_.end()) {
            return it->second;
        }
        return sol::nil;
    });

    (*lua_)["width"]  = matrix_width_;
    (*lua_)["height"] = matrix_height_;
    (*lua_)["time"]   = 0.0;
    (*lua_)["dt"]     = 0.0;
}

bool ScriptedScenesDesktop::load_and_exec_script(const std::string &script_content) {
    auto result = lua_->script(
        script_content,
        [](lua_State *, sol::protected_function_result pfr) { return pfr; }
    );

    if (!result.valid()) {
        sol::error err = result;
        spdlog::error("[ScriptedScenesDesktop:{}] Script load error: {}", current_scene_name_, err.what());
        is_lua_loaded_ = false;
        return false;
    }

    sol::object offload_obj = (*lua_)["offload"];
    if (offload_obj.is<bool>()) {
        offload_render_ = offload_obj.as<bool>();
    } else {
        offload_render_ = false;
    }

    is_lua_loaded_ = true;
    return true;
}

std::optional<std::unique_ptr<UdpPacket, void (*)(UdpPacket *)>> ScriptedScenesDesktop::compute_next_packet(const std::string sceneName) {
    std::lock_guard<std::mutex> lock(script_mutex_);
    
    if (sceneName != current_scene_name_ || !is_lua_loaded_ || !offload_render_) {
        return std::nullopt;
    }

    if (!lua_) return std::nullopt;

    double current_time = get_time_sec();
    double t = current_time - start_time_;
    double dt = current_time - last_time_;
    last_time_ = current_time;

    (*lua_)["time"] = t;
    (*lua_)["dt"]   = dt;

    sol::protected_function render_fn = (*lua_)["render"];
    if (render_fn.valid()) {
        auto result = render_fn();
        if (!result.valid()) {
            sol::error err = result;
            spdlog::error("[ScriptedScenesDesktop:{}] render() error: {}", current_scene_name_, err.what());
            return std::nullopt;
        }
    }

    return std::unique_ptr<UdpPacket, void (*)(UdpPacket *)>(
        new ScriptedScenesPacket(canvas_data_),
        [](UdpPacket *p) { delete dynamic_cast<ScriptedScenesPacket *>(p); }
    );
}
