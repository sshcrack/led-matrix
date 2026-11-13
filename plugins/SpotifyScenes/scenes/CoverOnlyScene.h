#pragma once

#include <vector>

#include "SpotifyArtworkScene.h"

namespace Scenes {
    /// "spotify": the current album cover, staged in a living backdrop that is
    /// derived from the artwork itself. The backdrop is a slowly flowing, blurred
    /// copy of the cover; a spectrum aura, beat sparkles and the progress bar all
    /// take their colours from the cover palette. With live music analysis the
    /// cover reacts to real beats, otherwise to a beat simulated from the BPM.
class CoverOnlyScene final : public SpotifyArtworkScene {
    PropertyPointer<float> cover_border_glow_intensity = MAKE_PROPERTY_MINMAX("cover_border_glow_intensity", float, 0.35f, 0.0f, 1.0f);
    PropertyPointer<int> cover_size_percent = MAKE_PROPERTY_MINMAX("cover_size_percent", int, 66, 45, 94);
    PropertyPointer<int> background_blur = MAKE_PROPERTY_MINMAX("background_blur", int, 8, 0, 20);
    PropertyPointer<int> background_brightness = MAKE_PROPERTY_MINMAX("background_brightness", int, 42, 10, 85);
    PropertyPointer<float> background_motion = MAKE_PROPERTY_MINMAX("background_motion", float, 1.0f, 0.0f, 2.5f);
    PropertyPointer<int> progress_bar_height = MAKE_PROPERTY_MINMAX("progress_bar_height", int, 2, 1, 8);
    PropertyPointer<float> beat_pulse_strength = MAKE_PROPERTY_MINMAX("beat_pulse_strength", float, 0.25f, 0.0f, 0.6f);
    PropertyPointer<bool> show_progress = MAKE_PROPERTY("show_progress", bool, true);
    PropertyPointer<bool> spectrum_aura = MAKE_PROPERTY("spectrum_aura", bool, true);
    PropertyPointer<bool> beat_sparkles = MAKE_PROPERTY("beat_sparkles", bool, true);

    std::vector<float> aura_;       // smoothed per-segment aura length
    std::vector<float> aura_peak_;  // per-segment auto-gain
    struct Sparkle {
        float x, y, vx, vy, life, max_life;
        int color;
    };
    std::vector<Sparkle> sparkles_;
    std::vector<Rgb> grid_;  // backdrop samples (coarse on the Pi)

protected:
    [[nodiscard]] ArtworkOptions artwork_options() const override;
    void draw(rgb_matrix::FrameCanvas* canvas, float dt) override;
    void on_track_changed() override;

public:
    void initialize(int width, int height) override;
    [[nodiscard]] string get_name() const override;
    [[nodiscard]] SceneDescriptor get_descriptor() const override;
    void register_properties() override;
    tmillis_t get_default_duration() override { return 25000; }
};

    class CoverOnlySceneWrapper : public Plugins::SceneWrapper {
        std::unique_ptr<Scenes::Scene> create() override;
    };
}
