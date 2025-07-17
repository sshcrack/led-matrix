#include "AudioVisualizerDesktop.h"
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
#include "imgui_stdlib.h"
#include <portaudio.h>

extern "C" PLUGIN_EXPORT AudioVisualizerDesktop *createAudioVisualizer()
{
    return new AudioVisualizerDesktop();
}

extern "C" PLUGIN_EXPORT void destroyAudioVisualizer(AudioVisualizerDesktop *c)
{
    delete c;
}

AudioVisualizerDesktop::AudioVisualizerDesktop()
{
    Pa_Initialize();
    implotContext = ImPlot::CreateContext();
    recorder = new AudioRecorder::Recorder();
}

AudioVisualizerDesktop::~AudioVisualizerDesktop()
{
    Pa_Terminate();
    ImPlot::DestroyContext();
    delete recorder;
}

int PortFilter(ImGuiInputTextCallbackData *data)
{
    if (data->EventChar < 32 || data->EventChar >= 127)
        return 1; // Disallow non-printable

    if (data->BufTextLen > 5)
    {
        return 1; // Limit to 5 characters
    }

    char c = static_cast<char>(data->EventChar);
    if (c >= '0' && c <= '9')
    {
        return 0; // Allow
    }

    return 1; // Block everything else
}

void AudioVisualizerDesktop::render(ImGuiContext *ctx)
{
    ImGui::SetCurrentContext(ctx);
    ImPlot::SetCurrentContext(implotContext);

    addConnectionSettings();
    addAudioSettings();
    addAnalysisSettings();
    addDeviceSettings();
    // Process audio if connected
    if (status == ConnectionStatus::Connected && audioProcessor && analyzer)
    {
        // Get real audio samples from recorder
        std::vector<float> samples = recorder->getLatestSamples(1024);
        audioProcessor->processAudio(samples);

        // Compute bands using analyzer
        float freqResolution = static_cast<float>(recorder->getSampleRate()) / AudioProcessor::FFT_SIZE;
        size_t minBin = static_cast<size_t>(cfg.minFreq / freqResolution);
        size_t maxBin = std::min(static_cast<size_t>(cfg.maxFreq / freqResolution), audioProcessor->getBands().size() - 1);
        latestBands = analyzer->computeBands(audioProcessor->getBands(), cfg, freqResolution, minBin, maxBin);
    }
    addSpectrumSettings();
}

void AudioVisualizerDesktop::addConnectionSettings()
{
    ImGui::SeparatorText("Connection Settings");

    static std::string port = std::to_string(cfg.port);
    if (ImGui::InputText("Port", &port, ImGuiInputTextFlags_CallbackCharFilter, PortFilter))
    {
        try
        {
            cfg.port = std::stoi(port);
        }
        catch (const std::exception &e)
        {
            std::cerr << "Invalid port number: " << e.what() << std::endl;
        }
    }

    std::string currStatus = to_string(status);
    ImGui::Text("Status: %s", currStatus.c_str());

    std::string buttonText = status == ConnectionStatus::Connected ? "Disconnect" : "Connect";
    if (ImGui::Button(buttonText.c_str()))
    {
        if (status == ConnectionStatus::Connected)
        {
            recorder->stopRecording();
            status = ConnectionStatus::Disconnected;
        }
        else
        {
            // Find device index
            auto devices = recorder->listDevices();
            int deviceIndex = -1;
            for (const auto &device : devices)
            {
                if (device.name == cfg.deviceName)
                {
                    deviceIndex = device.index;
                    break;
                }
            }
            if (deviceIndex != -1 && recorder->startRecording(deviceIndex))
            {
                status = ConnectionStatus::Connected;
                // Setup audio processor and analyzer
                audioProcessor = std::make_unique<AudioProcessor>(cfg, static_cast<uint32_t>(recorder->getSampleRate()));
                analyzer = getAnalyzer(cfg.analysisMode, cfg.frequencyScale);
            }
            else
            {
                status = ConnectionStatus::Error;
            }
        }
    }
}

void AudioVisualizerDesktop::addAnalysisSettings()
{

    ImGui::SeparatorText("Analysis Settings");

    static int selectedModeIdx = static_cast<int>(cfg.analysisMode);
    std::string modePreview = analysisModes[selectedModeIdx];
    if (ImGui::BeginCombo("Mode", modePreview.c_str()))
    {
        for (int n = 0; n < analysisModes.size(); n++)
        {
            const bool is_selected = (selectedModeIdx == n);
            if (ImGui::Selectable(analysisModes[n].c_str(), is_selected))
            {
                cfg.analysisMode = static_cast<AnalysisMode>(n);
                selectedModeIdx = n;
            }

            // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    static int selectedFreqIdx = static_cast<int>(cfg.frequencyScale);
    std::string frequencyPreview = frequencyScales[selectedFreqIdx];
    if (ImGui::BeginCombo("Frequency Scale", frequencyPreview.c_str()))
    {
        for (int n = 0; n < frequencyScales.size(); n++)
        {
            const bool is_selected = (selectedFreqIdx == n);
            if (ImGui::Selectable(frequencyScales[n].c_str(), is_selected))
            {
                cfg.frequencyScale = static_cast<FrequencyScale>(n);
                selectedFreqIdx = n;
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

void AudioVisualizerDesktop::addAudioSettings()
{
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

void AudioVisualizerDesktop::addDeviceSettings()
{
    ImGui::SeparatorText("Audio Device Settings");
    static auto devices = recorder->listDevices();
    if (cfg.deviceName.empty() && !devices.empty())
    {
        cfg.deviceName = devices[0].name; // Default to first device
    }

    if (ImGui::BeginCombo("Select Device", cfg.deviceName.empty() ? "None" : cfg.deviceName.c_str()))
    {
        for (const auto &device : devices)
        {
            std::string deviceName = device.name;
            if (ImGui::Selectable(deviceName.c_str()))
                cfg.deviceName = deviceName;

            if (cfg.deviceName == deviceName)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    if (ImGui::Button("Refresh Devices"))
    {
        devices = recorder->listDevices();
    }
}

void AudioVisualizerDesktop::addSpectrumSettings()
{
    ImGui::SeparatorText("Audio Spectrum");
    if (!latestBands.empty())
    {
        ImPlot::SetNextAxesLimits(ImAxis_X1, 0.0, cfg.numBands, ImPlotCond_Always);
        ImPlot::SetNextAxisLimits(ImAxis_Y1, 0.0, 1.0, ImPlotCond_Always);
        if (ImPlot::BeginPlot("Spectrum", ImVec2(-1,0), ImPlotFlags_NoLegend | ImPlotFlags_NoMouseText | ImPlotAxisFlags_Lock | ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_NoMenus | ImPlotAxisFlags_NoDecorations))
        {
            std::vector<float> x(latestBands.size());
            for (size_t i = 0; i < x.size(); ++i)
                x[i] = static_cast<float>(i);

            ImPlot::PlotBars("Bands", x.data(), latestBands.data(), static_cast<int>(latestBands.size()), 1);
            ImPlot::EndPlot();
        }
    }
    else
    {
        ImGui::Text("No spectrum data available.");
    }
}
