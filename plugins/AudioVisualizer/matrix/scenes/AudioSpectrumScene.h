#pragma once

#include <shared/matrix/audio_state.h>
#include <shared/matrix/media_artwork_state.h>
#include "shared/matrix/Scene.h"
#include "shared/matrix/wrappers.h"
#include <deque>

namespace Scenes {
enum class WaveformStyle { TRACE = 0, MIRRORED = 1, FILLED = 2 };

enum class DisplayMode {
    NORMAL = 0,
    CENTER_OUT = 1,
    EDGES_TO_CENTER = 2,
    CIRCLE = 3,
    SPIRAL = 4,
    WAVEFORM = 5,
    SPECTROGRAM = 6
};

class AudioSpectrumScene final : public Scene {
    std::vector<float> smoothed_;
    std::vector<float> peaks_;
    std::deque<std::vector<float>> history_;
    std::vector<float> waveformSmoothed_;
    std::vector<float> waveformTarget_;
    std::vector<float> waveformScratch_;
    uint64_t lastBeat_ = 0;
    float beatPulse_ = 0.0f;
    float rotation_ = 0.0f;

    PropertyPointer<int> barWidth_ = MAKE_PROPERTY_MINMAX("bar_width", int, 2, 1, 10);
    PropertyPointer<int> gapWidth_ = MAKE_PROPERTY_MINMAX("gap_width", int, 1, 0, 5);
    PropertyPointer<bool> mirror_ = MAKE_PROPERTY("mirror_display", bool, true);
    PropertyPointer<float> mirrorLayerBrightness_ = MAKE_PROPERTY_MINMAX("mirror_layer_brightness", float, 0.42f, 0.08f, 1.0f);
    PropertyPointer<bool> rainbow_ = MAKE_PROPERTY("rainbow_colors", bool, true);
    PropertyPointer<bool> musicalColor_ = MAKE_PROPERTY("musical_color", bool, true);
    PropertyPointer<rgb_matrix::Color> baseColor_ = MAKE_PROPERTY("base_color", rgb_matrix::Color, rgb_matrix::Color(0, 255, 160));
    PropertyPointer<rgb_matrix::Color> accentColor_ = MAKE_PROPERTY("accent_color", rgb_matrix::Color, rgb_matrix::Color(255, 70, 190));
    PropertyPointer<bool> percussionColor_ = MAKE_PROPERTY("percussion_color", bool, true);
    PropertyPointer<bool> fallingDots_ = MAKE_PROPERTY("falling_dots", bool, true);
    PropertyPointer<float> dotFallSpeed_ = MAKE_PROPERTY_MINMAX("dot_fall_speed", float, 0.35f, 0.02f, 2.0f);
    PropertyPointer<Plugins::EnumProperty<DisplayMode>> displayMode_ = MAKE_ENUM_PROPERTY("display_mode", DisplayMode, DisplayMode::NORMAL);
    PropertyPointer<float> circleRadius_ = MAKE_PROPERTY_MINMAX("circle_radius", float, 0.72f, 0.25f, 1.0f);
    PropertyPointer<bool> rotate_ = MAKE_PROPERTY("rotate_visualization", bool, true);
    PropertyPointer<float> rotationSpeed_ = MAKE_PROPERTY_MINMAX("rotation_speed", float, 0.6f, 0.0f, 5.0f);
    PropertyPointer<float> sensitivity_ = MAKE_PROPERTY_MINMAX("sensitivity", float, 1.0f, 0.25f, 4.0f);
    PropertyPointer<float> smoothing_ = MAKE_PROPERTY_MINMAX("smoothing", float, 0.62f, 0.0f, 0.96f);
    PropertyPointer<float> releaseSpeed_ = MAKE_PROPERTY_MINMAX("release_speed", float, 3.8f, 0.5f, 14.0f);
    PropertyPointer<bool> beatPulseEnabled_ = MAKE_PROPERTY("beat_pulse", bool, true);
    PropertyPointer<bool> showWaveform_ = MAKE_PROPERTY("waveform_overlay", bool, false);
    PropertyPointer<Plugins::EnumProperty<WaveformStyle>> waveformStyle_ = MAKE_ENUM_PROPERTY("waveform_style", WaveformStyle, WaveformStyle::TRACE);
    PropertyPointer<float> waveformGain_ = MAKE_PROPERTY_MINMAX("waveform_gain", float, 1.0f, 0.25f, 3.0f);
    PropertyPointer<bool> waveformStabilization_ = MAKE_PROPERTY("waveform_stabilization", bool, true);
    PropertyPointer<float> waveformSmoothing_ = MAKE_PROPERTY_MINMAX("waveform_smoothing", float, 0.84f, 0.0f, 0.98f);
    PropertyPointer<int> waveformThickness_ = MAKE_PROPERTY_MINMAX("waveform_thickness", int, 1, 1, 3);
    PropertyPointer<bool> stereoMotion_ = MAKE_PROPERTY("stereo_motion", bool, true);
    PropertyPointer<bool> useSpotifyArtwork_ = MAKE_PROPERTY("use_spotify_artwork", bool, false);
    MediaArtworkState::Snapshot artworkSnapshot_{};

    void updateSpectrum(const AudioState::Snapshot &audio, float dt);
    void updateWaveform(const AudioState::Snapshot &audio, float dt);
    void colorFor(float position, float intensity, const AudioState::Snapshot &audio,
                  uint8_t &r, uint8_t &g, uint8_t &b) const;
    void renderBars(rgb_matrix::FrameCanvas *canvas, const AudioState::Snapshot &audio);
    void renderCircle(rgb_matrix::FrameCanvas *canvas, const AudioState::Snapshot &audio, bool spiral);
    void renderWaveform(rgb_matrix::FrameCanvas *canvas, const AudioState::Snapshot &audio);
    void renderSpectrogram(rgb_matrix::FrameCanvas *canvas, const AudioState::Snapshot &audio);

public:
    AudioSpectrumScene() = default;
    bool render(rgb_matrix::FrameCanvas *canvas) override;
    std::string get_name() const override { return "audio_spectrum"; }
    std::string get_category() const override { return "Audio Reactive"; }
    void register_properties() override;
    tmillis_t get_default_duration() override { return 30000; }
    int get_default_weight() override { return 5; }
    bool needs_desktop_app() override { return true; }
    [[nodiscard]] Previews::SceneSpec get_preview_spec() const override {
        return Previews::SceneSpec::with_inputs({Previews::Inputs::Audio});
    }
    SceneCapabilities get_capabilities() const override {
        auto caps = Scene::get_capabilities();
        caps.requires_audio = true;
        caps.supports_audio = true;
        return caps;
    }
    [[nodiscard]] bool supports_virtual_time() const override { return true; }
};

class AudioSpectrumSceneWrapper final : public Plugins::SceneWrapper {
public:
    std::unique_ptr<Scenes::Scene> create() override;
};
}
