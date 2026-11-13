#pragma once

#include <vector>

#include "SpotifyArtworkScene.h"

namespace Scenes {
/// "spotify_vinyl": the current track as a spinning record. Either a picture
/// disc printed with the album art, or black vinyl with the art as its
/// centre label. Grooves catch a fixed light (the sheen pulses on beats) and
/// the tonearm creeps inwards with the track progress.
class VinylScene final : public SpotifyArtworkScene {
    PropertyPointer<int> label_percent = MAKE_PROPERTY_MINMAX("label_percent", int, 100, 30, 100);
    PropertyPointer<float> rpm = MAKE_PROPERTY_MINMAX("rpm", float, 6.0f, 0.0f, 33.3f);
    PropertyPointer<float> sheen_strength = MAKE_PROPERTY_MINMAX("sheen_strength", float, 0.5f, 0.0f, 1.0f);
    PropertyPointer<int> background_brightness = MAKE_PROPERTY_MINMAX("background_brightness", int, 30, 0, 80);
    PropertyPointer<bool> show_tonearm = MAKE_PROPERTY("show_tonearm", bool, true);

    /// Static per-pixel geometry of the disc, rebuilt when the size changes.
    struct DiscPixel {
        int index;
        float dx, dy;  // offset from the centre in disc radii
        float r;       // distance from the centre in disc radii
        float groove;  // 0..1 groove relief, fixed (grooves are concentric)
        float sheen;   // 0..1 reflection of the fixed light
        float edge;    // anti-aliased coverage at the rim
    };
    std::vector<DiscPixel> disc_;
    int disc_w_ = 0, disc_h_ = 0;
    float radius_ = 0, cx_ = 0, cy_ = 0;
    float angle_ = 0;
    std::vector<uint8_t> inside_;  // 1 where the disc fully covers the pixel

    void build_geometry(int W, int H);
    void draw_tonearm(int W, int H, float needle_r);

protected:
    [[nodiscard]] ArtworkOptions artwork_options() const override;
    void draw(rgb_matrix::FrameCanvas* canvas, float dt) override;

public:
    [[nodiscard]] string get_name() const override { return "spotify_vinyl"; }
    [[nodiscard]] SceneDescriptor get_descriptor() const override;
    void register_properties() override;
    tmillis_t get_default_duration() override { return 25000; }
};

class VinylSceneWrapper : public Plugins::SceneWrapper {
    std::unique_ptr<Scenes::Scene> create() override;
};
}  // namespace Scenes
