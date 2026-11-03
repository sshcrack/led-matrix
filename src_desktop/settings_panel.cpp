#include "app_state.h"
#include "settings_panel.h"
#include "filters.h"
#include "matrix_connection.h"

#include <iostream>

#include <fmt/format.h>
#include <imgui.h>
#include "imgui_stdlib.h"
#include <shared/desktop/utils.h>
#include <spdlog/spdlog.h>

using namespace Config;

namespace desktop_app::detail {
    SettingsPanel::SettingsPanel(Plugins::PluginManager *pl, ConfigManager *cfg)
        : pl(pl), cfg(cfg),
          hostname(cfg->getGeneralConfig().getHostname()),
          port(cfg->getGeneralConfig().getPort()),
          fpsLimit(cfg->getGeneralConfig().getFpsLimit()),
          udpFpsLimit(cfg->getGeneralConfig().getUdpFpsLimit()),
          matrixOnOnStart(cfg->getGeneralConfig().isTurnMatrixOnOnStart()),
          matrixOffOnExit(cfg->getGeneralConfig().isTurnMatrixOffOnExit()),
          ytdlpPathInput(cfg->getGeneralConfig().getYtDlpPath()),
          ytdlpStatusError(check_ytdlp_available()) {
    }

    void SettingsPanel::render() {
        General &generalCfg = cfg->getGeneralConfig();
        ImGui::SeparatorText("General Device Settings");

        if (ImGui::InputTextWithHint("LED Matrix hostname", "e.g. 10.4.1.2", &hostname,
                                     ImGuiInputTextFlags_CallbackCharFilter, hostname_filter)) {
            std::cout << "Hostname changed to: " << hostname << std::endl;
            generalCfg.setHostname(hostname);
            ws->stop();
        }

        static std::string apiToken = generalCfg.getApiToken();
        if (ImGui::InputTextWithHint("API token", "optional", &apiToken, ImGuiInputTextFlags_Password)) {
            generalCfg.setApiToken(apiToken);
            ws->stop();
        }
        ImGui::SetItemTooltip("Only needed when LED_MATRIX_API_TOKEN is set on the Pi (/etc/default/led-matrix).");

        bool somethingInvalid = false;
        if (hostname.empty()) {
            const ImVec2 min = ImGui::GetItemRectMin();
            const ImVec2 max = ImGui::GetItemRectMax();
            ImDrawList *draw_list = ImGui::GetWindowDrawList();
            draw_list->AddRect(min, max, IM_COL32(255, 0, 0, 255), 0.0f, 0, 2.0f);
            somethingInvalid = true;
        }

        if (ImGui::InputScalar("Port", ImGuiDataType_U16, &port, nullptr, nullptr, "%u", ImGuiInputTextFlags_None)) {
            std::cout << "Port changed to: " << port << std::endl;
            generalCfg.setPort(port);
        }

        if (port < 1 || port > 65535) {
            const ImVec2 min = ImGui::GetItemRectMin();
            const ImVec2 max = ImGui::GetItemRectMax();
            ImDrawList *draw_list = ImGui::GetWindowDrawList();
            draw_list->AddRect(min, max, IM_COL32(255, 0, 0, 255), 0.0f, 0, 2.0f);
            somethingInvalid = true;
        }

        if (ImGui::InputInt("FPS Limit", &fpsLimit, 1, 5, ImGuiInputFlags_None)) {
            if (fpsLimit < 1) fpsLimit = 1;
            if (fpsLimit > 360) fpsLimit = 360;
            generalCfg.setFpsLimit(fpsLimit);
            HelloImGui::GetRunnerParams()->fpsIdling.fpsIdle = fpsLimit;
        }
        if (fpsLimit < 1 || fpsLimit > 360) {
            const ImVec2 min = ImGui::GetItemRectMin();
            const ImVec2 max = ImGui::GetItemRectMax();
            ImDrawList *draw_list = ImGui::GetWindowDrawList();
            draw_list->AddRect(min, max, IM_COL32(255, 0, 0, 255), 0.0f, 0, 2.0f);
            somethingInvalid = true;
        }

        if (ImGui::SliderInt("UDP Send FPS", &udpFpsLimit, 1, 120, "%d FPS")) {
            if (udpFpsLimit < 1) udpFpsLimit = 1;
            if (udpFpsLimit > 120) udpFpsLimit = 120;
            generalCfg.setUdpFpsLimit(udpFpsLimit);
        }
        ImGui::SetItemTooltip("Limits the rate at which frames are sent to the Pi. "
                              "Lower this if the Pi can't keep up (e.g. 15-25 FPS).");

        if (somethingInvalid) {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Please fix the highlighted fields.");
        } else if (initialConnect) {
            ws->setUrl(fmt::format("ws://{}:{}/desktopWebsocket", hostname, port));
            ws->start();
            matrixVersionManager.checkMatrixVersionAsync(hostname, port);
            initialConnect = false;
            if (generalCfg.isTurnMatrixOnOnStart()) {
                spdlog::info("Turning Matrix ON on start.");
                change_matrix_status(hostname, port, true);
            }
        }

        if (HelloImGui::GetRunnerParams()->appWindowParams.hidden) {
            return;
        }

        if (startMinimized) {
            if (startupFrameCounter > 10) {
                spdlog::info("Hiding window after startup.");
                HelloImGui::GetRunnerParams()->appWindowParams.hidden = true;
                startMinimized = false;
            } else {
                startupFrameCounter++;
            }
            return;
        }

        if (ImGui::Checkbox("Turn Matrix On on Start", &matrixOnOnStart)) {
            generalCfg.setTurnMatrixOnOnStart(matrixOnOnStart);
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("Turn Matrix Off on Exit", &matrixOffOnExit)) {
            generalCfg.setTurnMatrixOffOnExit(matrixOffOnExit);
        }

        ImGui::SeparatorText("External Tools");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##ytdlpPath", "yt-dlp binary path (empty = use PATH)", &ytdlpPathInput);

        auto applyYtDlpPath = [&] {
            generalCfg.setYtDlpPath(ytdlpPathInput);
            ytdlpStatusError = check_ytdlp_available();
            cfg->saveConfig();
        };

        if (ImGui::Button("Apply yt-dlp Path")) {
            applyYtDlpPath();
        }
        ImGui::SameLine();
        if (ImGui::Button("Browse...")) {
            const auto selectedPath = open_file_dialog("Select yt-dlp binary");
            if (!selectedPath.empty()) {
                ytdlpPathInput = selectedPath;
                applyYtDlpPath();
            }
        }
        if (!ytdlpPathInput.empty()) {
            ImGui::SameLine();
            if (ImGui::Button("Use PATH")) {
                ytdlpPathInput.clear();
                applyYtDlpPath();
            }
        }

        if (ytdlpStatusError.empty()) {
            const auto configured = generalCfg.getYtDlpPath();
            if (configured.empty())
                ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.3f, 1.0f), "yt-dlp: found in PATH");
            else
                ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.3f, 1.0f), "yt-dlp: configured binary is available");
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.25f, 1.0f), "%s", ytdlpStatusError.c_str());
        }
        ImGui::TextDisabled("Shared by Video and SpotifyMV. You can change this at any time.");

        auto state = ws->getReadyState();
        const std::string stateStr = ws->getReadyStateString();
        ImGui::Text("WebSocket is currently: %s", stateStr.c_str());

        if (state == ix::ReadyState::Closed) {
            if (ws->isTransportStarted()) {
                ImGui::BeginDisabled();
                ImGui::Button("Reconnecting...", ImVec2(0, 0));
                ImGui::EndDisabled();
            } else if (ImGui::Button("Connect", ImVec2(0, 0))) {
                ws->setUrl(fmt::format("ws://{}:{}/desktopWebsocket", hostname, port));
                ws->start();
                matrixVersionManager.checkMatrixVersionAsync(hostname, port);
                spdlog::info("Connecting to WebSocket at ws://{}:{}/desktopWebsocket", hostname, port);
            }
            return;
        }
        if (state == ix::ReadyState::Connecting) {
            ImGui::BeginDisabled();
            ImGui::Button("Connecting...", ImVec2(0, 0));
            ImGui::EndDisabled();
            return;
        }
        if (state == ix::ReadyState::Closing) {
            ImGui::BeginDisabled();
            ImGui::Button("Disconnecting...", ImVec2(0, 0));
            ImGui::EndDisabled();
            return;
        }
        if (ImGui::Button("Disconnect", ImVec2(0, 0))) {
            ws->stop();
            spdlog::info("Disconnecting from WebSocket at ws://{}:{}/desktopWebsocket", hostname, port);
        }

        ImGui::Text("Active Scene: %s", ws->getActiveScene().c_str());
        auto last = ws->getLastError();
        if (!last.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Last Error: %s", last.c_str());
        }

        ImGui::SeparatorText("Plugin Settings");
        auto plugins = pl->get_plugins();
        if (plugins.empty()) {
            ImGui::Text("No plugins loaded.");
            return;
        }

        static std::pair<std::string, Plugins::DesktopPlugin *> selected = plugins[0];
        {
            ImGui::BeginChild("Plugin Selector Pane", ImVec2(150, 0),
                              ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
            for (const auto &currPair : plugins) {
                if (ImGui::Selectable(currPair.first.c_str(), selected.first == currPair.first))
                    selected = currPair;
            }
            ImGui::EndChild();
        }

        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::BeginChild(selected.first.c_str(), ImVec2(0, -ImGui::GetFrameHeightWithSpacing()));
        selected.second->render();
        ImGui::EndChild();
        ImGui::EndGroup();

        updateManager.render(ImGui::GetCurrentContext());
        matrixVersionManager.render(ImGui::GetCurrentContext());
        if (updateManager.shallAppExit())
            HelloImGui::GetRunnerParams()->appShallExit = true;
    }

    void render_app_menu_items(HelloImGui::RunnerParams &runnerParams, ConfigManager *cfg) {
        General &generalCfg = cfg->getGeneralConfig();
        bool autostartEnabled = generalCfg.isAutostartEnabled();
        if (ImGui::MenuItem("Start with System", nullptr, autostartEnabled)) {
            generalCfg.setAutostartEnabled(!autostartEnabled);
        }

        if (ImGui::MenuItem("Save Config", nullptr, false)) {
            cfg->saveConfig();
        }

        ImGui::Separator();

        // Matrix version compatibility info
        auto matrixVersionInfo = matrixVersionManager.getLastCheckResult();
        std::string matrixVersionText = "Matrix Version: ";
        ImVec4 matrixVersionColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f); // Default gray

        switch (matrixVersionInfo.compatibility) {
            case MatrixVersionChecker::VersionCompatibility::Compatible:
                matrixVersionText += matrixVersionInfo.version.toString() + " ✓";
                matrixVersionColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f); // Green
                break;
            case MatrixVersionChecker::VersionCompatibility::MatrixNewer:
                matrixVersionText += matrixVersionInfo.version.toString() + " (newer)";
                matrixVersionColor = ImVec4(0.0f, 0.8f, 1.0f, 1.0f); // Blue
                break;
            case MatrixVersionChecker::VersionCompatibility::DesktopNewer:
                matrixVersionText += matrixVersionInfo.version.toString() + " (needs update)";
                matrixVersionColor = ImVec4(1.0f, 0.8f, 0.0f, 1.0f); // Orange
                break;
            case MatrixVersionChecker::VersionCompatibility::NetworkError:
                matrixVersionText += "Connection Error";
                matrixVersionColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // Red
                break;
            case MatrixVersionChecker::VersionCompatibility::ParseError:
                matrixVersionText += "Parse Error";
                matrixVersionColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // Red
                break;
        }

        // Use colored text for the menu item (read-only)
        ImGui::TextColored(matrixVersionColor, "%s", matrixVersionText.c_str());

        if (matrixVersionInfo.compatibility == MatrixVersionChecker::VersionCompatibility::DesktopNewer) {
            if (ImGui::MenuItem("Show Matrix Update Dialog")) {
                matrixVersionManager.clearDialogs();
                General &generalCfg = cfg->getGeneralConfig();
                matrixVersionManager.checkMatrixVersionAsync(generalCfg.getHostname(), generalCfg.getPort());
            }
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Check for Updates", nullptr, false)) {
            updateManager.checkForUpdatesManual();
        }

        bool updateNotificationsEnabled = updateManager.isUpdateNotificationsEnabled();
        if (ImGui::MenuItem("Update Notifications", nullptr, updateNotificationsEnabled)) {
            updateManager.setUpdateNotificationsEnabled(!updateNotificationsEnabled);
        }

        if (ImGui::BeginMenu("Update Settings")) {
            auto &prefs = updateManager.getUpdatePreferences();

            if (!prefs.skippedVersion.empty()) {
                ImGui::Text("Skipped version: %s", prefs.skippedVersion.c_str());
                if (ImGui::MenuItem("Clear skipped version")) {
                    prefs.skippedVersion = "";
                    prefs.save();
                }
                ImGui::Separator();
            }

            int remindDays = prefs.remindIntervalDays;
            if (ImGui::SliderInt("Remind interval (days)", &remindDays, 1, 30)) {
                prefs.remindIntervalDays = remindDays;
                prefs.save();
            }

            if (ImGui::MenuItem("Reset all preferences")) {
                updateManager.resetUpdatePreferences();
            }

            ImGui::EndMenu();
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Hide to tray")) {
            runnerParams.appWindowParams.hidden = true;
        }
    }
}
