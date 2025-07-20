#include "AudioVisualizerDesktop.h"
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
#include "imgui_stdlib.h"
#include <spdlog/spdlog.h>

extern "C" PLUGIN_EXPORT AudioVisualizerDesktop *createAudioVisualizer() {
    return new AudioVisualizerDesktop();
}

extern "C" PLUGIN_EXPORT void destroyAudioVisualizer(AudioVisualizerDesktop *c) {
    delete c;
}

AudioVisualizerDesktop::AudioVisualizerDesktop() {
    implotContext = ImPlot::CreateContext();
}

AudioVisualizerDesktop::~AudioVisualizerDesktop() = default;

int PortFilter(ImGuiInputTextCallbackData *data) {
    if (data->EventChar < 32 || data->EventChar >= 127)
        return 1; // Disallow non-printable

    if (data->BufTextLen > 5) {
        return 1; // Limit to 5 characters
    }

    char c = static_cast<char>(data->EventChar);
    if (c >= '0' && c <= '9') {
        return 0; // Allow
    }

    return 1; // Block everything else
}

void AudioVisualizerDesktop::render(ImGuiContext *ctx) {
    if (audioProcessor == nullptr)
        audioProcessor = std::make_unique<AudioProcessor>(cfg);

    ImGui::SetCurrentContext(ctx);
    ImPlot::SetCurrentContext(implotContext);

    addConnectionSettings();
    addAudioSettings();
    addAnalysisSettings();
    addDeviceSettings();

    addVisualizer();
}

void AudioVisualizerDesktop::beforeExit() {
    DesktopPlugin::beforeExit();
    ImPlot::DestroyContext();
}

void AudioVisualizerDesktop::addConnectionSettings() {
    const bool isProcessingRunning = audioProcessor->isThreadRunning();
    ImGui::SeparatorText("Connection Settings");

    static std::string port = std::to_string(cfg.port);
    if (ImGui::InputText("Port", &port, ImGuiInputTextFlags_CallbackCharFilter, PortFilter)) {
        try {
            cfg.port = std::stoi(port);
        } catch (const std::exception &e) {
            spdlog::error("Invalid port number: {}", e.what());
        }
    }

    const std::string buttonText = isProcessingRunning ? "Disconnect" : "Connect";

    if (ImGui::Button(buttonText.c_str())) {
        if (isProcessingRunning) {
            audioProcessor->stopProcessingThread();
        } else {
            lastError = "";
            // Find device index
            auto devices = audioProcessor->listDevices();
            int deviceIndex = -1;
            for (const auto &device: devices) {
                if (device.name == cfg.deviceName) {
                    deviceIndex = device.index;
                    break;
                }
            }

            auto res = audioProcessor->startProcessingThread(deviceIndex);
            if (!res.has_value()) {
                spdlog::error("Couldn't start audio processing thread: {}", res.error());
                lastError = res.error();
                return;
            }
        }
    }

    if (!lastError.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Error: %s", lastError.c_str());
    } else {
        ImGui::Text("Status: %s", isProcessingRunning ? "Processing" : "Idle");
    }

    const std::string latestError = audioProcessor->getLatestError();
    if (!audioProcessor->getLatestError().empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "AudioProcessor Error: %s", latestError.c_str());
    }
}

