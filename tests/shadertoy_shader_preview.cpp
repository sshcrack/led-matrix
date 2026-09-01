// Consumer-side preview harness derived from shadertoy-headless tools/shadertoy-preview.cpp.
// Keep this tiny executable buildable with the exact renderer version consumed by LED Matrix.
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <shadertoy/PipelineEditor.hpp>
#include <shadertoy/ShaderToyContext.hpp>

#include <stb_image_write.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace {
constexpr float Pi = 3.14159265358979323846f;

std::string readFile(const std::filesystem::path &path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Could not open shader: " + path.string());
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

ShaderToy::AudioInput syntheticAudio(const int frame, const float fps) {
    ShaderToy::AudioInput audio;
    audio.available = true;
    audio.silence = false;
    audio.sampleRate = 48000.0f;
    audio.bpm = 120.0f;

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

    audio.bass = std::clamp(0.32f + 0.58f * audio.kick + 0.10f * std::sin(seconds * 2.1f), 0.0f, 1.0f);
    audio.mid = std::clamp(0.34f + 0.18f * std::sin(seconds * 3.7f + 0.8f) + 0.22f * audio.snare, 0.0f, 1.0f);
    audio.treble = std::clamp(0.30f + 0.32f * audio.hihat, 0.0f, 1.0f);
    audio.loudness = std::clamp(0.35f + 0.35f * audio.bass + 0.20f * audio.mid, 0.0f, 1.0f);
    audio.stereoWidth = 0.62f;
    audio.stereoBalance = 0.08f * std::sin(seconds * 0.7f);
    audio.stereoCorrelation = 0.55f;
    audio.energyTrend = 0.5f + 0.3f * std::sin(seconds * 0.22f);
    audio.drop = (frame % static_cast<int>(fps * 8.0f)) < 8 ? 0.8f : 0.0f;
    audio.sectionChange = (frame % static_cast<int>(fps * 16.0f)) < 8 ? 0.8f : 0.0f;
    audio.spectralCentroid = 0.42f + 0.18f * std::sin(seconds * 0.33f);
    audio.spectralFlux = audio.onset;

    audio.spectrum.resize(256);
    for (std::size_t i = 0; i < audio.spectrum.size(); ++i) {
        const float x = static_cast<float>(i) / static_cast<float>(audio.spectrum.size() - 1);
        const float bassPeak = std::exp(-55.0f * (x - 0.10f) * (x - 0.10f)) * audio.bass;
        const float midPeak = std::exp(-36.0f * (x - 0.42f) * (x - 0.42f)) * audio.mid;
        const float highPeak = std::exp(-28.0f * (x - 0.78f) * (x - 0.78f)) * audio.treble;
        const float ripple = 0.05f * (1.0f + std::sin(38.0f * x + seconds * 2.8f));
        audio.spectrum[i] = std::clamp(0.03f + ripple + 0.65f * bassPeak + 0.45f * midPeak + 0.32f * highPeak, 0.0f, 1.0f);
    }

    audio.waveform.resize(256);
    for (std::size_t i = 0; i < audio.waveform.size(); ++i) {
        const float x = static_cast<float>(i) / static_cast<float>(audio.waveform.size());
        audio.waveform[i] = std::clamp(
            0.58f * std::sin(2.0f * Pi * (3.0f * x + seconds * 0.8f)) +
            0.24f * std::sin(2.0f * Pi * (7.0f * x - seconds * 0.43f)), -1.0f, 1.0f);
    }
    return audio;
}

void flipRows(std::vector<std::uint8_t> &rgb, const int width, const int height) {
    const auto rowBytes = static_cast<std::size_t>(width) * 3;
    std::vector<std::uint8_t> temp(rowBytes);
    for (int y = 0; y < height / 2; ++y) {
        auto *top = rgb.data() + static_cast<std::size_t>(y) * rowBytes;
        auto *bottom = rgb.data() + static_cast<std::size_t>(height - 1 - y) * rowBytes;
        std::copy_n(top, rowBytes, temp.data());
        std::copy_n(bottom, rowBytes, top);
        std::copy_n(temp.data(), rowBytes, bottom);
    }
}
}

