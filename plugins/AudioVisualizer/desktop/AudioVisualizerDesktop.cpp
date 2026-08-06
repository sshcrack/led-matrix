#include "AudioVisualizerDesktop.h"
#include "udpBandsPacket.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <spdlog/spdlog.h>

REGISTER_PLUGIN(AudioVisualizer, AudioVisualizerDesktop)

namespace {
constexpr std::array<const char *, 7> MusicalBandNames{
    "Sub", "Bass", "Low mid", "Mid", "High mid", "Treble", "Air"};
constexpr double GainMin = 0.1;
constexpr double GainMax = 10.0;
constexpr double SmoothMin = 0.0;
constexpr double SmoothMax = 0.97;
constexpr double MinFrequencyMin = 20.0;
constexpr double MinFrequencyMax = 1000.0;
constexpr double MaxFrequencyMax = 22000.0;
constexpr double AnalysisGainMin = 0.25;
constexpr double AnalysisGainMax = 4.0;
constexpr double SensitivityMin = 0.35;
constexpr double SensitivityMax = 2.5;
constexpr double WaveformGainMin = 0.5;
constexpr double WaveformGainMax = 8.0;

constexpr std::array<AudioProtocol::Feature, 7> MusicalBandFeatures{
    AudioProtocol::Feature::SubBass, AudioProtocol::Feature::Bass,
    AudioProtocol::Feature::LowMid, AudioProtocol::Feature::Mid,
    AudioProtocol::Feature::HighMid, AudioProtocol::Feature::Treble,
    AudioProtocol::Feature::Air};

bool isDedicatedAudioScene(const std::string &name) {
    return name == "audio_spectrum" || name == "audio_particles" ||
           name == "audio_pulse_tunnel" || name == "audio_aurora" ||
           name == "audio_kaleidoscope";
}
}

AudioVisualizerDesktop::AudioVisualizerDesktop() = default;
AudioVisualizerDesktop::~AudioVisualizerDesktop() = default;

void AudioVisualizerDesktop::render() {
    if (!stateMutex.try_lock()) return;
    std::lock_guard lock(stateMutex, std::adopt_lock);
    ImPlot::SetCurrentContext(implotContext);
    addConnectionSettings();
    addDeviceSettings();
    addAudioSettings();
    addAnalysisSettings();
    addMusicAnalysisSettings();
    addVisualizer();
}

void AudioVisualizerDesktop::load_config(std::optional<const nlohmann::json> config) {
    if (config.has_value() && !config->is_null()) cfg = *config;
    if (!audioProcessor) audioProcessor = std::make_unique<AudioProcessor>(cfg);
    if (!musicAnalyzer) musicAnalyzer = std::make_unique<MusicAnalyzer>(cfg);
    if (!recorder) recorder = std::make_unique<AudioRecorder::Recorder>();
}

void AudioVisualizerDesktop::before_exit() {
    if (recorder) recorder->stopRecording();
    if (implotContext) {
        ImPlot::DestroyContext(implotContext);
        implotContext = nullptr;
    }
}

void AudioVisualizerDesktop::post_init() { implotContext = ImPlot::CreateContext(); }

void AudioVisualizerDesktop::addConnectionSettings() {
    const bool running = recorder && recorder->isRecording();
    std::shared_lock lock(lastErrorMutex);
    if (!lastError.empty())
        ImGui::TextColored(ImVec4(1.0f, 0.22f, 0.22f, 1.0f), "Error: %s", lastError.c_str());
    else
        ImGui::TextColored(running ? ImVec4(0.25f, 1.0f, 0.50f, 1.0f)
                                  : ImVec4(0.75f, 0.75f, 0.75f, 1.0f),
                           "%s", running ? "Streaming deep music analysis" : "Waiting for an active matrix connection");
}

