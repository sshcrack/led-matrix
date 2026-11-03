#pragma once

#include <string>

#include <hello_imgui/hello_imgui.h>
#include <shared/desktop/config.h>
#include <shared/desktop/plugin_loader/loader.h>

namespace desktop_app::detail {
    /// Main ImGui settings window content (device settings, external tools,
    /// websocket connection and per-plugin settings). UI state lives in the members.
    class SettingsPanel {
    public:
        SettingsPanel(Plugins::PluginManager *pl, Config::ConfigManager *cfg);

        /// Equivalent of the HelloImGui ShowGui callback body (after window request handling).
        void render();

    private:
        Plugins::PluginManager *pl;
        Config::ConfigManager *cfg;

        bool initialConnect = true;
        std::string hostname;
        int port;
        int fpsLimit;
        int udpFpsLimit;
        bool matrixOnOnStart;
        bool matrixOffOnExit;
        std::string ytdlpPathInput;
        std::string ytdlpStatusError;
    };

    /// Content of the "app" menu (HelloImGui ShowAppMenuItems).
    void render_app_menu_items(HelloImGui::RunnerParams &runnerParams, Config::ConfigManager *cfg);
}