int main(int argc, char **argv) {
    if (argc < 3 || argc > 8) {
        std::cerr << "Usage: shadertoy_shader_preview <shader.frag> <output.png> [width=256] [height=128] [frames=120] [audio=synthetic|none] [max-temporal-delta]\n";
        return 2;
    }

    const std::filesystem::path shaderPath = argv[1];
    const std::filesystem::path outputPath = argv[2];
    const int width = argc >= 4 ? std::stoi(argv[3]) : 256;
    const int height = argc >= 5 ? std::stoi(argv[4]) : 128;
    const int frames = argc >= 6 ? std::stoi(argv[5]) : 120;
    const std::string audioMode = argc >= 7 ? argv[6] : "synthetic";
    const double maxTemporalDeltaLimit = argc >= 8 ? std::stod(argv[7]) : -1.0;
    if (audioMode != "synthetic" && audioMode != "none") {
        std::cerr << "audio must be 'synthetic' or 'none'\n";
        return 2;
    }
    if (width <= 0 || height <= 0 || frames <= 0) {
        std::cerr << "width, height and frames must be positive\n";
        return 2;
    }

    glfwSetErrorCallback([](int, const char *message) { std::cerr << "GLFW: " << message << '\n'; });
    if (!glfwInit()) return 3;
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow *window = glfwCreateWindow(width, height, "shadertoy_shader_preview", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 3;
    }
    glfwMakeContextCurrent(window);
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return 4;
    }
    glGetError(); // GLEW may leave GL_INVALID_ENUM on core contexts.

    try {
        auto &editor = ShaderToy::PipelineEditor::get();
        auto load = editor.loadImageShader(shaderPath.stem().string(), readFile(shaderPath), 0);
        if (!load) throw load.error();

        ShaderToy::ShaderToyContext context;
        auto build = editor.build(context);
        if (!build) throw build.error();

        constexpr float Fps = 60.0f;
        std::vector<std::uint8_t> rgb;
        std::vector<std::uint8_t> previousRgb;
        double temporalDeltaSum = 0.0;
        double maxTemporalDelta = 0.0;
        int temporalComparisons = 0;
        for (int frame = 0; frame < frames; ++frame) {
            context.setAudioInput(audioMode == "synthetic" ? syntheticAudio(frame, Fps) : ShaderToy::AudioInput{});
            context.tickFixed(1.0f / Fps, Fps);
            rgb = context.renderToBuffer(ImVec2(static_cast<float>(width), static_cast<float>(height)));
            if (previousRgb.size() == rgb.size() && !rgb.empty()) {
                double delta = 0.0;
                for (std::size_t i = 0; i < rgb.size(); ++i)
                    delta += std::abs(static_cast<int>(rgb[i]) - static_cast<int>(previousRgb[i]));
                delta /= static_cast<double>(rgb.size()) * 255.0;
                temporalDeltaSum += delta;
                maxTemporalDelta = std::max(maxTemporalDelta, delta);
                ++temporalComparisons;
            }
            previousRgb = rgb;
        }
        if (rgb.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3)
            throw std::runtime_error("Renderer returned an unexpected buffer size");

        flipRows(rgb, width, height);
        if (outputPath.has_parent_path())
            std::filesystem::create_directories(outputPath.parent_path());
        if (!stbi_write_png(outputPath.string().c_str(), width, height, 3, rgb.data(), width * 3))
            throw std::runtime_error("Could not write preview image: " + outputPath.string());

        const auto [minIt, maxIt] = std::minmax_element(rgb.begin(), rgb.end());
        double sum = 0.0;
        for (const auto byte : rgb) sum += byte;
        const double temporalMean = temporalComparisons > 0 ? temporalDeltaSum / temporalComparisons : 0.0;
        std::cout << "Rendered " << width << 'x' << height << " after " << frames << ' ' << audioMode << " frames; "
                  << "range=" << static_cast<int>(*minIt) << ".." << static_cast<int>(*maxIt)
                  << ", mean=" << (sum / static_cast<double>(rgb.size()))
                  << ", temporal_mean=" << temporalMean << ", temporal_max=" << maxTemporalDelta << '\n';
        if (maxTemporalDeltaLimit >= 0.0 && maxTemporalDelta > maxTemporalDeltaLimit)
            throw std::runtime_error("Temporal delta exceeded limit: " + std::to_string(maxTemporalDelta) +
                                     " > " + std::to_string(maxTemporalDeltaLimit));
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        glfwDestroyWindow(window);
        glfwTerminate();
        return 5;
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