void AudioVisualizerDesktop::addAnalysisSettings() {
    ImGui::SeparatorText("Display spectrum");
    const int currentMode = static_cast<int>(cfg.analysisMode);
    if (ImGui::BeginCombo("Band layout", analysisModes[currentMode].c_str())) {
        for (int i = 0; i < static_cast<int>(analysisModes.size()); ++i) {
            const bool selected = i == currentMode;
            if (ImGui::Selectable(analysisModes[i].c_str(), selected)) {
                cfg.analysisMode = static_cast<AnalysisMode>(i);
                audioProcessor->updateAnalyzer();
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    const int currentScale = static_cast<int>(cfg.frequencyScale);
    if (ImGui::BeginCombo("Frequency scale", frequencyScales[currentScale].c_str())) {
        for (int i = 0; i < static_cast<int>(frequencyScales.size()); ++i) {
            const bool selected = i == currentScale;
            if (ImGui::Selectable(frequencyScales[i].c_str(), selected)) {
                cfg.frequencyScale = static_cast<FrequencyScale>(i);
                audioProcessor->updateAnalyzer();
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::SliderInt("Spectrum bins", &cfg.numBands, 16, 128);
    ImGui::SliderScalar("Spectrum gain", ImGuiDataType_Double, &cfg.gain,
                        &GainMin, &GainMax, "%.2fx");
    ImGui::SliderScalar("Spectrum smoothing", ImGuiDataType_Double, &cfg.smoothing,
                        &SmoothMin, &SmoothMax, "%.2f");
    ImGui::Checkbox("Interpolate sparse logarithmic bins", &cfg.interpolateMissingBands);
}

void AudioVisualizerDesktop::addAudioSettings() {
    ImGui::SeparatorText("Frequency range");
    ImGui::SliderScalar("Minimum", ImGuiDataType_Double, &cfg.minFreq,
                        &MinFrequencyMin, &MinFrequencyMax, "%.0f Hz");
    ImGui::SliderScalar("Maximum", ImGuiDataType_Double, &cfg.maxFreq,
                        &cfg.minFreq, &MaxFrequencyMax, "%.0f Hz");
}

void AudioVisualizerDesktop::addDeviceSettings() {
    ImGui::SeparatorText("Audio source");
    static auto devices = AudioRecorder::Recorder::listDevices();
    if (cfg.deviceName.empty()) {
        if (AudioRecorder::Recorder::isDefaultOutputLoopbackAvailable())
            cfg.deviceName = DEFAULT_LOOPBACK_DEVICE_NAME;
        else if (!devices.empty())
            cfg.deviceName = devices.front().name;
    }

    if (ImGui::BeginCombo("Capture device", cfg.deviceName.empty() ? "None" : cfg.deviceName.c_str())) {
        if (AudioRecorder::Recorder::isDefaultOutputLoopbackAvailable()) {
            const bool selected = cfg.deviceName == DEFAULT_LOOPBACK_DEVICE_NAME;
            if (ImGui::Selectable(DEFAULT_LOOPBACK_DEVICE_NAME.c_str(), selected))
                cfg.deviceName = DEFAULT_LOOPBACK_DEVICE_NAME;
            if (selected) ImGui::SetItemDefaultFocus();
            ImGui::Separator();
        }
        for (const auto &device : devices) {
            const bool selected = cfg.deviceName == device.name;
            const std::string label = device.name + "##" + std::to_string(device.index);
            if (ImGui::Selectable(label.c_str(), selected)) cfg.deviceName = device.name;
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh")) devices = AudioRecorder::Recorder::listDevices();
    ImGui::Checkbox("Analyze continuously", &cfg.streamContinuously);
    ImGui::SetItemTooltip("Required for audio-reactive modes inside non-audio scenes such as Boids or Starfield.");
}

void AudioVisualizerDesktop::addMusicAnalysisSettings() {
    ImGui::SeparatorText("Musical intelligence");
    ImGui::SliderScalar("Analysis input gain", ImGuiDataType_Double, &cfg.musicAnalysisGain,
                        &AnalysisGainMin, &AnalysisGainMax, "%.2fx");
    ImGui::SliderScalar("Transient sensitivity", ImGuiDataType_Double, &cfg.transientSensitivity,
                        &SensitivityMin, &SensitivityMax, "%.2f");
    ImGui::SliderScalar("Beat sensitivity", ImGuiDataType_Double, &cfg.beatSensitivity,
                        &SensitivityMin, &SensitivityMax, "%.2f");
    ImGui::SliderScalar("Waveform gain", ImGuiDataType_Double, &cfg.waveformGain,
                        &WaveformGainMin, &WaveformGainMax, "%.2fx");
    if (ImGui::Button("Reset analyzer history") && musicAnalyzer) musicAnalyzer->reset();

    if (!hasLatestAnalysis) return;
    const auto f = [&](AudioProtocol::Feature id) { return latestAnalysis.feature(id); };
    ImGui::Spacing();
    ImGui::Text("Tempo  %.1f BPM", f(AudioProtocol::Feature::Bpm));
    ImGui::SameLine();
    ImGui::TextDisabled("confidence %.0f%%", f(AudioProtocol::Feature::BeatConfidence) * 100.0f);
    ImGui::ProgressBar(f(AudioProtocol::Feature::BeatPhase), ImVec2(-1, 5), "");
    ImGui::Text("Kick %.2f   Snare %.2f   Hi-hat %.2f",
                f(AudioProtocol::Feature::Kick), f(AudioProtocol::Feature::Snare),
                f(AudioProtocol::Feature::Hihat));
    ImGui::Text("Width %.2f   Balance %+.2f   Brightness %.2f",
                f(AudioProtocol::Feature::StereoWidth), f(AudioProtocol::Feature::StereoBalance),
                f(AudioProtocol::Feature::SpectralCentroid));
}

static bool showPreview = true;

void AudioVisualizerDesktop::addVisualizer() {
    ImGui::SeparatorText("Live analysis");
    ImGui::Checkbox("Show diagnostics", &showPreview);
    if (!showPreview || !hasLatestAnalysis || latestBands.empty()) return;

    if (ImPlot::BeginPlot("##spectrum", ImVec2(-1, 190),
                          ImPlotFlags_NoLegend | ImPlotFlags_NoMouseText | ImPlotFlags_NoMenus)) {
        ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);
        ImPlot::SetupAxisLimits(ImAxis_X1, 0.0, static_cast<double>(latestBands.size()), ImGuiCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, 1.0, ImGuiCond_Always);
        static std::vector<float> x;
        if (x.size() != latestBands.size()) {
            x.resize(latestBands.size());
            for (size_t i = 0; i < x.size(); ++i) x[i] = static_cast<float>(i);
        }
        ImPlot::PlotBars("Spectrum", x.data(), latestBands.data(), static_cast<int>(latestBands.size()), 0.9);
        ImPlot::EndPlot();
    }

    for (size_t i = 0; i < MusicalBandFeatures.size(); ++i) {
        const float value = latestAnalysis.feature(MusicalBandFeatures[i]);
        ImGui::TextUnformatted(MusicalBandNames[i]);
        ImGui::SameLine(70.0f);
        ImGui::ProgressBar(value, ImVec2(-1, 10), "");
    }
}

void AudioVisualizerDesktop::initialize_imgui(ImGuiContext *context, ImGuiMemAllocFunc *alloc,
                                               ImGuiMemFreeFunc *free, void **userData) {
    ImGui::SetCurrentContext(context);
    ImGui::GetAllocatorFunctions(alloc, free, userData);
}

std::optional<std::unique_ptr<UdpPacket>> AudioVisualizerDesktop::compute_next_packet(
    const std::string sceneName) {
    if (!cfg.streamContinuously && !isDedicatedAudioScene(sceneName)) return std::nullopt;
    std::lock_guard stateLock(stateMutex);

    if (cfg.deviceName != currentDeviceName) {
        if (recorder) recorder->stopRecording();
        currentDeviceName = cfg.deviceName;
        if (musicAnalyzer) musicAnalyzer->reset();
    }

#ifdef _WIN32
    static int cachedLoopbackIndex = -2;
    static auto lastLoopbackCheck = std::chrono::steady_clock::time_point::min();
    if (cfg.deviceName == DEFAULT_LOOPBACK_DEVICE_NAME) {
        const auto now = std::chrono::steady_clock::now();
        if (cachedLoopbackIndex == -2 || now - lastLoopbackCheck > std::chrono::seconds(2)) {
            cachedLoopbackIndex = AudioRecorder::Recorder::getDefaultOutputLoopbackIndex();
            lastLoopbackCheck = now;
        }
        if (recorder->isRecording() && cachedLoopbackIndex >= 0 &&
            recorder->getCurrentDeviceIndex() != cachedLoopbackIndex)
            recorder->stopRecording();
    }
#endif

    if (!recorder->isRecording()) {
        bool started = false;
        if (cfg.deviceName == DEFAULT_LOOPBACK_DEVICE_NAME) {
#ifdef _WIN32
            const int index = AudioRecorder::Recorder::getDefaultOutputLoopbackIndex();
            started = index >= 0 && recorder->startRecording(index);
#else
            started = recorder->startDefaultOutputLoopback();
#endif
        } else {
            const auto devices = AudioRecorder::Recorder::listDevices();
            const auto found = std::find_if(devices.begin(), devices.end(), [&](const auto &device) {
                return device.name == cfg.deviceName;
            });
            started = found != devices.end() && recorder->startRecording(found->index);
        }
        if (!started) {
            std::unique_lock lock(lastErrorMutex);
            lastError = "Could not start audio capture for '" + cfg.deviceName + "'.";
            return std::nullopt;
        }
        std::unique_lock lock(lastErrorMutex);
        lastError.clear();
    }

    const auto captured = recorder->getLastSamples();
    if (!captured) return std::nullopt;
    auto bands = audioProcessor->computeBands(captured->mono, captured->sampleRate);
    if (bands.empty()) return std::nullopt;
    auto analysis = musicAnalyzer->analyze(*captured, bands);

    latestBands = bands;
    latestAnalysis = analysis;
    hasLatestAnalysis = true;
    return std::make_unique<MusicAnalysisPacket>(std::move(analysis));
}
