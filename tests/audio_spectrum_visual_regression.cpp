#include <emulator.h>
#include <led-matrix.h>
#include <matrix-factory.h>
#include <shared/matrix/audio_state.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

#include "plugins/AudioVisualizer/matrix/scenes/AudioReactiveScenes.h"
#include "plugins/AudioVisualizer/matrix/scenes/AudioSpectrumScene.h"

namespace {
constexpr int Width = 128;
constexpr int Height = 128;
constexpr float Pi = 3.14159265358979323846f;

std::vector<std::uint8_t> capture(rgb_matrix::FrameCanvas* canvas)
{
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(Width) * Height * 3);
    for (int y = 0; y < Height; ++y) {
        for (int x = 0; x < Width; ++x) {
            std::uint8_t r = 0, g = 0, b = 0;
            canvas->GetPixel(x, y, &r, &g, &b);
            const auto index = static_cast<std::size_t>((y * Width + x) * 3);
            pixels[index] = r;
            pixels[index + 1] = g;
            pixels[index + 2] = b;
        }
    }
    return pixels;
}

float changed_pixel_fraction(const std::vector<std::uint8_t>& a, const std::vector<std::uint8_t>& b)
{
    int changed = 0;
    for (int pixel = 0; pixel < Width * Height; ++pixel) {
        const auto i = static_cast<std::size_t>(pixel) * 3;
        const int delta = std::abs(static_cast<int>(a[i]) - static_cast<int>(b[i])) +
                          std::abs(static_cast<int>(a[i + 1]) - static_cast<int>(b[i + 1])) +
                          std::abs(static_cast<int>(a[i + 2]) - static_cast<int>(b[i + 2]));
        if (delta > 24)
            ++changed;
    }
    return static_cast<float>(changed) / static_cast<float>(Width * Height);
}

int pixel_luma(rgb_matrix::FrameCanvas* canvas, int x, int y)
{
    std::uint8_t r = 0, g = 0, b = 0;
    canvas->GetPixel(x, y, &r, &g, &b);
    return (static_cast<int>(r) + static_cast<int>(g) + static_cast<int>(b)) / 3;
}

float mean_luma(const std::vector<std::uint8_t>& pixels)
{
    double total = 0.0;
    for (std::size_t i = 0; i + 2 < pixels.size(); i += 3)
        total += (static_cast<double>(pixels[i]) + pixels[i + 1] + pixels[i + 2]) / (3.0 * 255.0);
    return static_cast<float>(total / static_cast<double>(Width * Height));
}

AudioProtocol::Frame waveform_frame(std::uint32_t sequence, float phase)
{
    AudioProtocol::Frame frame;
    frame.sequence = sequence;
    frame.set(AudioProtocol::Feature::LoudnessFast, 0.65f);
    frame.set(AudioProtocol::Feature::BeatConfidence, 0.0f);
    frame.spectrum.assign(64, 0.2f);
    frame.waveform.resize(64);
    for (std::size_t i = 0; i < frame.waveform.size(); ++i) {
        const float x = static_cast<float>(i) / static_cast<float>(frame.waveform.size());
        frame.waveform[i] = std::sin((x * 4.0f + phase) * 2.0f * Pi) * 0.72f + std::sin((x * 9.0f + phase * 1.7f) * 2.0f * Pi) * 0.18f;
    }
    return frame;
}
}  // namespace

