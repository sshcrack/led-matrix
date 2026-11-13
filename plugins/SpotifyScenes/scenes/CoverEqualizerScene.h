#pragma once

#include <vector>

#include "SpotifyArtworkScene.h"

namespace Scenes {
/// "spotify_equalizer": the album cover fills the panel, dimmed, and an
/// equalizer of the playing music reveals it at full brightness band by
/// band. Falling peak caps take the cover's accent colours. Without live
/// analysis the bands follow a beat simulated from the track BPM.
class CoverEqualizerScene final : public SpotifyArtworkScene {
    PropertyPointer<int> bands = MAKE_PROPERTY_MINMAX("bands", int, 16, 6, 32);
    PropertyPointer<float> dim_level = MAKE_PROPERTY_MINMAX("dim_level", float, 0.30f, 0.0f, 0.7f);
    PropertyPointer<bool> mirror = MAKE_PROPERTY("mirror", bool, false);
    PropertyPointer<bool> peak_caps = MAKE_PROPERTY("peak_caps", bool, true);
    PropertyPointer<int> band_gap = MAKE_PROPERTY_MINMAX("band_gap", int, 1, 0, 3);
    PropertyPointer<float> responsiveness = MAKE_PROPERTY_MINMAX("responsiveness", float, 1.0f, 0.3f, 2.0f);

    std::vector<float> level_bands_;  // smoothed band height 0..1
    std::vector<float> band_peak_;    // auto-gain per band
    std::vector<float> cap_;          // falling peak cap 0..1
    std::vector<float> cap_hold_;     // seconds a cap stays before falling

protected:
    [[nodiscard]] ArtworkOptions artwork_options() const override;
    void draw(rgb_matrix::FrameCanvas* canvas, float dt) override;

public:
    [[nodiscard]] string get_name() const override { return "spotify_equalizer"; }
    [[nodiscard]] SceneDescriptor get_descriptor() const override;
    void register_properties() override;
    tmillis_t get_default_duration() override { return 25000; }
};

class CoverEqualizerSceneWrapper : public Plugins::SceneWrapper {
    std::unique_ptr<Scenes::Scene> create() override;
};
}  // namespace Scenes
