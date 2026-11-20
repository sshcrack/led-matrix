#pragma once

#include "shared/matrix/Scene.h"
#include "shared/matrix/utils/FrameTimer.h"
#include "shared/matrix/wrappers.h"

namespace Scenes {
/// Test pattern for setting up the panel hardware (web UI, #233). Shows
/// whether the layout is right:
/// - red top-left, green top-right, blue bottom-left and yellow bottom-right
///   corners, and a white arrow at the top pointing up: orientation and mirroring
/// - a white frame around the whole canvas: size (chain/parallel/rows/cols)
/// - a magenta diagonal and a grey grid every 32 pixels: scrambled rows
///   (multiplexing, row address type)
/// - a cyan line sweeping left to right: the order of chained panels
/// - a grey ramp at the bottom: colour depth (PWM bits)
class PanelTestScene : public Scene {
    FrameTimer timer_;

public:
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    [[nodiscard]] std::string get_name() const override { return "panel_test"; }
    [[nodiscard]] std::string get_category() const override { return "Setup"; }
    void register_properties() override {}
    tmillis_t get_default_duration() override { return 30000; }
    int get_default_weight() override { return 1; }
};

class PanelTestSceneWrapper : public Plugins::SceneWrapper {
public:
    std::unique_ptr<Scenes::Scene> create() override { return std::make_unique<PanelTestScene>(); }
};
}  // namespace Scenes