int main()
{
    rgb_matrix::RGBMatrix::Options led;
    led.rows = Height;
    led.cols = Width;
    led.chain_length = 1;
    led.parallel = 1;
    rgb_matrix::EmulatorOptions emulator;
    emulator.headless = true;
    emulator.refresh_rate_hz = 60;
    std::unique_ptr<rgb_matrix::EmulatorMatrix> matrix(rgb_matrix::EmulatorMatrix::Create(led, emulator));
    if (!matrix) {
        std::cerr << "failed to create emulator matrix\n";
        return 2;
    }
    auto* canvas = matrix->CreateFrameCanvas();

    Scenes::AudioSpectrumScene scene;
    scene.register_properties();
    scene.load_properties({{"display_mode", "WAVEFORM"},
                           {"waveform_style", "TRACE"},
                           {"waveform_gain", 0.8f},
                           {"stereo_motion", false},
                           {"beat_pulse", false},
                           {"rainbow_colors", false},
                           {"base_color", 0xFFFFFF},
                           {"accent_color", 0xFFFFFF}});
    scene.initialize(Width, Height);

    std::vector<std::uint8_t> previous;
    float accumulated_change = 0.0f;
    int comparisons = 0;
    for (std::uint32_t frame_index = 0; frame_index < 18; ++frame_index) {
        // Deliberately jump the capture phase as real independent audio blocks do.
        AudioState::update(waveform_frame(frame_index + 1, (frame_index % 2 == 0) ? 0.03f : 0.41f));
        scene.render_frame(canvas, 1.0 / 60.0, true);
        auto pixels = capture(canvas);
        if (!previous.empty() && frame_index >= 4) {
            accumulated_change += changed_pixel_fraction(previous, pixels);
            ++comparisons;
        }
        previous = std::move(pixels);
    }

    const float average_change = comparisons > 0 ? accumulated_change / comparisons : 1.0f;
    std::cout << "waveform changed-pixel fraction=" << average_change << '\n';
    bool failed = false;
    if (average_change > 0.040f) {
        std::cerr << "waveform redraws too abruptly between independent audio blocks\n";
        failed = true;
    }

    Scenes::AudioSpectrumScene layeredScene;
    layeredScene.register_properties();
    layeredScene.load_properties({{"display_mode", "NORMAL"},
                                  {"bar_width", 2},
                                  {"gap_width", 1},
                                  {"mirror_display", true},
                                  {"rainbow_colors", false},
                                  {"base_color", 0xFFFFFF},
                                  {"accent_color", 0xFFFFFF},
                                  {"percussion_color", false},
                                  {"falling_dots", false},
                                  {"beat_pulse", false},
                                  {"smoothing", 0.0f},
                                  {"sensitivity", 1.0f}});
    layeredScene.initialize(Width, Height);

    AudioProtocol::Frame spectrumFrame;
    spectrumFrame.sequence = 100;
    spectrumFrame.spectrum.assign(42, 0.0f);
    spectrumFrame.spectrum[2] = 0.25f;
    spectrumFrame.spectrum[40] = 0.75f;
    AudioState::update(spectrumFrame);
    layeredScene.render_frame(canvas, 1.0 / 60.0, true);

    // At x=6, display bar 2 (25% tall) overlaps the mirrored display bar 40
    // (75% tall). The foreground top row should read clearly brighter than the
    // rear-only row immediately above it even when both configured colors are white.
    const int foregroundLuma = pixel_luma(canvas, 6, 96);
    const int rearLuma = pixel_luma(canvas, 6, 95);
    std::cout << "white mirror foreground/rear luma=" << foregroundLuma << '/' << rearLuma << '\n';
    if (foregroundLuma < 180 || rearLuma <= 0 || foregroundLuma < static_cast<int>(rearLuma * 1.6f)) {
        std::cerr << "white mirrored spectrum layers are not visually separated\n";
        failed = true;
    }

    // Stress the radial/tunnel path with exactly the failure mode seen in
    // production: beat phase jumps wildly while a single beat/drop packet flag
    // remains the latest packet for many display frames. Geometry should remain
    // continuous and each event counter must be consumed exactly once.
    Scenes::AudioPulseTunnelScene tunnel;
    tunnel.register_properties();
    tunnel.load_properties({{"speed", 1.65f},
                            {"ring_count", 17},
                            {"twist", 1.55f},
                            {"sensitivity", 1.18f},
                            {"tempo_lock", true},
                            {"spectrum_ribs", true},
                            {"rainbow", true}});
    tunnel.initialize(Width, Height);

    previous.clear();
    accumulated_change = 0.0f;
    comparisons = 0;
    float lateFlagLuma = 0.0f;
    float postEventPeakLuma = 0.0f;
    for (std::uint32_t frame_index = 0; frame_index < 72; ++frame_index) {
        AudioProtocol::Frame f;
        f.sequence = 1000 + frame_index;
        f.spectrum.assign(64, 0.34f);
        f.set(AudioProtocol::Feature::LoudnessFast, 0.67f);
        f.set(AudioProtocol::Feature::Bass, 0.62f);
        f.set(AudioProtocol::Feature::Kick, frame_index >= 12 && frame_index < 18 ? 0.80f : 0.16f);
        f.set(AudioProtocol::Feature::Snare, 0.24f);
        f.set(AudioProtocol::Feature::Hihat, 0.30f);
        f.set(AudioProtocol::Feature::StereoWidth, 0.46f);
        f.set(AudioProtocol::Feature::StereoBalance, (frame_index & 1U) ? 0.18f : -0.18f);
        f.set(AudioProtocol::Feature::Bpm, frame_index < 36 ? 132.0f : 96.0f);
        f.set(AudioProtocol::Feature::BeatConfidence, 0.92f);
        f.set(AudioProtocol::Feature::TempoStability, 0.90f);
        // Deliberately discontinuous tracker phase: spatial ring position must
        // not be tied to it.
        f.set(AudioProtocol::Feature::BeatPhase, (frame_index & 1U) ? 0.94f : 0.06f);
        if (frame_index >= 12) {
            f.beat_counter = 1;
            f.drop_counter = 1;
            if (frame_index < 42)
                f.flags = AudioProtocol::BeatEvent | AudioProtocol::DropEvent;
        }
        AudioState::update(f);
        tunnel.render_frame(canvas, 1.0 / 60.0, true);
        auto pixels = capture(canvas);
        const float luma = mean_luma(pixels);
        if (frame_index >= 12 && frame_index < 26)
            postEventPeakLuma = std::max(postEventPeakLuma, luma);
        if (frame_index == 40)
            lateFlagLuma = luma;
        if (!previous.empty() && frame_index >= 8) {
            accumulated_change += changed_pixel_fraction(previous, pixels);
            ++comparisons;
        }
        previous = std::move(pixels);
    }
    const float tunnelChange = comparisons > 0 ? accumulated_change / comparisons : 1.0f;
    std::cout << "tunnel stress changed-pixel fraction=" << tunnelChange << " peak/late luma=" << postEventPeakLuma << '/' << lateFlagLuma
              << '\n';
    if (tunnelChange > 0.34f) {
        std::cerr << "pulse tunnel geometry jumps too aggressively during phase/tempo changes\n";
        failed = true;
    }
    if (postEventPeakLuma > 0.0f && lateFlagLuma > postEventPeakLuma * 1.18f) {
        std::cerr << "persistent event flags keep retriggering tunnel brightness\n";
        failed = true;
    }

    return failed ? 1 : 0;
}
