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
           name == "audio_kaleidoscope" || name == "music_director";
}
}

AudioVisualizerDesktop::AudioVisualizerDesktop() = default;
AudioVisualizerDesktop::~AudioVisualizerDesktop() {
    if (recorder) recorder->stopRecording();
}

void AudioVisualizerDesktop::render() {
    // Never skip an ImGui frame when the audio worker is updating state. An
    // empty frame changes the child window's content height, which can remove
    // its scrollbar for one frame and visibly resize/flash the whole panel.
    // The worker holds stateMutex only for one audio update, so waiting here
    // keeps the immediate-mode UI structurally stable instead.
    std::lock_guard lock(stateMutex);
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
    if (ImGui::Button("Reset analyzer history") && musicAnalyzer) { musicAnalyzer->reset(); diagnosticHistory.clear(); }

    if (!hasLatestAnalysis) return;
    const auto f = [&](AudioProtocol::Feature id) { return latestAnalysis.feature(id); };
    ImGui::Spacing();
    const float confidence = f(AudioProtocol::Feature::BeatConfidence);
    const float stability = f(AudioProtocol::Feature::TempoStability);
    const float tempoTrust = std::clamp((confidence - 0.28f) / 0.52f, 0.0f, 1.0f) * stability;
    const char *tempoState = tempoTrust >= 0.55f ? "LOCKED" : tempoTrust >= 0.20f ? "TRACKING" : "LEARNING";
    ImGui::Text("Tempo  %.1f BPM", f(AudioProtocol::Feature::Bpm));
    ImGui::SameLine();
    ImGui::TextDisabled("%s  confidence %.0f%%  stability %.0f%%", tempoState, confidence * 100.0f, stability * 100.0f);
    ImGui::ProgressBar(f(AudioProtocol::Feature::BeatPhase), ImVec2(-1, 5), "");
    ImGui::Text("Kick %.2f   Snare %.2f   Hi-hat %.2f",
                f(AudioProtocol::Feature::Kick), f(AudioProtocol::Feature::Snare),
                f(AudioProtocol::Feature::Hihat));
    ImGui::Text("Width %.2f   Balance %+.2f   Brightness %.2f",
                f(AudioProtocol::Feature::StereoWidth), f(AudioProtocol::Feature::StereoBalance),
                f(AudioProtocol::Feature::SpectralCentroid));
    ImGui::Text("Flux %.2f   Energy trend %+.2f   Drop %.2f   Section %.2f",
                f(AudioProtocol::Feature::SpectralFlux), f(AudioProtocol::Feature::EnergyTrend),
                f(AudioProtocol::Feature::Drop), f(AudioProtocol::Feature::SectionChange));
}

static bool showPreview = true;

