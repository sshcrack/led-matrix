#pragma once

#include "shared/desktop/plugin/main.h"
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <map>
#include "shared/desktop/utils.h"

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

class ScriptedScenesPacket : public UdpPacket
{
public:
    std::vector<uint8_t> frameData;

    explicit ScriptedScenesPacket(const std::vector<uint8_t>& data) : UdpPacket(0x04), frameData(data)
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

    std::optional<std::unique_ptr<UdpPacket, void (*)(UdpPacket*)>>
    compute_next_packet(const std::string sceneName) override;

    void initialize_imgui(ImGuiContext* im_gui_context, ImGuiMemAllocFunc* alloc_fn, ImGuiMemFreeFunc* free_fn,
                          void** user_data) override;

private:
    std::unique_ptr<sol::state> lua_;
    std::string current_scene_name_;
    bool is_lua_loaded_;
    bool offload_render_;

    // Desktop canvas buffer
    std::vector<uint8_t> canvas_data_;
    int matrix_width_ = 128;
    int matrix_height_ = 128;

    std::map<std::string, sol::object> default_properties_;

    // Time tracking
    double start_time_ = 0.0;
    double last_time_ = 0.0;

    // FPS tracking
    int frame_count_ = 0;
    double last_fps_update_ = 0.0;
    float current_fps_ = 0.0f;

    std::mutex script_mutex_;

    void setup_lua_state();
    bool load_and_exec_script(const std::string& script_content);
};