void AudioVisualizerDesktop::addAnalysisSettings() {
    ImGui::SeparatorText("Analysis Settings");

    static int selectedModeIdx = cfg.analysisMode;
    const std::string &modePreview = analysisModes[selectedModeIdx];
    if (ImGui::BeginCombo("Mode", modePreview.c_str())) {
        for (int n = 0; n < analysisModes.size(); n++) {
            const bool is_selected = (selectedModeIdx == n);
            if (ImGui::Selectable(analysisModes[n].c_str(), is_selected)) {
                cfg.analysisMode = static_cast<AnalysisMode>(n);
                selectedModeIdx = n;
                audioProcessor->updateAnalyzer();
            }

            // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    static int selectedFreqIdx = cfg.frequencyScale;
    const std::string &frequencyPreview = frequencyScales[selectedFreqIdx];
    if (ImGui::BeginCombo("Frequency Scale", frequencyPreview.c_str())) {
        for (int n = 0; n < frequencyScales.size(); n++) {
            const bool is_selected = (selectedFreqIdx == n);
            if (ImGui::Selectable(frequencyScales[n].c_str(), is_selected)) {
                cfg.frequencyScale = static_cast<FrequencyScale>(n);
                selectedFreqIdx = n;
                audioProcessor->updateAnalyzer();
            }

            // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    static bool linearAmplitudeScaling = cfg.linearAmplitudeScaling;
    if (ImGui::Checkbox("Linear Amplitude Scaling", &linearAmplitudeScaling))
        cfg.linearAmplitudeScaling = linearAmplitudeScaling;

    static bool interpolateMissingBands = cfg.interpolateMissingBands;
    if (ImGui::Checkbox("Interpolate Missing Bands", &interpolateMissingBands))
        cfg.interpolateMissingBands = interpolateMissingBands;

    static bool skipMissingBandsFromOutput = cfg.skipMissingBandsFromOutput;
    if (ImGui::Checkbox("Skip Missing Bands from Output", &skipMissingBandsFromOutput))
        cfg.skipMissingBandsFromOutput = skipMissingBandsFromOutput;
}

void AudioVisualizerDesktop::addAudioSettings() {
    ImGui::SeparatorText("Audio Settings");

    static uint16_t numBandsMin = 1;
    static uint16_t numBandsMax = 256;

    ImGui::SliderScalar("Number of Bands", ImGuiDataType_U16, &cfg.numBands, &numBandsMin, &numBandsMax, "%d");

    static double gainMin = 0.1;
    static double gainMax = 10.0;
    ImGui::SliderScalar("Gain", ImGuiDataType_Double, &cfg.gain, &gainMin, &gainMax, "%.1f");

    static double smoothingMin = 0.0;
    static double smoothingMax = 1.0;
    ImGui::SliderScalar("Smoothing", ImGuiDataType_Double, &cfg.smoothing, &smoothingMin, &smoothingMax, "%.2f");

    static double minFreqMin = 20.0;
    static double minFreqMax = 1000.0;
    static double maxFreqMax = 22000.0;

    ImGui::SliderScalar("Min Frequency", ImGuiDataType_Double, &cfg.minFreq, &minFreqMin, &minFreqMax, "%.1f Hz");
    ImGui::SliderScalar("Max Frequency", ImGuiDataType_Double, &cfg.maxFreq, &cfg.minFreq, &maxFreqMax, "%.1f Hz");
}

void AudioVisualizerDesktop::addDeviceSettings() {
    ImGui::SeparatorText("Audio Device Settings");
    static auto devices = audioProcessor->listDevices();
    if (cfg.deviceName.empty() && !devices.empty()) {
        cfg.deviceName = devices[0].name; // Default to first device
    }

    if (ImGui::BeginCombo("Select Device", cfg.deviceName.empty() ? "None" : cfg.deviceName.c_str())) {
        for (const auto &device: devices) {
            std::string deviceNameWithId = device.name + "##" + std::to_string(device.index);
            if (ImGui::Selectable(deviceNameWithId.c_str()))
                cfg.deviceName = device.name;

            if (cfg.deviceName == device.name)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    if (ImGui::Button("Refresh Devices")) {
        devices = audioProcessor->listDevices();
    }
}

void AudioVisualizerDesktop::addVisualizer() {
    ImGui::SeparatorText("Audio Spectrum");

    static bool showPreview = false;

    if (audioProcessor && showPreview) {
        latestBands = audioProcessor->getLatestBands();
    }

    ImGui::Checkbox("Live Preview (may cause stutters on the LED Matrix)", &showPreview);

    if (!latestBands.empty() || !showPreview)
        return;


    if (ImPlot::BeginPlot("Spectrum", ImVec2(-1, 0),
                          ImPlotFlags_NoLegend | ImPlotFlags_NoMouseText | ImPlotAxisFlags_Lock |
                          ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_NoMenus | ImPlotAxisFlags_NoDecorations)) {
        ImPlot::SetupAxisLimits(ImAxis_X1, 0.0, cfg.numBands);
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, 1.0);

        // Cache x-axis data to avoid recomputation
        static std::vector<float> x;
        if (x.size() != latestBands.size()) {
            x.resize(latestBands.size());
            for (size_t i = 0; i < x.size(); ++i)
                x[i] = static_cast<float>(i);
        }

        // Use indices to avoid frequent memory allocation
        static std::vector<size_t> defaultIndices, yellowIndices, redIndices;
        defaultIndices.clear();
        yellowIndices.clear();
        redIndices.clear();

        for (size_t i = 0; i < latestBands.size(); ++i) {
            if (latestBands[i] < 0.5f) {
                defaultIndices.push_back(i);
            } else if (latestBands[i] < 0.8f) {
                yellowIndices.push_back(i);
            } else {
                redIndices.push_back(i);
            }
        }

        // Render default bars
        ImPlot::PushStyleColor(ImPlotCol_Fill, IM_COL32(255, 255, 255, 255)); // White
        for (int idx: defaultIndices) {
            ImPlot::PlotBars("Default", &x[idx], &latestBands[idx], 1, 1);
        }
        ImPlot::PopStyleColor();

        // Render yellow bars
        ImPlot::PushStyleColor(ImPlotCol_Fill, IM_COL32(255, 255, 0, 255)); // Yellow
        for (int idx: yellowIndices) {
            ImPlot::PlotBars("Yellow", &x[idx], &latestBands[idx], 1, 1);
        }
        ImPlot::PopStyleColor();

        // Render red bars
        ImPlot::PushStyleColor(ImPlotCol_Fill, IM_COL32(255, 0, 0, 255)); // Red
        for (int idx: redIndices) {
            ImPlot::PlotBars("Red", &x[idx], &latestBands[idx], 1, 1);
        }
        ImPlot::PopStyleColor();

        ImPlot::EndPlot();
    }
}
