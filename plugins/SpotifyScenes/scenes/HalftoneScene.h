#pragma once

#include <vector>

#include "SpotifyArtworkScene.h"

namespace Scenes {
/// "spotify_halftone": the album cover printed as a grid of dots whose size
/// follows the artwork's brightness. Beats send rings through the dots from
/// the centre and a new track re-prints the grid outwards.
class HalftoneScene final : public SpotifyArtworkScene {
    PropertyPointer<int> dot_size = MAKE_PROPERTY_MINMAX("dot_size", int, 6, 3, 12);
    PropertyPointer<float> ripple_strength = MAKE_PROPERTY_MINMAX("ripple_strength", float, 0.5f, 0.0f, 1.0f);
    PropertyPointer<float> breathing = MAKE_PROPERTY_MINMAX("breathing", float, 0.4f, 0.0f, 1.0f);
    PropertyPointer<float> background_glow = MAKE_PROPERTY_MINMAX("background_glow", float, 0.12f, 0.0f, 0.5f);
    PropertyPointer<bool> square_dots = MAKE_PROPERTY("square_dots", bool, false);

    struct Cell {
        Rgb color;
        float value;  // 0..1 brightness
    };
    std::vector<Cell> cells_, prev_cells_;
    std::vector<float> ripples_;  // ages in seconds

    void sample_cells(const Artwork& art, std::vector<Cell>& out, int W, int H, int cell, int cols, int rows, int x0, int y0) const;

protected:
    [[nodiscard]] ArtworkOptions artwork_options() const override;
    void draw(rgb_matrix::FrameCanvas* canvas, float dt) override;
    void on_track_changed() override { ripples_.clear(); }

public:
    void initialize(int width, int height) override
    {
        SpotifyArtworkScene::initialize(width, height);
        set_target_fps(30);  // slow ambient motion; 60 fps would double the Pi cost
    }
    [[nodiscard]] string get_name() const override { return "spotify_halftone"; }
    [[nodiscard]] SceneDescriptor get_descriptor() const override;
    void register_properties() override;
    tmillis_t get_default_duration() override { return 25000; }
    float transition_seconds() const override { return 1.6f; }
};

class HalftoneSceneWrapper : public Plugins::SceneWrapper {
    std::unique_ptr<Scenes::Scene> create() override;
};
}  // namespace Scenes
