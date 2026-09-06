#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <shadertoy/PipelineEditor.hpp>
#include <shadertoy/ShaderToyContext.hpp>
#include <stdexcept>
#include <string>

namespace {
constexpr float Pi = 3.14159265358979323846f;

std::string read_file(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("Could not open shader: " + path.string());
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

ShaderToy::AudioInput synthetic_audio(const int frame, const float fps, const float bpm, const std::string& profile)
{
    ShaderToy::AudioInput audio;
    audio.available = true;
    audio.silence = false;
    audio.sampleRate = 48000.0f;
    audio.bpm = std::clamp(bpm, 40.0f, 240.0f);

    const float seconds = static_cast<float>(frame) / fps;
    const float beats = seconds * audio.bpm / 60.0f;
    audio.beatPhase = beats - std::floor(beats);
    audio.beatConfidence = 0.96f;
    audio.beatStrength = std::exp(-7.0f * audio.beatPhase);
    audio.kick = audio.beatStrength;
    const float halfBeat = std::fabs(audio.beatPhase - 0.5f);
    audio.snare = std::exp(-45.0f * halfBeat * halfBeat);
    audio.hihat = 0.25f + 0.55f * std::pow(std::max(0.0f, std::sin(beats * Pi * 4.0f)), 8.0f);
    audio.onset = std::max({audio.kick, audio.snare, audio.hihat * 0.65f});

    float low_gain = 1.0f;
    float mid_gain = 1.0f;
    float high_gain = 1.0f;
    float transient_gain = 1.0f;
    if (profile == "bass") {
        low_gain = 1.28f;
        mid_gain = 0.82f;
        high_gain = 0.58f;
        transient_gain = 0.90f;
    }
    else if (profile == "percussion") {
        low_gain = 1.08f;
        mid_gain = 0.92f;
        high_gain = 1.18f;
        transient_gain = 1.30f;
    }
    else if (profile == "ambient") {
        low_gain = 0.78f;
        mid_gain = 1.05f;
        high_gain = 0.82f;
        transient_gain = 0.45f;
        audio.kick *= 0.35f;
        audio.beatStrength *= 0.55f;
    }

    const float groove = 0.5f + 0.5f * std::sin(seconds * (profile == "ambient" ? 0.75f : 1.7f));
    const float shimmer = 0.5f + 0.5f * std::sin(seconds * (profile == "ambient" ? 1.25f : 4.1f) + 0.7f);
    audio.bass = std::clamp((0.32f + 0.58f * audio.kick) * low_gain, 0.0f, 1.0f);
    audio.mid = std::clamp((0.34f + 0.20f * groove + 0.20f * audio.snare) * mid_gain, 0.0f, 1.0f);
    audio.treble = std::clamp((0.30f + 0.32f * audio.hihat) * high_gain, 0.0f, 1.0f);
    audio.loudness = std::clamp(0.35f + 0.35f * audio.bass + 0.20f * audio.mid, 0.0f, 1.0f);
    audio.onset = std::clamp(audio.onset * transient_gain, 0.0f, 1.0f);
    audio.kick = std::clamp(audio.kick * transient_gain, 0.0f, 1.0f);
    audio.snare = std::clamp(audio.snare * transient_gain, 0.0f, 1.0f);
    audio.hihat = std::clamp(audio.hihat * high_gain * transient_gain, 0.0f, 1.0f);
    audio.stereoWidth = 0.62f;
    audio.stereoBalance = 0.08f * std::sin(seconds * 0.7f);
    audio.stereoCorrelation = 0.55f;
    audio.energyTrend = 0.5f + 0.3f * std::sin(seconds * 0.22f);
    audio.drop = (frame % std::max(1, static_cast<int>(fps * 8.0f))) < 8 ? 0.8f : 0.0f;
    audio.sectionChange = (frame % std::max(1, static_cast<int>(fps * 16.0f))) < 8 ? 0.8f : 0.0f;
    audio.spectralCentroid = 0.42f + 0.18f * std::sin(seconds * 0.33f);
    audio.spectralFlux = audio.onset;

    audio.spectrum.resize(256);
    for (std::size_t i = 0; i < audio.spectrum.size(); ++i) {
        const float x = static_cast<float>(i) / static_cast<float>(audio.spectrum.size() - 1);
        const float bass_peak = std::exp(-55.0f * (x - 0.10f) * (x - 0.10f)) * audio.bass;
        const float mid_peak = std::exp(-36.0f * (x - 0.42f) * (x - 0.42f)) * audio.mid;
        const float high_peak = std::exp(-28.0f * (x - 0.78f) * (x - 0.78f)) * audio.treble;
        const float ripple = 0.05f * (1.0f + std::sin(38.0f * x + seconds * 2.8f));
        audio.spectrum[i] = std::clamp(0.03f + ripple + 0.65f * bass_peak + 0.45f * mid_peak + 0.32f * high_peak, 0.0f, 1.0f);
    }

    audio.waveform.resize(256);
    for (std::size_t i = 0; i < audio.waveform.size(); ++i) {
        const float x = static_cast<float>(i) / static_cast<float>(audio.waveform.size());
        audio.waveform[i] = std::clamp(
            0.58f * std::sin(2.0f * Pi * (3.0f * x + seconds * 0.8f)) + 0.24f * std::sin(2.0f * Pi * (7.0f * x - seconds * 0.43f)), -1.0f,
            1.0f);
    }
    return audio;
}

class GlfwGuard {
public:
    GlfwGuard(const int width, const int height)
    {
        glfwSetErrorCallback([](int, const char* message) {
            if (message)
                std::cerr << "GLFW: " << message << '\n';
        });
        if (!glfwInit())
            throw std::runtime_error("Could not initialize GLFW");
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        window_ = glfwCreateWindow(width, height, "led-matrix-shadertoy-preview", nullptr, nullptr);
        if (!window_)
            throw std::runtime_error("Could not create hidden OpenGL preview context");
        glfwMakeContextCurrent(window_);
        glewExperimental = GL_TRUE;
        if (glewInit() != GLEW_OK)
            throw std::runtime_error("Could not initialize GLEW");
        glGetError();
    }

    ~GlfwGuard()
    {
        if (window_)
            glfwDestroyWindow(window_);
        glfwTerminate();
    }

private:
    GLFWwindow* window_ = nullptr;
};

}  // namespace

int main(int argc, char** argv)
{
    if (argc != 9) {
        std::cerr << "Usage: shadertoy_scene_preview_worker <shader> <output.rgb> <width> <height> <frames> <fps> <bpm> <profile>\n";
        return 2;
    }

    try {
        const std::filesystem::path shader = argv[1];
        const std::filesystem::path output = argv[2];
        const int width = std::stoi(argv[3]);
        const int height = std::stoi(argv[4]);
        const int frames = std::stoi(argv[5]);
        const int fps = std::stoi(argv[6]);
        const float bpm = std::stof(argv[7]);
        const std::string profile = argv[8];
        if (width <= 0 || height <= 0 || frames <= 0 || fps <= 0)
            throw std::runtime_error("Preview dimensions, frame count, and FPS must be positive");

        GlfwGuard glfw(width, height);
        auto& editor = ShaderToy::PipelineEditor::get();
        auto load = editor.loadImageShader(shader.stem().string(), read_file(shader), 0);
        if (!load)
            throw load.error();

        ShaderToy::ShaderToyContext context;
        auto build = editor.build(context);
        if (!build)
            throw build.error();

        std::ofstream stream(output, std::ios::binary | std::ios::trunc);
        if (!stream)
            throw std::runtime_error("Could not open RGB preview output: " + output.string());
        const float fixed_fps = static_cast<float>(fps);
        const float dt = 1.0f / fixed_fps;
        for (int frame = 0; frame < frames; ++frame) {
            context.setAudioInput(synthetic_audio(frame, fixed_fps, bpm, profile));
            context.tickFixed(dt, fixed_fps);
            const auto rgb = context.renderToBuffer(ImVec2(static_cast<float>(width), static_cast<float>(height)));
            const auto expected = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3U;
            if (rgb.size() != expected)
                throw std::runtime_error("Renderer returned an unexpected RGB buffer size");
            stream.write(reinterpret_cast<const char*>(rgb.data()), static_cast<std::streamsize>(rgb.size()));
            if (!stream)
                throw std::runtime_error("Failed while writing RGB preview output");
        }
    }
    catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 5;
    }
    return 0;
}
