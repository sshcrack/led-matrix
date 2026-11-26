#pragma once

#include "HobbyScene.h"

namespace Hobbies {

struct Planet;  // procedural planet textures, generated once per process

// The view through a space station window: Earth turns slowly below with
// continents, oceans and drifting cloud, the day/night terminator follows the
// local time of day, cities glow on the night side, the thin atmosphere lights
// the limb, and now and then a satellite passes, an aurora ripples along the
// night-side limb or lightning flickers in the clouds.
class SpaceStationWindowScene final : public HobbySceneBase {
    // A screen pixel that shows the planet, with its cached sphere geometry.
    struct EarthPixel {
        int index;         // pixel index
        float nx, ny, nz;  // surface normal in view space (z toward the viewer)
        float tx, ty;      // texel position at rotation 0
        float cover;       // anti-aliased coverage of the planet's disc
    };
    // A pixel near the limb: atmosphere glow, airglow and aurora.
    struct LimbPixel {
        int index;
        float nx, ny;  // outward limb direction
        float d;       // signed distance outside the limb, in design pixels
        float angle;   // position along the limb, radians
    };
    struct Star {
        int index;
        float brightness, phase;
    };
    struct Flash {
        float x, y, life, size;
    };

    std::shared_ptr<const Planet> planet_;
    std::vector<EarthPixel> earth_;
    std::vector<LimbPixel> limb_;
    std::vector<Star> stars_;
    std::vector<float> glass_;     // window coverage per pixel (1 = see-through)
    std::vector<float> frame_;     // frame and cabin wall colour per pixel
    std::vector<float> rimLight_;  // how much earthshine falls on each frame pixel
    std::vector<Point> leds_;
    float cx_ = 0, cy_ = 0, radius_ = 1;  // planet disc on screen, pixels
    float view_[3]{};                     // planet normal at the centre of the view
    float spin_ = 0.0f, cloudDrift_ = 0.0f;
    float scale_ = 1.0f;   // pixels per design pixel
    float sunlit_ = 0.5f;  // share of the visible planet in daylight (last frame)

    // Satellite crossing the sky above the limb.
    float satX_ = 0, satY_ = 0, satVx_ = 0, satVy_ = 0, satLife_ = 0, satWait_ = 8.0f, satFlare_ = 0;
    // Aurora along the night-side limb.
    float aurora_ = 0.0f, auroraTarget_ = 0.0f, auroraWait_ = 6.0f, auroraTime_ = 0.0f, auroraCenter_ = 0.0f, auroraWidth_ = 0.6f;
    // Lightning flickers in night-side clouds.
    std::vector<Flash> flashes_;
    float flashWait_ = 3.0f;

    void build_geometry();
    void draw_stars();
    void draw_earth(const float sun[3]);
    void draw_limb(const float sun[3], float dt);
    void draw_satellite(float dt, float sunlit);
    void draw_lightning(float dt, const float sun[3]);
    void draw_frame(float earthshine);

public:
    SpaceStationWindowScene();
    void initialize(int width, int height) override;
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    [[nodiscard]] std::string get_name() const override { return "space_station_window"; }
    [[nodiscard]] std::string get_category() const override { return "Ambient"; }
    [[nodiscard]] Scenes::SceneDescriptor get_descriptor() const override;
};

}  // namespace Hobbies
