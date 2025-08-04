#include "AudioVisualizerDesktop.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include "udpBandsPacket.h"

extern "C" PLUGIN_EXPORT AudioVisualizerDesktop *createAudioVisualizer() {
    return new AudioVisualizerDesktop();
}

extern "C" PLUGIN_EXPORT void destroyAudioVisualizer(AudioVisualizerDesktop *c) {
    delete c;
}

AudioVisualizerDesktop::AudioVisualizerDesktop() : beat_detected(false) {
    last_beat_time = std::chrono::steady_clock::now();
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

void AudioVisualizerDesktop::render() {
    ImPlot::SetCurrentContext(implotContext);
#ifndef _WIN32
    ImGui::Text("This plugin isn't supported for linux. Loopback recording is not available in this OS.");
#else
    addConnectionSettings();
    addAudioSettings();
    addAnalysisSettings();
    addDeviceSettings();

    addVisualizer();
#endif
}

void AudioVisualizerDesktop::load_config(std::optional<const nlohmann::json> config) {
    if (config.has_value())
        cfg = config.value();

#ifdef _WIN32
    if (audioProcessor == nullptr)
        audioProcessor = std::make_unique<AudioProcessor>(cfg);

    if (recorder == nullptr)
        recorder = std::make_unique<AudioRecorder::Recorder>();
#endif
}

void AudioVisualizerDesktop::before_exit() {
    ImPlot::DestroyContext();
}

void AudioVisualizerDesktop::post_init() {
    implotContext = ImPlot::CreateContext();
}

void AudioVisualizerDesktop::addConnectionSettings() {
    const bool isProcessingRunning = recorder->isRecording();

    std::shared_lock lock(lastErrorMutex);
    if (!lastError.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Error: %s", lastError.c_str());
    } else {
        ImGui::Text("Status: %s", isProcessingRunning ? "Processing" : "Idle");
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

    ImGui::DragScalar("Number of Bands", ImGuiDataType_U16, &cfg.numBands, 1, &numBandsMin, &numBandsMax, "%d");

    static double gainMin = 0.1;
    static double gainMax = 10.0;
    ImGui::DragScalar("Gain", ImGuiDataType_Double, &cfg.gain, 1, &gainMin, &gainMax, "%.1f");

    static double smoothingMin = 0.0;
    static double smoothingMax = 1.0;
    ImGui::DragScalar("Smoothing", ImGuiDataType_Double, &cfg.smoothing, 1, &smoothingMin, &smoothingMax, "%.2f");

    static double minFreqMin = 20.0;
    static double minFreqMax = 1000.0;
    static double maxFreqMax = 22000.0;

    ImGui::DragScalar("Min Frequency", ImGuiDataType_Double, &cfg.minFreq, 1, &minFreqMin, &minFreqMax, "%.1f Hz");
    ImGui::DragScalar("Max Frequency", ImGuiDataType_Double, &cfg.maxFreq, 1, &cfg.minFreq, &maxFreqMax, "%.1f Hz");
}

void AudioVisualizerDesktop::addDeviceSettings() {
    ImGui::SeparatorText("Audio Device Settings");
    static auto devices = AudioRecorder::Recorder::listDevices();
    if (cfg.deviceName.empty() && !devices.empty()) {
        cfg.deviceName = devices[0].name; // Default to first device
    }

    if (ImGui::BeginCombo("Select Device", cfg.deviceName.empty() ? "None" : cfg.deviceName.c_str())) {
        for (const auto &device: devices) {
            std::string deviceNameWithId = device.name + "##" + std::to_string(device.index);
            if (ImGui::Selectable(deviceNameWithId.c_str())) {
                cfg.deviceName = device.name;
                recorder->stopRecording();
                recorder->startRecording(device.index);
            }

            if (cfg.deviceName == device.name)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::SetItemTooltip("You'll need to reconnect to apply the new device.");
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    if (ImGui::Button("Refresh Devices")) {
        devices = AudioRecorder::Recorder::listDevices();
    }
}

static bool showPreview = false;

void AudioVisualizerDesktop::addVisualizer() {
    ImGui::SeparatorText("Audio Spectrum");
    ImGui::Checkbox("Live Preview (may cause stutters on the LED Matrix)", &showPreview);

    if (!showPreview)
        return;

    if (latestBands.empty()) {
        ImGui::Text("Waiting for audio data...");
        return;
    }

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
        for (const auto idx: defaultIndices) {
            ImPlot::PlotBars("Default", &x[idx], &latestBands[idx], 1, 1);
        }

        // Render yellow bars
        ImPlot::PushStyleColor(ImPlotCol_Fill, IM_COL32(255, 255, 0, 255)); // Yellow
        for (const auto idx: yellowIndices) {
            ImPlot::PlotBars("Yellow", &x[idx], &latestBands[idx], 1, 1);
        }
        ImPlot::PopStyleColor();

        // Render red bars
        ImPlot::PushStyleColor(ImPlotCol_Fill, IM_COL32(255, 0, 0, 255)); // Red
        for (const auto idx: redIndices) {
            ImPlot::PlotBars("Red", &x[idx], &latestBands[idx], 1, 1);
        }
        ImPlot::PopStyleColor();

        ImPlot::EndPlot();
    }
}

void AudioVisualizerDesktop::initialize_imgui(ImGuiContext *im_gui_context, ImGuiMemAllocFunc*alloc_fn,
    ImGuiMemFreeFunc*free_fn, void **user_data) {
    ImGui::SetCurrentContext(im_gui_context);
    ImGui::GetAllocatorFunctions(alloc_fn, free_fn, user_data);
}

std::optional<std::unique_ptr<UdpPacket, void (*)(UdpPacket *)> > AudioVisualizerDesktop::compute_next_packet(
    const std::string sceneName) {
#ifndef _WIN32
    if constexpr (true)
        return std::nullopt;
#else
    if (sceneName != "audio_spectrum")
        return std::nullopt;

    if (!recorder->isRecording())
    {
        auto devices = AudioRecorder::Recorder::listDevices();
        int deviceIndex = -1;
        for (const auto &device : devices)
        {
            if (device.name == cfg.deviceName)
            {
                deviceIndex = device.index;
                break;
            }
        }

        if (deviceIndex == -1)
        {
            std::unique_lock lock(lastErrorMutex);
            lastError = "Device not found: '" + cfg.deviceName + "'. Please select another device in the settings.";
            return std::nullopt;
        }

        lastError.clear();
        recorder->startRecording(deviceIndex);
    }

    auto samplesOpt = recorder->getLastSamples();
    if (!samplesOpt.has_value())
        return std::nullopt;

    auto bands = audioProcessor->computeBands(samplesOpt.value(), recorder->getSampleRate());
    if (bands.empty())
        return std::nullopt;

    if (showPreview)
    {
        std::unique_lock lock(latestBandsMutex);
        latestBands = bands;
    }
    
    // Perform beat detection on the processed bands
    bool beat_detected_now = detect_beat(bands);
    
    // Check and clear beat flag
    bool send_beat_flag = false;
    {
        std::lock_guard<std::mutex> lock(beat_mutex);
        send_beat_flag = beat_detected;
        beat_detected = false; // Clear flag after reading
    }

    bool interpolatedLog = audioProcessor->getInterpolatedLog();
    return std::unique_ptr<UdpPacket, void (*)(UdpPacket *)>(new CompactAudioPacket(bands, interpolatedLog, send_beat_flag),
                                                             [](UdpPacket *packet)
                                                             {
                                                                 delete (CompactAudioPacket *)packet;
                                                             });
#endif
}

bool AudioVisualizerDesktop::detect_beat(const std::vector<float>& audio_data) {
    if (audio_data.empty()) {
        return false;
    }
    
    // Calculate current energy
    float current_energy = calculate_energy(audio_data);
    
    // Add to energy history
    energy_history.push_back(current_energy);
    if (energy_history.size() > 43) { // Same as matrix version
        energy_history.pop_front();
    }
    
    // Need sufficient history to detect beats
    if (energy_history.size() < 20) {
        return false;
    }
    
    // Calculate average energy over history
    float avg_energy = 0.0f;
    for (float energy : energy_history) {
        avg_energy += energy;
    }
    avg_energy /= energy_history.size();
    
    // Check for beat: current energy significantly higher than average
    bool is_beat = current_energy > (avg_energy * 1.5f); // Same threshold as matrix version
    
    // Apply minimum time constraint between beats
    auto now = std::chrono::steady_clock::now();
    auto time_since_last_beat = std::chrono::duration_cast<std::chrono::duration<float>>(now - last_beat_time).count();
    
    if (is_beat && time_since_last_beat >= 0.3f) { // 300ms minimum between beats
        last_beat_time = now;
        {
            std::lock_guard<std::mutex> lock(beat_mutex);
            beat_detected = true;
        }
        spdlog::debug("Beat detected! Energy: {:.2f}, Avg: {:.2f}, Ratio: {:.2f}", 
                     current_energy, avg_energy, current_energy / avg_energy);
        return true;
    }
    
    return false;
}

float AudioVisualizerDesktop::calculate_energy(const std::vector<float>& audio_data) {
    float total_energy = 0.0f;
    
    // Focus on lower frequency bands for beat detection (typically where bass is)
    int focus_bands = std::min(static_cast<int>(audio_data.size()), 16);
    
    for (int i = 0; i < focus_bands; i++) {
        float normalized = audio_data[i];
        total_energy += normalized * normalized; // Square for energy
    }
    
    return total_energy / focus_bands;
}