void AudioVisualizerDesktop::addVisualizer() {
    ImGui::SeparatorText("Live analyzer diagnostics");
    ImGui::Checkbox("Show diagnostics", &showPreview);
    if (!showPreview || !hasLatestAnalysis || latestBands.empty()) return;

    if (recorder) {
        ImGui::Text("Capture buffer  %zu frames  /  %.1f ms",
                    recorder->getBufferedFrameCount(), recorder->getBufferedLatencyMs());
        ImGui::SameLine();
        ImGui::TextDisabled("hop %.1f ms", 1000.0 * FFT_HOP_SIZE / std::max(1.0, recorder->getSampleRate()));
    }

    if (ImPlot::BeginPlot("Spectrum##music_debug", ImVec2(-1, 180),
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

    if (!latestAnalysis.waveform.empty() && ImPlot::BeginPlot("Waveform##music_debug", ImVec2(-1, 120),
        ImPlotFlags_NoLegend | ImPlotFlags_NoMouseText | ImPlotFlags_NoMenus)) {
        ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);
        ImPlot::SetupAxisLimits(ImAxis_X1, 0.0, static_cast<double>(latestAnalysis.waveform.size()), ImGuiCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, -1.0, 1.0, ImGuiCond_Always);
        ImPlot::PlotLine("Waveform", latestAnalysis.waveform.data(), static_cast<int>(latestAnalysis.waveform.size()));
        ImPlot::EndPlot();
    }

    for (size_t i = 0; i < MusicalBandFeatures.size(); ++i) {
        const float value = latestAnalysis.feature(MusicalBandFeatures[i]);
        ImGui::TextUnformatted(MusicalBandNames[i]);
        ImGui::SameLine(70.0f);
        ImGui::ProgressBar(value, ImVec2(-1, 10), "");
    }

    if (diagnosticHistory.size() >= 2) {
        const size_t n = diagnosticHistory.size();
        std::vector<float> x(n), loudness(n), kick(n), snare(n), hihat(n), onset(n), bpm(n), confidence(n), stability(n), width(n), balance(n);
        for (size_t i = 0; i < n; ++i) {
            const auto &d = diagnosticHistory[i];
            x[i] = d.time; loudness[i] = d.loudness; kick[i] = d.kick; snare[i] = d.snare;
            hihat[i] = d.hihat; onset[i] = d.onset; bpm[i] = d.bpm; confidence[i] = d.beatConfidence;
            stability[i] = d.tempoStability; width[i] = d.stereoWidth; balance[i] = d.stereoBalance;
        }

        const double xmin = x.front(), xmax = std::max<double>(x.back(), x.front() + 0.001f);
        if (ImPlot::BeginPlot("Dynamics / transients##music_debug", ImVec2(-1, 160), ImPlotFlags_NoMenus)) {
            ImPlot::SetupAxes("seconds", "strength", ImPlotAxisFlags_None, ImPlotAxisFlags_AutoFit);
            ImPlot::SetupAxisLimits(ImAxis_X1, xmin, xmax, ImGuiCond_Always);
            ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, 1.05, ImGuiCond_Always);
            ImPlot::PlotLine("Loudness", x.data(), loudness.data(), static_cast<int>(n));
            ImPlot::PlotLine("Kick", x.data(), kick.data(), static_cast<int>(n));
            ImPlot::PlotLine("Snare", x.data(), snare.data(), static_cast<int>(n));
            ImPlot::PlotLine("Hi-hat", x.data(), hihat.data(), static_cast<int>(n));
            ImPlot::PlotLine("Onset", x.data(), onset.data(), static_cast<int>(n));
            ImPlot::EndPlot();
        }
        if (ImPlot::BeginPlot("Tempo tracking##music_debug", ImVec2(-1, 145), ImPlotFlags_NoMenus)) {
            ImPlot::SetupAxes("seconds", "BPM", ImPlotAxisFlags_None, ImPlotAxisFlags_AutoFit);
            ImPlot::SetupAxisLimits(ImAxis_X1, xmin, xmax, ImGuiCond_Always);
            ImPlot::PlotLine("BPM", x.data(), bpm.data(), static_cast<int>(n));
            ImPlot::EndPlot();
        }
        if (ImPlot::BeginPlot("Tempo quality##music_debug", ImVec2(-1, 125), ImPlotFlags_NoMenus)) {
            ImPlot::SetupAxes("seconds", "quality", ImPlotAxisFlags_None, ImPlotAxisFlags_AutoFit);
            ImPlot::SetupAxisLimits(ImAxis_X1, xmin, xmax, ImGuiCond_Always);
            ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, 1.05, ImGuiCond_Always);
            ImPlot::PlotLine("Confidence", x.data(), confidence.data(), static_cast<int>(n));
            ImPlot::PlotLine("Stability", x.data(), stability.data(), static_cast<int>(n));
            ImPlot::EndPlot();
        }
        if (ImPlot::BeginPlot("Stereo field##music_debug", ImVec2(-1, 145), ImPlotFlags_NoMenus)) {
            ImPlot::SetupAxes("seconds", nullptr, ImPlotAxisFlags_None, ImPlotAxisFlags_AutoFit);
            ImPlot::SetupAxisLimits(ImAxis_X1, xmin, xmax, ImGuiCond_Always);
            ImPlot::SetupAxisLimits(ImAxis_Y1, -1.05, 1.05, ImGuiCond_Always);
            ImPlot::PlotLine("Balance", x.data(), balance.data(), static_cast<int>(n));
            ImPlot::PlotLine("Width", x.data(), width.data(), static_cast<int>(n));
            ImPlot::EndPlot();
        }
        ImGui::ProgressBar(latestAnalysis.feature(AudioProtocol::Feature::BeatConfidence), ImVec2(-1, 8), "beat confidence");
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
        diagnosticHistory.clear();
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
    DiagnosticSample sample;
    sample.time = static_cast<float>(analysis.timestamp_ms) / 1000.0f;
    sample.loudness = analysis.feature(AudioProtocol::Feature::LoudnessFast);
    sample.kick = analysis.feature(AudioProtocol::Feature::Kick);
    sample.snare = analysis.feature(AudioProtocol::Feature::Snare);
    sample.hihat = analysis.feature(AudioProtocol::Feature::Hihat);
    sample.onset = analysis.feature(AudioProtocol::Feature::OnsetStrength);
    sample.bpm = analysis.feature(AudioProtocol::Feature::Bpm);
    sample.beatConfidence = analysis.feature(AudioProtocol::Feature::BeatConfidence);
    sample.tempoStability = analysis.feature(AudioProtocol::Feature::TempoStability);
    sample.stereoWidth = analysis.feature(AudioProtocol::Feature::StereoWidth);
    sample.stereoBalance = analysis.feature(AudioProtocol::Feature::StereoBalance);
    diagnosticHistory.push_back(sample);
    while (diagnosticHistory.size() > MaxDiagnosticHistory) diagnosticHistory.pop_front();
    return std::make_unique<MusicAnalysisPacket>(std::move(analysis));
}
