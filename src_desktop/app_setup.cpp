#include "app_setup.h"

#include <cstdio>
#include <filesystem>
#include <memory>

#include <hello_imgui/hello_imgui.h>
#include "imgui_freetype.h"
#include <shared/desktop/config.h>
#include <shared/desktop/utils.h>
#include "shared/desktop/plugin_loader/loader.h"
#include "spdlog/cfg/env.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace fs = std::filesystem;
using namespace Config;

namespace desktop_app::detail {
    // ---- Platform-specific utility functions ----
#ifndef WIN32
    std::string get_noto_color_emoji_path() {
        std::string result;
        const char *cmd = "fc-list :family=file | grep -i 'Noto Color Emoji' | awk -F: '{print $1}' | head -n 1";
        struct PipeCloser {
            void operator()(FILE* f) const { pclose(f); }
        };
        std::unique_ptr<FILE, PipeCloser> pipe(popen(cmd, "r"));
        if (!pipe)
            return result;
        char buffer[512];
        if (fgets(buffer, sizeof(buffer), pipe.get())) {
            result = buffer;
            // Remove newline
            result.erase(result.find_last_not_of(" \n\r\t") + 1);
        }
        return result;
    }
#endif

    // ---- Extracted helpers ----
    void setup_logging() {
        fs::path logDir = get_data_dir() / "logs";
        if (!fs::exists(logDir))
            fs::create_directories(logDir);
        auto max_size = 1048576 * 5;
        auto max_files = 3;
        spdlog::cfg::load_env_levels();
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_pattern("[%^%l%$] %v");
        console_sink->set_level(spdlog::level::debug);
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            (logDir / "app.log").string(), max_size, max_files);
        file_sink->set_level(spdlog::level::trace);
        spdlog::logger logger("multi_sink", {console_sink, file_sink});
        auto sharedLogger = std::make_shared<spdlog::logger>(logger);
        spdlog::set_default_logger(sharedLogger);
        auto assets_dir = get_exec_dir() / ".." / "share" / "led-matrix-desktop" / "assets";
        if (!fs::is_directory(assets_dir)) {
            assets_dir = get_exec_dir() / ".." / "assets";
        }
        HelloImGui::SetAssetsFolder(assets_dir.string());
    }

    void init_plugins() {
        auto pl = Plugins::PluginManager::instance();
        auto cfg = ConfigManager::instance();
        pl->initialize();
        for (const auto &[plName, plugin]: pl->get_plugins()) {
            plugin->load_config(cfg->getPluginSetting(plName));
        }
    }

    void load_additional_fonts() {
        const ImGuiIO &io = ImGui::GetIO();
#ifdef WIN32
        std::string fontPath = "C:/Windows/Fonts/seguiemj.ttf";
#else
        std::string fontPath = get_noto_color_emoji_path();
#endif
        HelloImGui::ImGuiDefaultSettings::LoadDefaultFont_WithFontAwesomeIcons();
        if (!fontPath.empty()) {
            static ImFontConfig fontCfg;
            fontCfg.MergeMode = true;
            fontCfg.FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_LoadColor;
            io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 0.0f, &fontCfg);
            io.Fonts->Build();
        }
    }
}
